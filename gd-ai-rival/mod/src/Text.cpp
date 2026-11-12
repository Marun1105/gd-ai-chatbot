#include "Text.hpp"

#include <array>
#include <utility>

namespace {
    // Common typographic characters models like to use, as UTF-8
    constexpr std::array<std::pair<std::string_view, std::string_view>, 9> ASCII_FOLDS {{
        { "‘", "'" }, { "’", "'" }, { "“", "\"" }, { "”", "\"" },
        { "–", "-" }, { "—", "-" }, { "−", "-" }, { "…", "..." },
        { " ", " " },
    }};
}

std::string text::toDisplayable(std::string_view text) {
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

std::string text::cleanReply(std::string_view reply, size_t maxChars) {
    std::string folded;
    folded.reserve(reply.size());
    for (size_t i = 0; i < reply.size();) {
        bool matched = false;
        for (auto const& [from, to] : ASCII_FOLDS) {
            if (reply.substr(i).starts_with(from)) {
                folded += to;
                i += from.size();
                matched = true;
                break;
            }
        }
        if (matched) continue;
        char c = reply[i++];
        // Markdown leftovers look like noise in a speech bubble
        if (c == '*' || c == '_' || c == '`' || c == '#') continue;
        folded += c;
    }

    std::string out;
    for (char c : toDisplayable(folded)) {
        if (c == ' ' && (out.empty() || out.back() == ' ')) continue;
        out += c;
    }

    if (out.size() >= 2 && out.front() == '"' && out.back() == '"') {
        out = out.substr(1, out.size() - 2);
    }

    if (out.size() > maxChars) {
        auto cut = out.substr(0, maxChars);
        size_t lastEnd = std::string::npos;
        for (auto stop : { ". ", "! ", "? " }) {
            auto pos = cut.rfind(stop);
            if (pos != std::string::npos && (lastEnd == std::string::npos || pos > lastEnd)) {
                lastEnd = pos;
            }
        }
        if (lastEnd != std::string::npos && lastEnd > maxChars / 2) {
            out = cut.substr(0, lastEnd + 1);
        }
        else {
            auto space = cut.rfind(' ');
            out = cut.substr(0, space == std::string::npos ? cut.size() : space) + "...";
        }
    }
    return out;
}

std::string text::maskKey(std::string_view key) {
    if (key.empty()) return "";
    if (key.size() <= 10) return std::string(key.size(), '*');

    // Keep a recognisable prefix ("sk-ant-", "gsk_", "AIza") and the last 4 chars
    size_t prefix = 0;
    for (size_t i = 0; i < key.size() && i < 8; i++) {
        if (key[i] == '-' || key[i] == '_') prefix = i + 1;
    }
    if (prefix == 0) prefix = 4;
    return std::string(key.substr(0, prefix)) + "*****" + std::string(key.substr(key.size() - 4));
}

std::pair<std::string, std::string> text::splitCommands(std::string_view reply) {
    std::string spoken(reply), commands;
    // Every [DO: ...] block is taken out, wherever the model put it
    for (;;) {
        size_t start = std::string::npos;
        for (size_t i = 0; i + 3 < spoken.size(); i++) {
            if (spoken[i] == '[' && (spoken[i + 1] == 'D' || spoken[i + 1] == 'd')
                && (spoken[i + 2] == 'O' || spoken[i + 2] == 'o') && (spoken[i + 3] == ':' || spoken[i + 3] == ' ')) {
                start = i;
                break;
            }
        }
        if (start == std::string::npos) break;
        auto end = spoken.find(']', start);
        auto inner = spoken.substr(start + 4, end == std::string::npos ? std::string::npos : end - start - 4);
        if (!commands.empty()) commands += ", ";
        commands += inner;
        spoken.erase(start, end == std::string::npos ? std::string::npos : end - start + 1);
    }
    return { spoken, commands };
}
