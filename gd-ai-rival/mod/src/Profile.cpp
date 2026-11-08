#include "Profile.hpp"
#include "CustomSettings.hpp"
#include "Text.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <optional>

using namespace geode::prelude;

namespace {
    constexpr size_t MAX_PERSONA_CHARS = 1000;
    constexpr size_t MAX_NOTE_CHARS = 200;
    constexpr size_t MAX_NAME_CHARS = 24;
    constexpr int TEXT_OBJECT_ID = 914;

    // {} is replaced with the AI's name
    constexpr auto RIVAL =
        "You are {}, a cocky Geometry Dash player who thinks they are better than the player. "
        "Trash-talk silly deaths with playful roasts, act annoyed when they make progress, and give grudging "
        "respect when they get a new best or beat the level. Keep it playful, never hateful.";
    constexpr auto COACH =
        "You are {}, an upbeat Geometry Dash coach. Hype up every bit of progress, give a quick tip or a word of "
        "encouragement after deaths, and celebrate new bests and completions like a sports commentator.";
    constexpr auto CHILL =
        "You are {}, a laid-back friend watching the player's Geometry Dash run. Relaxed, friendly and a bit funny; "
        "you keep the mood light and never pressure them.";
    constexpr auto SARCASTIC =
        "You are {}, a dry, deadpan narrator watching the player's Geometry Dash run. Unimpressed, sarcastic "
        "one-liners about deaths and progress, with the occasional reluctant compliment.";
    // Grin's anger level arrives with every game event (see Rage.cpp)
    constexpr auto GRIN =
        "You are {}, a grinning yellow creature that lives inside the player's Geometry Dash screen and watches "
        "every attempt. Each game event tells you your anger from 0 to 100; let it set your tone. Calm: eerily "
        "polite, smiling, a little too friendly. Irritated: cold and clipped. Angry: openly hostile about their "
        "mistakes. Furious: barely holding it together, short sharp lines. When you snap, you smash their screen "
        "from the inside and then gloat about it. Your menace is theatrical and only about the game and the screen "
        "you live in, never about the real person.";

    // Creator config from the current level, read once per level
    bool s_scanned = false;
    PlayLayer* s_scannedLevel = nullptr;
    std::map<std::string, std::string> s_levelConfig;

    template <class T>
    T setting(std::string_view key) {
        return Mod::get()->getSettingValue<T>(key);
    }

    std::string lower(std::string_view text) {
        std::string out(text);
        for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return out;
    }

    // "Bubble Color", "bubble_color" and "bubble-color" all mean the same key
    std::string normalizeKey(std::string_view key) {
        auto out = lower(utils::string::trim(std::string(key)));
        for (auto& c : out) {
            if (c == '_' || c == ' ') c = '-';
        }
        return out;
    }

    std::string clip(std::string_view text, size_t max) {
        auto out = text::toDisplayable(utils::string::trim(std::string(text)));
        if (out.size() > max) out.resize(max);
        return out;
    }

    std::optional<float> parseNumber(std::string_view text) {
        // strtof rather than from_chars: Android's libc++ has no float from_chars
        auto trimmed = utils::string::trim(std::string(text));
        char* end = nullptr;
        float value = std::strtof(trimmed.c_str(), &end);
        if (end == trimmed.c_str()) return std::nullopt;
        return value;
    }

    std::optional<bool> parseBool(std::string_view text) {
        auto value = lower(utils::string::trim(std::string(text)));
        if (value == "on" || value == "yes" || value == "true" || value == "1") return true;
        if (value == "off" || value == "no" || value == "false" || value == "0") return false;
        return std::nullopt;
    }

    // "255, 80, 80", "255,80,80,200", "#ff5050" or "#ff5050c8"
    std::optional<ccColor4B> parseColor(std::string_view text, GLubyte defaultAlpha = 255) {
        auto value = utils::string::trim(std::string(text));
        std::vector<int> parts;
        if (value.starts_with('#') && (value.size() == 7 || value.size() == 9)) {
            for (size_t i = 1; i < value.size(); i += 2) {
                int part = 0;
                auto [ptr, ec] = std::from_chars(value.data() + i, value.data() + i + 2, part, 16);
                if (ec != std::errc()) return std::nullopt;
                parts.push_back(part);
            }
        }
        else {
            for (auto const& piece : utils::string::split(value, ",")) {
                auto number = parseNumber(piece);
                if (!number) return std::nullopt;
                parts.push_back(static_cast<int>(*number));
            }
        }
        if (parts.size() != 3 && parts.size() != 4) return std::nullopt;
        auto channel = [](int v) { return static_cast<GLubyte>(std::clamp(v, 0, 255)); };
        return ccColor4B {
            channel(parts[0]), channel(parts[1]), channel(parts[2]),
            parts.size() == 4 ? channel(parts[3]) : defaultAlpha,
        };
    }

    ccColor3B rgb(ccColor4B color) {
        return { color.r, color.g, color.b };
    }

    ReplyLength parseLength(std::string_view text) {
        auto value = normalizeKey(text);
        if (value == "very-short" || value == "veryshort" || value == "tiny") return ReplyLength::VeryShort;
        if (value == "medium" || value == "long") return ReplyLength::Medium;
        return ReplyLength::Short;
    }

    BubbleAnchor parseAnchor(std::string_view text) {
        return look::parseAnchor(text);
    }

    std::string fontFile(std::string_view text) {
        return look::fontFile(look::parseFont(text));
    }

    std::string soundFile(std::string_view text) {
        return look::soundFile(look::parseSound(text));
    }

    std::string presetPersona(std::string_view preset, std::string const& name) {
        auto value = normalizeKey(preset);
        auto const* text = RIVAL;
        if (value == "coach") text = COACH;
        else if (value == "chill") text = CHILL;
        else if (value == "sarcastic") text = SARCASTIC;
        else if (value == "grin") text = GRIN;
        return fmt::format(fmt::runtime(text), name);
    }

    void scanLevel() {
        auto playLayer = PlayLayer::get();
        // Objects may not exist yet while the level is still loading; try again later
        if (!playLayer || !playLayer->m_objects || playLayer->m_objects->count() == 0) return;

        s_scanned = true;
        for (auto object : CCArrayExt<GameObject*>(playLayer->m_objects)) {
            if (object->m_objectID != TEXT_OBJECT_ID) continue;
            auto textObject = typeinfo_cast<TextGameObject*>(object);
            if (!textObject) continue;
            for (auto& [key, value] : profile::parseConfigText(textObject->m_text)) {
                s_levelConfig[key] = value;
            }
        }
        if (!s_levelConfig.empty()) {
            log::info("This level customizes the AI ({} settings)", s_levelConfig.size());
        }
    }

    // Level config on top of the player's profile. Returns the level's persona, if any.
    std::string applyLevelConfig(Profile& p, std::string& preset) {
        std::string persona;
        auto& events = p.events;
        auto& bubble = p.bubble;

        for (auto const& [key, value] : s_levelConfig) {
            bool used = true;
            if (key == "name") {
                if (auto name = clip(value, MAX_NAME_CHARS); !name.empty()) p.name = name;
            }
            else if (key == "persona" || key == "personality") persona = clip(value, MAX_PERSONA_CHARS);
            else if (key == "preset") preset = value;
            else if (key == "length" || key == "reply-length") p.length = parseLength(value);
            // A level can turn reactions off, never back on over the player's choice
            else if (key == "start" || key == "greet") events.start = events.start && parseBool(value).value_or(true);
            else if (key == "death" || key == "deaths") events.death = events.death && parseBool(value).value_or(true);
            else if (key == "close-call" || key == "closecall") events.closeCall = events.closeCall && parseBool(value).value_or(true);
            else if (key == "new-best" || key == "newbest") events.newBest = events.newBest && parseBool(value).value_or(true);
            else if (key == "complete" || key == "finish") events.complete = events.complete && parseBool(value).value_or(true);
            else if (key == "close-call-range") {
                if (auto n = parseNumber(value)) events.closeCallRange = std::clamp(static_cast<int>(*n), 1, 20);
            }
            else if (key == "on-start") events.onStart = clip(value, MAX_NOTE_CHARS);
            else if (key == "on-death") events.onDeath = clip(value, MAX_NOTE_CHARS);
            else if (key == "on-new-best" || key == "on-newbest") events.onNewBest = clip(value, MAX_NOTE_CHARS);
            else if (key == "on-complete" || key == "on-finish") events.onComplete = clip(value, MAX_NOTE_CHARS);
            else if (key == "bubble-position" || key == "position") bubble.anchor = parseAnchor(value);
            else if (key == "bubble-color") {
                if (auto c = parseColor(value, bubble.background.a)) bubble.background = *c;
            }
            else if (key == "text-color") {
                if (auto c = parseColor(value)) bubble.textColor = rgb(*c);
            }
            else if (key == "name-color") {
                if (auto c = parseColor(value)) bubble.nameColor = rgb(*c);
            }
            else if (key == "bubble-font" || key == "font") bubble.font = fontFile(value);
            else if (key == "bubble-width" || key == "width") {
                if (auto n = parseNumber(value)) bubble.width = std::clamp(*n, 150.f, 500.f);
            }
            else if (key == "text-size" || key == "bubble-text-size") {
                if (auto n = parseNumber(value)) bubble.textScale = std::clamp(*n, .3f, 1.5f);
            }
            else if (key == "bubble-x") {
                if (auto n = parseNumber(value)) bubble.offsetX = std::clamp(*n, -300.f, 300.f);
            }
            else if (key == "bubble-y") {
                if (auto n = parseNumber(value)) bubble.offsetY = std::clamp(*n, -300.f, 300.f);
            }
            else if (key == "duration") {
                if (auto n = parseNumber(value)) bubble.duration = std::clamp(*n, 1.f, 20.f);
            }
            else if (key == "typewriter") bubble.typewriter = parseBool(value).value_or(bubble.typewriter);
            else if (key == "avatar") {
                auto mode = normalizeKey(value);
                if (mode == "off" || mode == "none") bubble.avatar.mode = AvatarMode::Off;
                else if (mode == "player" || mode == "me") bubble.avatar.mode = AvatarMode::Player;
                else if (auto icon = parseNumber(value)) {
                    bubble.avatar.mode = AvatarMode::Icon;
                    bubble.avatar.icon = std::clamp(static_cast<int>(*icon), 1, avatar::iconCount());
                }
            }
            else if (key == "avatar-colors") {
                // "main, second" or "main, second, glow"
                auto parts = utils::string::split(value, ",");
                auto color = [](std::string_view part) -> std::optional<int> {
                    if (auto n = parseNumber(part)) return std::clamp(static_cast<int>(*n), 0, avatar::COLOR_COUNT - 1);
                    return std::nullopt;
                };
                if (parts.size() >= 2) {
                    if (auto c = color(parts[0])) bubble.avatar.color1 = *c;
                    if (auto c = color(parts[1])) bubble.avatar.color2 = *c;
                }
                if (parts.size() >= 3) {
                    if (auto c = color(parts[2])) {
                        bubble.avatar.glow = true;
                        bubble.avatar.glowColor = *c;
                    }
                }
            }
            else if (key == "avatar-drawing") {
                // The code from the avatar editor's Copy button
                if (auto code = avatar::parseDrawingCode(value)) {
                    bubble.avatar.mode = AvatarMode::Drawn;
                    bubble.avatar.pixels = *code;
                }
            }
            else if (key == "sound") p.soundFile = soundFile(value);
            else if (key == "chat-color") {
                if (auto c = parseColor(value)) {
                    p.chatCustomColor = true;
                    p.chatColor = rgb(*c);
                }
            }
            else used = false;

            if (used) p.fromLevel = true;
        }
        return persona;
    }
}

std::map<std::string, std::string> profile::parseConfigText(std::string_view text) {
    std::map<std::string, std::string> entries;
    auto trimmed = utils::string::trim(std::string(text));
    auto prefix = lower(trimmed.substr(0, 8));

    size_t start;
    if (prefix.starts_with("ai:")) start = 3;
    else if (prefix.starts_with("airival:")) start = 8;
    else return entries;

    // Several "key = value" entries per text object, split by | or new lines
    auto body = trimmed.substr(start);
    size_t pos = 0;
    while (pos <= body.size()) {
        auto end = body.find_first_of("|\n", pos);
        if (end == std::string::npos) end = body.size();
        auto entry = body.substr(pos, end - pos);
        pos = end + 1;

        // "=" first, so a persona like "persona = You are Bob: a wizard" keeps its colon
        auto split = entry.find('=');
        if (split == std::string::npos) split = entry.find(':');
        if (split == std::string::npos) continue;

        auto key = normalizeKey(entry.substr(0, split));
        auto value = utils::string::trim(entry.substr(split + 1));
        if (!key.empty() && !value.empty()) entries[key] = value;
    }
    return entries;
}

void profile::resetLevel() {
    s_scanned = false;
    s_scannedLevel = nullptr;
    s_levelConfig.clear();
}

size_t profile::maxReplyChars(ReplyLength length) {
    switch (length) {
        case ReplyLength::VeryShort: return 120;
        case ReplyLength::Medium: return 320;
        case ReplyLength::Short: default: return 220;
    }
}

Profile profile::current() {
    Profile p;

    auto name = clip(setting<std::string>("ai-name"), MAX_NAME_CHARS);
    p.name = name.empty() ? "Rival" : name;
    auto preset = setting<std::string>("personality-preset");
    auto custom = clip(settings::longText("custom-personality"), MAX_PERSONA_CHARS);
    p.length = parseLength(setting<std::string>("reply-length"));

    auto& events = p.events;
    events.start = setting<bool>("react-start");
    events.death = setting<bool>("react-deaths");
    events.closeCall = setting<bool>("react-close-calls");
    events.newBest = setting<bool>("react-new-best");
    events.complete = setting<bool>("react-complete");
    events.closeCallRange = static_cast<int>(setting<int64_t>("close-call-range"));
    events.onStart = clip(settings::longText("note-start"), MAX_NOTE_CHARS);
    events.onDeath = clip(settings::longText("note-death"), MAX_NOTE_CHARS);
    events.onNewBest = clip(settings::longText("note-new-best"), MAX_NOTE_CHARS);
    events.onComplete = clip(settings::longText("note-complete"), MAX_NOTE_CHARS);

    auto& bubble = p.bubble;
    auto bubbleLook = settings::bubble();
    bubble.anchor = bubbleLook.anchor;
    bubble.offsetX = bubbleLook.offsetX;
    bubble.offsetY = bubbleLook.offsetY;
    bubble.width = bubbleLook.width;
    bubble.textScale = bubbleLook.textScale;
    bubble.font = look::fontFile(bubbleLook.font);
    bubble.background = bubbleLook.background;
    bubble.textColor = bubbleLook.textColor;
    bubble.nameColor = bubbleLook.nameColor;
    bubble.duration = bubbleLook.duration;
    bubble.typewriter = bubbleLook.typewriter;
    bubble.avatar = settings::avatar();

    p.soundFile = look::soundFile(bubbleLook.sound);
    p.soundVolume = static_cast<float>(bubbleLook.volume) / 100.f;

    auto chat = settings::chatLook();
    p.chatCustomColor = chat.customColor;
    p.chatColor = chat.color;
    p.chatLarge = chat.large;

    // Outside a level (e.g. testing from the settings page) only the player's settings apply
    std::string levelPersona;
    if (auto playLayer = PlayLayer::get(); playLayer && setting<bool>("allow-level-config")) {
        if (playLayer != s_scannedLevel) {
            resetLevel();
            s_scannedLevel = playLayer;
        }
        if (!s_scanned) scanLevel();
        levelPersona = applyLevelConfig(p, preset);
    }

    // Characters with their own mechanics (Grin's anger) follow the preset,
    // even when a level swaps in its own personality text
    p.character = normalizeKey(preset);

    if (!levelPersona.empty()) {
        p.persona = levelPersona;
    }
    else if (normalizeKey(preset) == "custom" && !custom.empty()) {
        p.persona = custom;
    }
    else {
        p.persona = presetPersona(preset, p.name);
    }
    return p;
}
