#include "AIClient.hpp"
#include "Prompt.hpp"
#include "SpeechBubble.hpp"
#include "Text.hpp"

#include <fmt/ranges.h>

using namespace geode::prelude;

using text::toDisplayable;

namespace {
    bool setting(std::string_view key) {
        return Mod::get()->getSettingValue<bool>(key);
    }

    int64_t intSetting(std::string_view key) {
        return Mod::get()->getSettingValue<int64_t>(key);
    }

    // A creator's or player's direction for how to react, after the event report
    std::string withNote(std::string text, std::string const& note) {
        if (!note.empty()) {
            text += fmt::format(" Direction for this moment: {}", note);
        }
        return text;
    }
}

AIClient& AIClient::get() {
    // Intentionally leaked: the pending request must not be torn down during
    // static destruction, after the async runtime may already be gone.
    static auto instance = new AIClient();
    return *instance;
}

AIClient::AIClient() : m_rng(std::random_device{}()) {}

void AIClient::startLevel(LevelInfo info) {
    m_request.cancel();
    m_generation++;
    m_level = std::move(info);
    m_active = true;
    m_attempt = 1;
    m_memory.clear();
    m_unreportedDeaths.clear();
    m_lastSent = {};
    profile::resetLevel();
    this->notifyUpdate();
}

void AIClient::endLevel() {
    m_request.cancel();
    m_generation++;
    m_active = false;
    this->notifyUpdate();
}

bool AIClient::isActive() const {
    return m_active;
}

bool AIClient::isWaiting() const {
    return m_request.isPending();
}

bool AIClient::isWaitingForChat() const {
    return m_request.isPending() && m_requestIsChat;
}

std::deque<AIClient::Turn> const& AIClient::getMemory() const {
    return m_memory;
}

std::string const& AIClient::getLastChatError() const {
    return m_lastChatError;
}

std::string AIClient::getName() const {
    return profile::current().name;
}

void AIClient::setUpdateCallback(std::function<void()> callback) {
    m_onUpdate = std::move(callback);
}

void AIClient::notifyUpdate() {
    if (m_onUpdate) m_onUpdate();
}

std::string AIClient::statusLine() const {
    if (m_level.platformer) {
        return fmt::format("attempt {}", m_attempt);
    }
    return fmt::format("attempt {}, best {}%", m_attempt, m_level.bestPercent);
}

void AIClient::sendChat(std::string const& message) {
    if (!m_active) return;
    auto clean = toDisplayable(message);
    if (clean.empty()) return;

    this->send(
        fmt::format("[PLAYER] {} ({})", clean, this->statusLine()),
        fmt::format("You: {}", clean),
        true
    );
}

void AIClient::onLevelStart() {
    if (!m_active) return;

    // First level of the session without a working setup: say how to fix it, once.
    // Next frame, because the level isn't in its scene yet while it initialises.
    if (!m_warnedAboutSetup && !providers::loadCurrent().missing().empty()) {
        m_warnedAboutSetup = true;
        geode::queueInMainThread([] {
            SpeechBubble::show(
                profile::current(),
                "I'm not connected yet! Pause and tap the chat button, then the gear, to pick an AI and add your key.",
                true
            );
        });
        return;
    }

    auto rules = profile::current().events;
    if (!setting("auto-reactions") || !rules.start) return;
    if (!this->canSendEvent(Priority::Always)) return;

    std::string text;
    if (m_level.platformer) {
        text = fmt::format(
            "[GAME EVENT] The player just opened the platformer level \"{}\" ({} attempts so far).",
            m_level.name, m_level.totalAttempts
        );
    }
    else if (m_level.bestPercent >= 100) {
        text = fmt::format(
            "[GAME EVENT] The player just opened the level \"{}\", which they have already beaten ({} attempts so far).",
            m_level.name, m_level.totalAttempts
        );
    }
    else {
        text = fmt::format(
            "[GAME EVENT] The player just opened the level \"{}\" (best {}%, {} attempts so far).",
            m_level.name, m_level.bestPercent, m_level.totalAttempts
        );
    }
    this->sendEvent(withNote(std::move(text), rules.onStart), "* Entered the level");
}

void AIClient::onDeath(int percent, int attempt, bool practice) {
    if (!m_active) return;
    m_attempt = attempt;

    bool newBest = !practice && !m_level.platformer && percent > m_level.bestPercent;
    int oldBest = m_level.bestPercent;
    if (newBest) m_level.bestPercent = percent;

    if (!setting("auto-reactions") || (practice && !setting("react-in-practice"))) return;
    auto rules = profile::current().events;

    // With new-best comments off, a new best is just another (close) death
    if (newBest && rules.newBest) {
        if (!this->canSendEvent(Priority::Important)) {
            m_unreportedDeaths.push_back(percent);
            return;
        }
        auto text = oldBest == 0
            ? fmt::format("[GAME EVENT] NEW BEST: the player died at {}% on attempt {}, their first progress on this level.", percent, attempt)
            : fmt::format("[GAME EVENT] NEW BEST: the player died at {}% on attempt {}, beating their old best of {}%.", percent, attempt, oldBest);
        this->sendEvent(withNote(std::move(text), rules.onNewBest), fmt::format("* New best: {}%", percent));
        return;
    }

    bool closeCall = rules.closeCall && !practice && !m_level.platformer &&
        m_level.bestPercent >= 30 && percent >= m_level.bestPercent - rules.closeCallRange;
    if (!closeCall && !rules.death) return;

    if (!this->canSendEvent(closeCall ? Priority::CloseCall : Priority::Normal)) {
        m_unreportedDeaths.push_back(percent);
        if (m_unreportedDeaths.size() > 8) {
            m_unreportedDeaths.erase(m_unreportedDeaths.begin());
        }
        return;
    }

    auto where = m_level.platformer ? std::string() : fmt::format(" at {}%", percent);
    auto text = fmt::format(
        "[GAME EVENT] The player died{} on attempt {}{}.",
        where, attempt, practice ? " (practice mode)" : ""
    );
    if (!practice && !m_level.platformer) {
        text += fmt::format(" Their best is {}%.", m_level.bestPercent);
    }
    if (closeCall) {
        text += " That was really close to their best.";
    }
    if (!m_unreportedDeaths.empty()) {
        if (m_level.platformer) {
            text += fmt::format(" They also died {} more times since your last comment.", m_unreportedDeaths.size());
        }
        else {
            text += fmt::format(" Other deaths since your last comment: {}%.", fmt::join(m_unreportedDeaths, "%, "));
        }
    }
    this->sendEvent(withNote(std::move(text), rules.onDeath), fmt::format("* Died{}", where));
}

void AIClient::onComplete(int attempt, bool practice) {
    if (!m_active) return;
    m_attempt = attempt;

    bool firstWin = !practice && m_level.bestPercent < 100;
    if (!practice) m_level.bestPercent = 100;

    auto rules = profile::current().events;
    if (!setting("auto-reactions") || !rules.complete || (practice && !setting("react-in-practice"))) return;
    if (!this->canSendEvent(Priority::Always)) return;

    std::string text;
    if (practice) {
        text = fmt::format("[GAME EVENT] The player finished a practice run of the level (attempt {}).", attempt);
    }
    else {
        text = fmt::format(
            "[GAME EVENT] The player BEAT THE LEVEL on attempt {}! {}",
            attempt, firstWin ? "This is their first completion." : "They had already beaten it before."
        );
    }
    this->sendEvent(withNote(std::move(text), rules.onComplete), practice ? "* Finished practice run" : "* Beat the level!");
}

bool AIClient::canSendEvent(Priority priority) {
    if (m_request.isPending()) {
        // Never interrupt a chat reply; only big moments replace a pending comment
        if (m_requestIsChat) return false;
        if (priority != Priority::Important && priority != Priority::Always) return false;
    }

    auto elapsed = std::chrono::steady_clock::now() - m_lastSent;
    auto cooldown = std::chrono::seconds(intSetting("event-cooldown"));

    switch (priority) {
        case Priority::Always: return true;
        // Stops a burst of tiny new bests (3%, 5%, 8%...) on a fresh level
        // from spamming, and from tripping the proxy's per-player limit
        case Priority::Important: return elapsed >= std::chrono::seconds(8);
        case Priority::CloseCall: return elapsed >= cooldown / 2;
        case Priority::Normal: {
            if (elapsed < cooldown) return false;
            auto chance = intSetting("death-reaction-chance");
            return std::uniform_int_distribution<int64_t>(0, 99)(m_rng) < chance;
        }
    }
    return false;
}

void AIClient::sendEvent(std::string text, std::string display) {
    m_unreportedDeaths.clear();
    this->send(std::move(text), std::move(display), false);
}

void AIClient::send(std::string text, std::string display, bool isChat) {
    if (isChat) {
        m_lastChatError.clear();
    }

    auto look = profile::current();
    auto connection = providers::loadCurrent();
    if (auto missing = connection.missing(); !missing.empty()) {
        if (isChat) {
            m_lastChatError = fmt::format("Add {} first: tap the gear in this chat.", missing);
            SpeechBubble::show(look, m_lastChatError, true);
            this->notifyUpdate();
        }
        return;
    }

    if (!this->takeDailyMessage()) {
        if (isChat) {
            m_lastChatError = "Daily message limit reached. You can raise it in the mod settings.";
            SpeechBubble::show(look, m_lastChatError, true);
            this->notifyUpdate();
        }
        return;
    }

    ChatPayload payload;
    payload.persona = look.persona;
    payload.system = prompt::system(look.persona, look.length);
    for (auto const& turn : m_memory) {
        payload.history.emplace_back(turn.fromAI, turn.text);
    }
    payload.message = text;
    payload.playerID = this->getPlayerID();
    payload.levelID = m_level.id;

    m_lastSent = std::chrono::steady_clock::now();
    m_requestIsChat = isChat;
    if (isChat) {
        SpeechBubble::showTyping(look);
    }

    Turn userTurn { false, std::move(text), std::move(display) };

    // Spawning cancels whatever was pending (only ever an event comment, see canSendEvent)
    m_request.spawn(
        "AI Rival request",
        providers::sendChat(connection, payload),
        [this, generation = m_generation, connection, maxChars = profile::maxReplyChars(look.length),
            userTurn = std::move(userTurn), isChat](web::WebResponse response) mutable {
            // Player left the level (or entered another) while we were waiting
            if (generation != m_generation || !PlayLayer::get()) return;
            this->onResponse(connection, response, maxChars, std::move(userTurn), isChat);
        }
    );
    this->notifyUpdate();
}

void AIClient::onResponse(Connection const& connection, web::WebResponse const& response, size_t maxChars, Turn userTurn, bool isChat) {
    auto look = profile::current();
    auto result = providers::parseChat(connection, response, maxChars);
    if (result.isErr()) {
        auto error = result.unwrapErr();
        // Never log the request itself: it carries the player's API key
        log::warn("{} request failed (HTTP {}): {}", connection.provider->name, response.code(), error);
        if (isChat) {
            m_lastChatError = error;
            SpeechBubble::show(look, error, true);
        }
        this->notifyUpdate();
        return;
    }

    auto reply = result.unwrap();
    this->remember(std::move(userTurn), reply);
    SpeechBubble::show(look, reply);
    this->notifyUpdate();
}

bool AIClient::takeDailyMessage() {
    auto limit = intSetting("daily-limit");
    if (limit <= 0) return true;

    // UTC day number, so the count resets at the same moment everywhere
    auto today = std::chrono::duration_cast<std::chrono::hours>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count() / 24;

    auto mod = Mod::get();
    if (mod->getSavedValue<int64_t>("usage-day") != today) {
        mod->setSavedValue<int64_t>("usage-day", today);
        mod->setSavedValue<int64_t>("usage-count", 0);
    }
    auto used = mod->getSavedValue<int64_t>("usage-count");
    if (used >= limit) return false;
    mod->setSavedValue<int64_t>("usage-count", used + 1);
    return true;
}

void AIClient::connectionChanged() {
    m_lastChatError.clear();
    this->notifyUpdate();
}

std::string AIClient::missingSetup() const {
    return providers::loadCurrent().missing();
}

void AIClient::remember(Turn userTurn, std::string reply) {
    auto display = fmt::format("{}: {}", this->getName(), reply);
    m_memory.push_back(std::move(userTurn));
    m_memory.push_back({ true, std::move(reply), std::move(display) });

    auto max = static_cast<size_t>(intSetting("memory-size"));
    while (m_memory.size() > max) {
        m_memory.pop_front();
    }
    // The conversation sent to the AI has to start with a user turn
    while (!m_memory.empty() && m_memory.front().fromAI) {
        m_memory.pop_front();
    }
}

std::string const& AIClient::getPlayerID() {
    // Random, anonymous ID so the proxy can rate-limit per player
    if (m_playerID.empty()) {
        m_playerID = Mod::get()->getSavedValue<std::string>("player-id");
        if (m_playerID.size() != 32) {
            std::mt19937_64 gen(std::random_device{}());
            m_playerID = fmt::format("{:016x}{:016x}", gen(), gen());
            Mod::get()->setSavedValue("player-id", m_playerID);
        }
    }
    return m_playerID;
}
