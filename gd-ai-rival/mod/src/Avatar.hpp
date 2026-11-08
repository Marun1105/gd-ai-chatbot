#pragma once

#include <Geode/Geode.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

// The little picture next to the AI's name in the speech bubble: a cube from
// GD's icon kit in two colors, a 16x16 pixel drawing the player made in the
// avatar editor, a copy of the player's own icon, or nothing.

enum class AvatarMode { Off, Icon, Drawn, Player };

struct AvatarLook {
    AvatarMode mode = AvatarMode::Icon;
    int icon = 1;
    // Indices into GD's color palette, as in the icon kit
    int color1 = 11;
    int color2 = 3;
    bool glow = false;
    int glowColor = 3;
    // GRID x GRID pixels, row by row from the top; each character is a slot
    // in drawPalette(), '.' is see-through
    std::string pixels;

    bool operator==(AvatarLook const&) const = default;
};

namespace avatar {
    constexpr int GRID = 16;
    constexpr int PIXEL_COUNT = GRID * GRID;
    constexpr char EMPTY = '.';
    // GD 2.2 has 107 player colors
    constexpr int COLOR_COUNT = 107;

    // Fixed drawing palette. Slot i is stored as the letter 'a' + i, so never
    // reorder or remove entries: saved drawings refer to them by position.
    std::span<cocos2d::ccColor3B const> drawPalette();
    char slotChar(int slot);
    // -1 for see-through or unknown characters
    int slotIndex(char c);

    // A simple cube face, so the drawing canvas never starts out blank
    std::string starterDrawing();
    // Any string -> exactly PIXEL_COUNT valid characters
    std::string normalizePixels(std::string_view pixels);

    int iconCount();
    // GD's colors sorted by hue, then grays, the way a color picker should look
    std::vector<int> pickerOrder();
    cocos2d::ccColor3B gdColor(int index);

    // Sprite for the bubble or a preview, about `size` units across.
    // Null when the mode is Off.
    cocos2d::CCNode* create(AvatarLook const& look, float size);
    // Just the pixel drawing, as a crisp sprite
    cocos2d::CCSprite* createDrawing(std::string_view pixels, float size);

    char const* modeName(AvatarMode mode);
}

template <>
struct matjson::Serialize<AvatarLook> {
    static geode::Result<AvatarLook> fromJson(matjson::Value const& value);
    static matjson::Value toJson(AvatarLook const& look);
};
