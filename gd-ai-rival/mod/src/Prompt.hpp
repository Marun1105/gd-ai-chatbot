#pragma once

#include "Profile.hpp"

#include <string>

namespace prompt {
    // Fixed reply rules, Geometry Dash background knowledge, facts about the
    // current level (may be empty) and the character, for providers we call
    // directly. Mirrors buildSystemPrompt() in proxy/src/prompt.ts; keep the
    // two in sync.
    std::string system(std::string const& persona, ReplyLength length, std::string const& levelAbout);
}
