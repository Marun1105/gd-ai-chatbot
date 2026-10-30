#include "AIClient.hpp"
#include "SpeechBubble.hpp"

#include <fmt/ranges.h>

using namespace geode::prelude;

namespace {
    // GD's bitmap fonts only have printable ASCII, so anything else would
    // render as garbage. The proxy already maps smart quotes etc. to ASCII.
    std::string toDisplayable(std::string_view text) {
        std::string out;
        out.reserve(text.size());
        for (char ch : text) {
            auto c = static_cast<unsigned char>(ch);
            if (c == '\n' || c == '\r' || c == '\t') {
                if (!out.empty() && out.back() != ' ') out += ' ';
            }
            else if (c >= 0x20 && c < 0x7f) {
                out += ch;
            }
        }
        while (!out.empty() && out.back() == ' ') out.pop_back();
        return out;
    }

    bool setting(std::string_view key) {
        return Mod::get()->getSettingValue<bool>(key);
    }

    int64_t intSetting(std::string_view key) {
        return Mod::get()->getSettingValue<int64_t>(key);
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
    auto name = toDisplayable(Mod::get()->getSettingValue<std::string>("ai-name"));
    return name.empty() ? "Rival" : name;
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
    if (!m_active || !setting("auto-reactions") || !setting("greet-on-start")) return;
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
    this->sendEvent(std::move(text), "* Entered the level");
}

void AIClient::onDeath(int percent, int attempt, bool practice) {
    if (!m_active) return;
    m_attempt = attempt;

    bool newBest = !practice && !m_level.platformer && percent > m_level.bestPercent;
    int oldBest = m_level.bestPercent;
    if (newBest) m_level.bestPercent = percent;

    if (!setting("auto-reactions") || (practice && !setting("react-in-practice"))) return;

    if (newBest) {
        if (!this->canSendEvent(Priority::Important)) {
            m_unreportedDeaths.push_back(percent);
            return;
        }
        auto text = oldBest == 0
            ? fmt::format("[GAME EVENT] NEW BEST: the player died at {}% on attempt {}, their first progress on this level.", percent, attempt)
            : fmt::format("[GAME EVENT] NEW BEST: the player died at {}% on attempt {}, beating their old best of {}%.", percent, attempt, oldBest);
        this->sendEvent(std::move(text), fmt::format("* New best: {}%", percent));
        return;
    }

    bool closeCall = !practice && !m_level.platformer &&
        m_level.bestPercent >= 30 && percent >= m_level.bestPercent - 3;

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
    this->sendEvent(std::move(text), fmt::format("* Died{}", where));
}

void AIClient::onComplete(int attempt, bool practice) {
    if (!m_active) return;
    m_attempt = attempt;

    bool firstWin = !practice && m_level.bestPercent < 100;
    if (!practice) m_level.bestPercent = 100;

    if (!setting("auto-reactions") || (practice && !setting("react-in-practice"))) return;
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
    this->sendEvent(std::move(text), practice ? "* Finished practice run" : "* Beat the level!");
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
        case Priority::Important: return elapsed >= std::chrono::seconds(4);
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

    auto url = std::string(utils::string::trim(Mod::get()->getSettingValue<std::string>("proxy-url")));
    if (url.empty()) {
        if (isChat) {
            m_lastChatError = "No proxy URL set. Add it in the AI Rival mod settings.";
            SpeechBubble::show(this->getName(), m_lastChatError, true);
            this->notifyUpdate();
        }
        return;
    }

    auto history = matjson::Value::array();
    for (auto const& turn : m_memory) {
        history.push(matjson::makeObject({
            { "role", turn.fromAI ? "assistant" : "user" },
            { "content", turn.text },
        }));
    }

    auto body = matjson::makeObject({
        { "player_id", this->getPlayerID() },
        { "level_id", m_level.id },
        { "persona", Mod::get()->getSettingValue<std::string>("personality") },
        { "history", std::move(history) },
        { "message", text },
    });

    m_lastSent = std::chrono::steady_clock::now();
    m_requestIsChat = isChat;
    if (isChat) {
        SpeechBubble::showTyping(this->getName());
    }

    Turn userTurn { false, std::move(text), std::move(display) };

    // Spawning cancels whatever was pending (only ever an event comment, see canSendEvent)
    m_request.spawn(
        "AI Rival request",
        web::WebRequest()
            .timeout(std::chrono::seconds(30))
            .bodyJSON(body)
            .post(std::move(url)),
        [this, generation = m_generation, userTurn = std::move(userTurn), isChat](web::WebResponse response) mutable {
            // Player left the level (or entered another) while we were waiting
            if (generation != m_generation || !PlayLayer::get()) return;
            this->onResponse(response, std::move(userTurn), isChat);
        }
    );
    this->notifyUpdate();
}

void AIClient::onResponse(web::WebResponse const& response, Turn userTurn, bool isChat) {
    auto json = response.json();

    if (!response.ok()) {
        std::string message;
        if (json.isOk()) {
            message = toDisplayable(json.unwrap()["message"].asString().unwrapOr(""));
        }
        log::warn(
            "Proxy request failed (HTTP {}): {} {}",
            response.code(), message, response.errorMessage()
        );
        if (isChat) {
            if (message.empty()) {
                message = response.code() > 0
                    ? fmt::format("Couldn't reach the AI (HTTP {}).", response.code())
                    : "Couldn't reach the AI. Check the proxy URL and your connection.";
            }
            m_lastChatError = message;
            SpeechBubble::show(this->getName(), message, true);
        }
        this->notifyUpdate();
        return;
    }

    std::string reply;
    if (json.isOk()) {
        reply = toDisplayable(json.unwrap()["reply"].asString().unwrapOr(""));
    }
    if (reply.empty()) {
        log::warn("Proxy returned no reply: {}", response.string().unwrapOr("<binary>"));
        if (isChat) {
            m_lastChatError = "The AI had nothing to say. Try again.";
            SpeechBubble::show(this->getName(), m_lastChatError, true);
        }
        this->notifyUpdate();
        return;
    }

    this->remember(std::move(userTurn), reply);
    SpeechBubble::show(this->getName(), reply);
    this->notifyUpdate();
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
