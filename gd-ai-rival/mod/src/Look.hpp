#pragma once

#include <Geode/Geode.hpp>

#include <string>

// How the speech bubble and the chat window look and sound. Each is saved as
// one setting (custom:bubble, custom:chat-look) and edited in a designer with
// a live preview, rather than as a dozen separate number rows.

enum class BubbleAnchor { Top, Bottom, TopLeft, TopRight, BottomLeft, BottomRight };
enum class BubbleFont { Chat, Big, Gold };
enum class TalkSound { Blip, Chirp, Soft, Off };

struct BubbleLook {
    BubbleAnchor anchor = BubbleAnchor::Top;
    // Nudge from the anchor, in screen units
    float offsetX = 0.f;
    float offsetY = 0.f;
    float width = 360.f;
    float textScale = .7f;
    BubbleFont font = BubbleFont::Chat;
    cocos2d::ccColor4B background { 0, 0, 0, 170 };
    cocos2d::ccColor3B textColor { 255, 255, 255 };
    cocos2d::ccColor3B nameColor { 255, 255, 255 };
    // Base seconds on screen; longer replies stay a little longer
    float duration = 4.f;
    bool typewriter = true;
    TalkSound sound = TalkSound::Blip;
    int volume = 60;

    bool operator==(BubbleLook const& other) const;
};

struct ChatLook {
    bool customColor = false;
    cocos2d::ccColor3B color { 40, 40, 90 };
    bool large = false;

    bool operator==(ChatLook const& other) const;
};

namespace look {
    constexpr float MAX_OFFSET = 300.f;
    constexpr float MIN_WIDTH = 150.f;
    constexpr float MAX_WIDTH = 500.f;
    constexpr float MIN_TEXT_SCALE = .3f;
    constexpr float MAX_TEXT_SCALE = 1.5f;
    constexpr float MIN_DURATION = 1.f;
    constexpr float MAX_DURATION = 20.f;

    char const* fontFile(BubbleFont font);
    // Empty for TalkSound::Off
    std::string soundFile(TalkSound sound);
    char const* soundName(TalkSound sound);
    char const* anchorName(BubbleAnchor anchor);

    // Accept names from settings and level configs ("top-left", "Top Left", "chirp"...)
    BubbleAnchor parseAnchor(std::string_view text);
    BubbleFont parseFont(std::string_view text);
    TalkSound parseSound(std::string_view text);

    // Keep every number in its allowed range
    BubbleLook clamped(BubbleLook look);
}

template <>
struct matjson::Serialize<BubbleLook> {
    static geode::Result<BubbleLook> fromJson(matjson::Value const& value);
    static matjson::Value toJson(BubbleLook const& look);
};

template <>
struct matjson::Serialize<ChatLook> {
    static geode::Result<ChatLook> fromJson(matjson::Value const& value);
    static matjson::Value toJson(ChatLook const& look);
};
