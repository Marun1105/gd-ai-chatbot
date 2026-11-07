#pragma once

#include <string>

namespace prompt {
    // Character description from the Personality preset (or the custom text)
    std::string persona(std::string const& aiName);

    // Fixed reply rules plus the character, for providers we call directly.
    // Mirrors buildSystemPrompt() in proxy/src/prompt.ts; keep the two in sync.
    std::string system(std::string const& persona);
}
