#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>

#include "Profile.hpp"
#include "Providers.hpp"

#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>

// Owns the conversation with the AI: the rolling memory, the in-flight
// request, and the rules that decide which game events deserve a reply.
// One instance lives for the whole game session; memory resets per level.
class AIClient {
public:
    struct Turn {
        bool fromAI;
        // What the AI sees, e.g. "[PLAYER] hi (attempt 3, best 41%)"
        std::string text;
        // What the chat popup shows, e.g. "You: hi"
        std::string display;
    };

    struct LevelInfo {
        std::string name;
        int id = 0;
        int bestPercent = 0;
        int totalAttempts = 0;
        bool platformer = false;
        // One-line summary for event messages, and the full version for the system prompt
        std::string facts;
        std::string about;
    };

    static AIClient& get();

    void startLevel(LevelInfo info);
    void endLevel();
    bool isActive() const;

    void sendChat(std::string const& message);

    void onLevelStart();
    // situation: what the player was doing, e.g. "in the wave, 2x speed"
    // silent: keep count (best, death spots) but don't comment, e.g. while
    // Grin's screen-break plays and gets its own comment afterwards
    void onDeath(int percent, int attempt, bool practice, std::string const& situation, bool silent = false);
    // Grin just smashed the screen (and maybe the player fought it)
    void onRage(bool fought, bool won);
    void onComplete(int attempt, bool practice);

    bool isWaiting() const;
    bool isWaitingForChat() const;
    std::deque<Turn> const& getMemory() const;
    std::string getName() const;
    // Why the last chat message got no reply, or empty
    std::string const& getLastChatError() const;
    // What the current AI connection still needs ("an API key"...), or empty
    std::string missingSetup() const;
    // The player changed provider, key or model
    void connectionChanged();
    // Random, anonymous ID a creator proxy uses to rate-limit per player
    std::string const& getPlayerID();

    // Called whenever memory or the waiting state changes (used by the chat popup)
    void setUpdateCallback(std::function<void()> callback);

private:
    enum class Priority {
        // Ordinary deaths: cooldown + random chance
        Normal,
        // Deaths close to the best: half cooldown, no dice roll
        CloseCall,
        // New bests: short minimum gap, may replace a pending comment
        Important,
        // Level start and completion: always sent, may replace a pending comment
        Always,
    };

    AIClient();

    bool canSendEvent(Priority priority);
    void sendEvent(std::string text, std::string display);
    void send(std::string text, std::string display, bool isChat);
    void onResponse(
        Connection const& connection, geode::utils::web::WebResponse const& response,
        size_t maxChars, Turn userTurn, bool isChat
    );
    // Counts one message against the daily limit; false when the limit is used up
    bool takeDailyMessage();
    void remember(Turn userTurn, std::string reply);
    void notifyUpdate();
    std::string statusLine() const;
    // "63% (4x), 41% (3x)" for this session, or empty
    std::string deathHotspots() const;

    geode::async::TaskHolder<geode::utils::web::WebResponse> m_request;
    bool m_requestIsChat = false;
    std::string m_lastChatError;
    bool m_warnedAboutSetup = false;
    std::deque<Turn> m_memory;
    LevelInfo m_level;
    bool m_active = false;
    // Bumped on every level change so late replies from an old level are dropped
    unsigned m_generation = 0;
    int m_attempt = 0;
    std::vector<int> m_unreportedDeaths;
    // Death stats for this visit to the level, so the AI can spot patterns
    std::map<int, int> m_deathsAt;
    int m_sessionDeaths = 0;
    std::chrono::steady_clock::time_point m_sessionStart{};
    std::chrono::steady_clock::time_point m_lastSent{};
    std::function<void()> m_onUpdate;
    std::string m_playerID;
    std::mt19937 m_rng;
};
