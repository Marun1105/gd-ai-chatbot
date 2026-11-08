#include "Look.hpp"

#include <algorithm>
#include <cctype>

using namespace geode::prelude;

namespace {
    bool same(ccColor3B a, ccColor3B b) {
        return a.r == b.r && a.g == b.g && a.b == b.b;
    }

    bool same(ccColor4B a, ccColor4B b) {
        return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    }

    // "Top Left", "top_left" and "top-left" all become "top-left"
    std::string normalize(std::string_view text) {
        std::string out;
        for (char c : utils::string::trim(std::string(text))) {
            if (c == ' ' || c == '_') c = '-';
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return out;
    }

    char const* fontKey(BubbleFont font) {
        switch (font) {
            case BubbleFont::Big: return "big";
            case BubbleFont::Gold: return "gold";
            case BubbleFont::Chat: default: return "chat";
        }
    }

    char const* anchorKey(BubbleAnchor anchor) {
        switch (anchor) {
            case BubbleAnchor::Bottom: return "bottom";
            case BubbleAnchor::TopLeft: return "top-left";
            case BubbleAnchor::TopRight: return "top-right";
            case BubbleAnchor::BottomLeft: return "bottom-left";
            case BubbleAnchor::BottomRight: return "bottom-right";
            case BubbleAnchor::Top: default: return "top";
        }
    }

    float readFloat(matjson::Value const& obj, std::string_view key, float fallback) {
        if (auto value = obj[key].as<double>()) return static_cast<float>(value.unwrap());
        return fallback;
    }
}

bool BubbleLook::operator==(BubbleLook const& o) const {
    return anchor == o.anchor && offsetX == o.offsetX && offsetY == o.offsetY && width == o.width
        && textScale == o.textScale && font == o.font && same(background, o.background)
        && same(textColor, o.textColor) && same(nameColor, o.nameColor) && duration == o.duration
        && typewriter == o.typewriter && sound == o.sound && volume == o.volume;
}

bool ChatLook::operator==(ChatLook const& o) const {
    return customColor == o.customColor && same(color, o.color) && large == o.large;
}

char const* look::fontFile(BubbleFont font) {
    switch (font) {
        case BubbleFont::Big: return "bigFont.fnt";
        case BubbleFont::Gold: return "goldFont.fnt";
        case BubbleFont::Chat: default: return "chatFont.fnt";
    }
}

std::string look::soundFile(TalkSound sound) {
    switch (sound) {
        case TalkSound::Blip: return "blip.wav"_spr;
        case TalkSound::Chirp: return "chirp.wav"_spr;
        case TalkSound::Soft: return "soft.wav"_spr;
        case TalkSound::Off: default: return "";
    }
}

char const* look::soundName(TalkSound sound) {
    switch (sound) {
        case TalkSound::Blip: return "Blip";
        case TalkSound::Chirp: return "Chirp";
        case TalkSound::Soft: return "Soft";
        case TalkSound::Off: default: return "Off";
    }
}

char const* look::anchorName(BubbleAnchor anchor) {
    switch (anchor) {
        case BubbleAnchor::Bottom: return "Bottom";
        case BubbleAnchor::TopLeft: return "Top Left";
        case BubbleAnchor::TopRight: return "Top Right";
        case BubbleAnchor::BottomLeft: return "Bottom Left";
        case BubbleAnchor::BottomRight: return "Bottom Right";
        case BubbleAnchor::Top: default: return "Top";
    }
}

BubbleAnchor look::parseAnchor(std::string_view text) {
    auto value = normalize(text);
    if (value == "bottom") return BubbleAnchor::Bottom;
    if (value == "top-left" || value == "topleft") return BubbleAnchor::TopLeft;
    if (value == "top-right" || value == "topright") return BubbleAnchor::TopRight;
    if (value == "bottom-left" || value == "bottomleft") return BubbleAnchor::BottomLeft;
    if (value == "bottom-right" || value == "bottomright") return BubbleAnchor::BottomRight;
    return BubbleAnchor::Top;
}

BubbleFont look::parseFont(std::string_view text) {
    auto value = normalize(text);
    if (value == "big") return BubbleFont::Big;
    if (value == "gold") return BubbleFont::Gold;
    return BubbleFont::Chat;
}

TalkSound look::parseSound(std::string_view text) {
    auto value = normalize(text);
    if (value == "chirp") return TalkSound::Chirp;
    if (value == "soft") return TalkSound::Soft;
    if (value == "off" || value == "none" || value == "no") return TalkSound::Off;
    return TalkSound::Blip;
}

BubbleLook look::clamped(BubbleLook l) {
    l.offsetX = std::clamp(l.offsetX, -MAX_OFFSET, MAX_OFFSET);
    l.offsetY = std::clamp(l.offsetY, -MAX_OFFSET, MAX_OFFSET);
    l.width = std::clamp(l.width, MIN_WIDTH, MAX_WIDTH);
    l.textScale = std::clamp(l.textScale, MIN_TEXT_SCALE, MAX_TEXT_SCALE);
    l.duration = std::clamp(l.duration, MIN_DURATION, MAX_DURATION);
    l.volume = std::clamp(l.volume, 0, 100);
    return l;
}

Result<BubbleLook> matjson::Serialize<BubbleLook>::fromJson(matjson::Value const& value) {
    if (!value.isObject()) return Err("Bubble look must be an object");
    BubbleLook l;
    l.anchor = look::parseAnchor(value["anchor"].asString().unwrapOr("top"));
    l.offsetX = readFloat(value, "x", l.offsetX);
    l.offsetY = readFloat(value, "y", l.offsetY);
    l.width = readFloat(value, "width", l.width);
    l.textScale = readFloat(value, "text-size", l.textScale);
    l.font = look::parseFont(value["font"].asString().unwrapOr("chat"));
    if (auto c = value["background"].as<ccColor4B>()) l.background = c.unwrap();
    if (auto c = value["text-color"].as<ccColor3B>()) l.textColor = c.unwrap();
    if (auto c = value["name-color"].as<ccColor3B>()) l.nameColor = c.unwrap();
    l.duration = readFloat(value, "duration", l.duration);
    l.typewriter = value["typewriter"].asBool().unwrapOr(l.typewriter);
    l.sound = look::parseSound(value["sound"].asString().unwrapOr("blip"));
    l.volume = static_cast<int>(readFloat(value, "volume", static_cast<float>(l.volume)));
    return Ok(look::clamped(l));
}

matjson::Value matjson::Serialize<BubbleLook>::toJson(BubbleLook const& l) {
    auto obj = matjson::Value::object();
    obj["anchor"] = anchorKey(l.anchor);
    obj["x"] = l.offsetX;
    obj["y"] = l.offsetY;
    obj["width"] = l.width;
    obj["text-size"] = l.textScale;
    obj["font"] = fontKey(l.font);
    obj["background"] = matjson::Value(l.background);
    obj["text-color"] = matjson::Value(l.textColor);
    obj["name-color"] = matjson::Value(l.nameColor);
    obj["duration"] = l.duration;
    obj["typewriter"] = l.typewriter;
    obj["sound"] = normalize(look::soundName(l.sound));
    obj["volume"] = l.volume;
    return obj;
}

Result<ChatLook> matjson::Serialize<ChatLook>::fromJson(matjson::Value const& value) {
    if (!value.isObject()) return Err("Chat look must be an object");
    ChatLook l;
    l.customColor = value["custom-color"].asBool().unwrapOr(false);
    if (auto c = value["color"].as<ccColor3B>()) l.color = c.unwrap();
    l.large = value["large"].asBool().unwrapOr(false);
    return Ok(l);
}

matjson::Value matjson::Serialize<ChatLook>::toJson(ChatLook const& l) {
    auto obj = matjson::Value::object();
    obj["custom-color"] = l.customColor;
    obj["color"] = matjson::Value(l.color);
    obj["large"] = l.large;
    return obj;
}
