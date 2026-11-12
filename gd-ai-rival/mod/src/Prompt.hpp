#pragma once

#include "Profile.hpp"

#include <string>

namespace prompt {
    // Fixed reply rules, Geometry Dash background knowledge, facts about the
    // current level (may be empty) and the character, for providers we call
    // directly. Mirrors buildSystemPrompt() in proxy/src/prompt.ts; keep the
    // two in sync.
    // powers: what the character can do to the game (see Haunt.cpp), or empty
    std::string system(
        std::string const& persona, ReplyLength length, std::string const& levelAbout, std::string const& powers = ""
    );
}
