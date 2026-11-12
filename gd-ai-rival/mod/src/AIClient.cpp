#include "AIClient.hpp"
#include "Rage.hpp"
#include "GameInfo.hpp"
#include "Haunt.hpp"
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

    // Grin's state for the AI playing it: its anger bar and what it has done
    std::string grinNote() {
        if (!rage::active()) return "";
        return rage::eventNote() + haunt::takeLog();
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
    m_deathsAt.clear();
    m_sessionDeaths = 0;
    m_sessionStart = std::chrono::steady_clock::now();
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
    auto status = m_level.platformer
        ? fmt::format("attempt {}", m_attempt)
        : fmt::format("attempt {}, best {}%", m_attempt, m_level.bestPercent);

    // Where they paused to chat, so "how do I do this part?" makes sense
    if (auto playLayer = PlayLayer::get()) {
        auto where = gameinfo::situation(playLayer->m_player1, playLayer->m_gameState.m_isDualMode);
        if (m_level.platformer) {
            status += fmt::format(", currently {}", where);
        }
        else {
            status += fmt::format(", paused at {}% {}", playLayer->getCurrentPercentInt(), where);
        }
    }
    return status;
}

void AIClient::sendChat(std::string const& message) {
    if (!m_active) return;
    auto clean = toDisplayable(message);
    if (clean.empty()) return;

    this->send(
        fmt::format("[PLAYER] {} ({}){}", clean, this->statusLine(), grinNote()),
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

    std::string progress;
    if (m_level.platformer) {
        progress = fmt::format("{} attempts so far", m_level.totalAttempts);
    }
    else if (m_level.bestPercent >= 100) {
        progress = fmt::format("they have already beaten it, {} attempts so far", m_level.totalAttempts);
    }
    else if (m_level.totalAttempts == 0) {
        progress = "their first time playing it";
    }
    else {
        progress = fmt::format("best {}%, {} attempts so far", m_level.bestPercent, m_level.totalAttempts);
    }
    auto text = fmt::format("[GAME EVENT] The player just opened a level: {}. ({}.)", m_level.facts, progress);
    text += grinNote();
    this->sendEvent(withNote(std::move(text), rules.onStart), "* Entered the level");
}

void AIClient::onDeath(int percent, int attempt, bool practice, std::string const& situation, bool silent) {
    if (!m_active) return;
    m_attempt = attempt;
    m_sessionDeaths++;
    int deathsHere = m_level.platformer ? 0 : ++m_deathsAt[percent];

    bool newBest = !practice && !m_level.platformer && percent > m_level.bestPercent;
    int oldBest = m_level.bestPercent;
    if (newBest) m_level.bestPercent = percent;

    if (silent || !setting("auto-reactions") || (practice && !setting("react-in-practice"))) return;
    auto rules = profile::current().events;
    auto doing = situation.empty() ? std::string() : fmt::format(" ({})", situation);

    // With new-best comments off, a new best is just another (close) death
    if (newBest && rules.newBest) {
        if (!this->canSendEvent(Priority::Important)) {
            m_unreportedDeaths.push_back(percent);
            return;
        }
        auto text = oldBest == 0
            ? fmt::format("[GAME EVENT] NEW BEST: the player died at {}%{} on attempt {}, their first progress on this level.", percent, doing, attempt)
            : fmt::format("[GAME EVENT] NEW BEST: the player died at {}%{} on attempt {}, beating their old best of {}%.", percent, doing, attempt, oldBest);
        text += grinNote();
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
        "[GAME EVENT] The player died{}{} on attempt {}{}.",
        where, doing, attempt, practice ? " (practice mode)" : ""
    );
    if (!practice && !m_level.platformer) {
        text += fmt::format(" Their best is {}%.", m_level.bestPercent);
    }
    if (closeCall) {
        text += " That was really close to their best.";
    }
    if (deathsHere >= 2) {
        text += fmt::format(" That's death number {} at {}% this session.", deathsHere, percent);
    }
    text += fmt::format(" {} deaths this session so far.", m_sessionDeaths);
    text += grinNote();
    if (auto hotspots = this->deathHotspots(); !hotspots.empty()) {
        text += fmt::format(" Most common death spots this session: {}.", hotspots);
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

void AIClient::onRage(bool fought, bool won) {
    if (!m_active || !setting("auto-reactions")) return;
    if (!this->canSendEvent(Priority::Always)) return;
    std::string text =
        "[GAME EVENT] You struck: in the middle of their attempt the screen went dark, you roared, lunged at the "
        "glass from the inside and smashed the player's screen to pieces.";
    std::string display = "* " + this->getName() + " smashed your screen";
    if (fought && won) {
        text += " Then the player came into your world and fought you, and beat you. You are humiliated and your "
            "anger is gone, for now. React to losing.";
        display += ", and you beat it";
    }
    else if (fought) {
        text += " Then the player came into your world to fight you, and you won. Your anger is still high. Gloat.";
        display += ", and won the fight";
    }
    else {
        text += " Their screen is back now and your anger has dropped a little. Say something about what you just did.";
    }
    text += grinNote();
    this->sendEvent(std::move(text), std::move(display));
}

void AIClient::onFight(std::string const& what) {
    if (!m_active || !setting("auto-reactions")) return;
    if (!this->canSendEvent(Priority::Always)) return;
    this->sendEvent("[GAME EVENT] " + what, "* The fight");
}

void AIClient::onComplete(int attempt, bool practice) {
    if (!m_active) return;
    m_attempt = attempt;

    bool firstWin = !practice && m_level.bestPercent < 100;
    if (!practice) m_level.bestPercent = 100;

    auto rules = profile::current().events;
    if (!setting("auto-reactions") || !rules.complete || (practice && !setting("react-in-practice"))) return;
    if (!this->canSendEvent(Priority::Always)) return;

    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(
        std::chrono::steady_clock::now() - m_sessionStart
    ).count();
    auto effort = fmt::format(" This session took {} deaths and {} minute{}.", m_sessionDeaths, minutes, minutes == 1 ? "" : "s");

    std::string text;
    if (practice) {
        text = fmt::format("[GAME EVENT] The player finished a practice run of the level (attempt {}).{}", attempt, effort);
    }
    else {
        text = fmt::format(
            "[GAME EVENT] The player BEAT THE LEVEL on attempt {}! {}{}",
            attempt, firstWin ? "This is their first completion." : "They had already beaten it before.", effort
        );
    }
    this->sendEvent(withNote(std::move(text), rules.onComplete), practice ? "* Finished practice run" : "* Beat the level!");
}

std::string AIClient::deathHotspots() const {
    // Spots with at least two deaths, worst first, at most three
    std::vector<std::pair<int, int>> spots;
    for (auto const& [percent, count] : m_deathsAt) {
        if (count >= 2) spots.emplace_back(count, percent);
    }
    std::sort(spots.begin(), spots.end(), std::greater<>());
    if (spots.size() > 3) spots.resize(3);

    std::vector<std::string> parts;
    for (auto const& [count, percent] : spots) {
        parts.push_back(fmt::format("{}% ({}x)", percent, count));
    }
    return fmt::format("{}", fmt::join(parts, ", "));
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
            // Grin runs the game through its replies, so it gets a word on most deaths
            if (rage::active()) return elapsed >= std::min(cooldown / 2, std::chrono::seconds(6));
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
    payload.powers = look.character == "grin" ? haunt::powers() : "";
    payload.system = prompt::system(look.persona, look.length, m_level.about, payload.powers);
    for (auto const& turn : m_memory) {
        payload.history.emplace_back(turn.fromAI, turn.text);
    }
    payload.message = text;
    payload.levelAbout = m_level.about;
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
    std::string commands;
    auto result = providers::parseChat(connection, response, maxChars, &commands);
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
    bool grin = look.character == "grin";
    // Its commands stay in its memory, so it knows what it has done
    this->remember(std::move(userTurn), reply, grin && !commands.empty() ? fmt::format(" [DO: {}]", commands) : "");
    if (reply != "...") SpeechBubble::show(look, reply);
    if (grin && !commands.empty()) haunt::run(commands);
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

void AIClient::remember(Turn userTurn, std::string reply, std::string const& commands) {
    auto display = fmt::format("{}: {}", this->getName(), reply);
    m_memory.push_back(std::move(userTurn));
    m_memory.push_back({ true, reply + commands, std::move(display) });

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
