#pragma once

#include <Geode/Geode.hpp>

#include <string>
#include <string_view>
#include <vector>

// Grin's hands on the game. The AI playing Grin ends its replies with a
// command line, e.g. "[DO: knock, crack, anger 70]", and this runs it: scare
// effects over the level, tricks on the level itself (mirror, tilt, zoom,
// blackout), its own anger, when it strikes, and its attacks in the fight.
// The screen also decays on its own as the anger bar fills, so the bar means
// something you can see, and everything done is reported back to the AI.
namespace haunt {
    // Puts the effect layer over the level (safe to call every frame)
    void attach();
    void detach();

    // The part of the system prompt that tells the AI what it can do
    std::string powers();
    // Runs a command line from a reply ("knock, crack, anger 70")
    void run(std::string_view commands);
    // One effect by name; false if unknown or not allowed right now
    bool effect(std::string_view name);

    // A death at this anger level: the screen decays a little by itself
    void onDeath();
    // Anger changed (calming clears the damage)
    void onAngerChanged();
    // Back from the scare and the fight: a clean screen
    void reset();

    // " Since your last message you did: knock, crack." (and forgets it)
    std::string takeLog();

    // Attacks the AI chose for the fight, in order; empty = the fight decides
    std::vector<std::string> const& fightPlan();
}
