#pragma once

#include <Geode/Geode.hpp>

#include <string>

// Turns GD's own data into short plain-English facts for the AI, so it
// knows what the level actually is (difficulty, creator, song...) and what
// the player was doing when something happened (game mode, speed...).
namespace gameinfo {
    // One line for event messages: "Bloodbath" by Riot | Extreme Demon, 10 stars, Featured | XL | ...
    std::string levelFacts(GJGameLevel* level, bool platformer);

    // Longer version for the system prompt, including the level description
    std::string levelAbout(GJGameLevel* level, bool platformer);

    // "in the wave, 3x speed, upside down" for the given player
    std::string situation(PlayerObject* player, bool dual);
}
