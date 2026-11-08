#pragma once

#include <Geode/Geode.hpp>

#include <string>

// Grin's anger. Every death makes it angrier, by how much depending on how
// painful the death was: barely at all for 1% on a first try of an easy
// level, a lot for 90%+ on an extreme demon, more for dying at the same spot
// again and again. New bests calm it a little, beating the level calms it
// completely, and it slowly cools down while you stay alive. At 100 it snaps.
namespace rage {
    constexpr float MAX = 100.f;

    // Is the current character Grin (the player's pick or the level's)?
    bool active();

    void startLevel(GJGameLevel* level, bool platformer);
    void endLevel();

    struct DeathResult {
        float gained = 0.f;
        bool newBest = false;
        // Anger hit the limit and the jumpscare should play
        bool snapped = false;
    };
    // testMode: started from a start position
    DeathResult onDeath(int percent, int attempt, bool practice, bool testMode);
    void onComplete();
    // While the player is alive and unpaused
    void coolDown(float seconds);
    // After the screen break: still annoyed (or calm, if the player beat it)
    void afterSnap(float anger);

    float anger();
    // 0..1 for the bar
    float level();
    // "calm", "irritated", "angry", "furious", "about to snap"
    char const* mood();
    // Appended to game events for the AI: " Your anger is now 72/100 (furious)."
    std::string eventNote();

    // Difficulty weight of a level: 0.3 for auto up to 2.0 for an extreme demon
    float difficultyWeight(GJGameLevel* level);
}
