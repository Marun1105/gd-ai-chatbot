#pragma once

#include <string>
#include <string_view>

namespace text {
    // GD's bitmap fonts only have printable ASCII: drop everything else
    // and fold line breaks into spaces.
    std::string toDisplayable(std::string_view text);

    // Turn a raw model reply into something that fits the speech bubble:
    // ASCII punctuation, no markdown, no wrapping quotes, at most maxChars.
    std::string cleanReply(std::string_view reply, size_t maxChars = 220);

    // "sk-ant-api03-abc...wxyz" -> "sk-ant-*****wxyz", safe to show on screen
    std::string maskKey(std::string_view key);
}
