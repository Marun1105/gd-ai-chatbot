#include "Avatar.hpp"

#include <algorithm>
#include <array>
#include <cmath>

using namespace geode::prelude;

namespace {
    constexpr std::array<ccColor3B, 24> PALETTE {{
        { 0, 0, 0 },       // a black
        { 255, 255, 255 }, // b white
        { 64, 64, 64 },    // c dark gray
        { 128, 128, 128 }, // d gray
        { 192, 192, 192 }, // e light gray
        { 255, 50, 50 },   // f red
        { 150, 0, 0 },     // g dark red
        { 255, 140, 0 },   // h orange
        { 255, 230, 0 },   // i yellow
        { 125, 255, 0 },   // j lime (GD's default main color)
        { 0, 200, 60 },    // k green
        { 0, 100, 40 },    // l dark green
        { 0, 255, 255 },   // m cyan (GD's default second color)
        { 0, 160, 255 },   // n light blue
        { 30, 70, 255 },   // o blue
        { 0, 0, 140 },     // p dark blue
        { 140, 40, 255 },  // q purple
        { 255, 0, 255 },   // r magenta
        { 255, 130, 200 }, // s pink
        { 120, 70, 30 },   // t brown
        { 220, 170, 110 }, // u tan
        { 255, 215, 180 }, // v peach
        { 0, 140, 140 },   // w teal
        { 160, 255, 160 }, // x mint
    }};

    constexpr int STARTER_SIZE = 16;
    constexpr std::array<std::string_view, STARTER_SIZE> STARTER {
        "aaaaaaaaaaaaaaaa",
        "ajjjjjjjjjjjjjja",
        "ajaaaaaaaaaaaaja",
        "ajammmmmmmmmmaja",
        "ajammmmmmmmmmaja",
        "ajamaaammaaamaja",
        "ajamabammabamaja",
        "ajamaaammaaamaja",
        "ajammmmmmmmmmaja",
        "ajammmmmmmmmmaja",
        "ajamaaaaaaaamaja",
        "ajammaaaaaammaja",
        "ajammmmmmmmmmaja",
        "ajaaaaaaaaaaaaja",
        "ajjjjjjjjjjjjjja",
        "aaaaaaaaaaaaaaaa",
    };

    // Cube icons are about 30 units across at scale 1
    constexpr float ICON_UNITS = 30.f;

    AvatarMode parseMode(std::string_view text) {
        if (text == "off" || text == "None") return AvatarMode::Off;
        if (text == "drawn") return AvatarMode::Drawn;
        if (text == "player" || text == "My Icon") return AvatarMode::Player;
        return AvatarMode::Icon;
    }

    char const* modeKey(AvatarMode mode) {
        switch (mode) {
            case AvatarMode::Off: return "off";
            case AvatarMode::Drawn: return "drawn";
            case AvatarMode::Player: return "player";
            case AvatarMode::Icon: default: return "icon";
        }
    }

    int readInt(matjson::Value const& obj, std::string_view key, int fallback, int min, int max) {
        if (auto value = obj[key].as<int>()) return std::clamp(value.unwrap(), min, max);
        return fallback;
    }
}

std::span<ccColor3B const> avatar::drawPalette() {
    return PALETTE;
}

char avatar::slotChar(int slot) {
    if (slot < 0 || slot >= static_cast<int>(PALETTE.size())) return EMPTY;
    return static_cast<char>('a' + slot);
}

int avatar::slotIndex(char c) {
    int slot = c - 'a';
    return slot >= 0 && slot < static_cast<int>(PALETTE.size()) ? slot : -1;
}

bool avatar::isGridSize(int size) {
    return std::find(GRID_SIZES.begin(), GRID_SIZES.end(), size) != GRID_SIZES.end();
}

int avatar::gridOf(std::string_view pixels) {
    for (int size : GRID_SIZES) {
        if (pixels.size() == static_cast<size_t>(size * size)) return size;
    }
    return DEFAULT_GRID;
}

std::string avatar::starterDrawing(int size) {
    std::string out;
    out.reserve(STARTER_SIZE * STARTER_SIZE);
    for (auto row : STARTER) out += row;
    return size == STARTER_SIZE ? out : resizeDrawing(out, size);
}

std::string avatar::emptyDrawing(int size) {
    if (!isGridSize(size)) size = DEFAULT_GRID;
    return std::string(static_cast<size_t>(size * size), EMPTY);
}

std::string avatar::normalizePixels(std::string_view pixels) {
    if (pixels.empty()) return starterDrawing();
    int size = gridOf(pixels);
    std::string out(static_cast<size_t>(size * size), EMPTY);
    for (size_t i = 0; i < out.size() && i < pixels.size(); i++) {
        if (slotIndex(pixels[i]) >= 0) out[i] = pixels[i];
    }
    return out;
}

std::string avatar::resizeDrawing(std::string_view pixels, int size) {
    if (!isGridSize(size)) size = DEFAULT_GRID;
    auto source = normalizePixels(pixels);
    int from = gridOf(source);
    if (from == size) return source;
    std::string out(static_cast<size_t>(size * size), EMPTY);

    if (size > from) {
        // More squares: each takes the nearest old square. Worked out for the
        // first half and mirrored, so symmetric drawings stay symmetric.
        auto nearest = [&](int i) { return std::min(from - 1, (2 * i + 1) * from / (2 * size)); };
        auto map = [&](int i) { return i < size / 2 ? nearest(i) : from - 1 - nearest(size - 1 - i); };
        for (int row = 0; row < size; row++) {
            for (int col = 0; col < size; col++) {
                out[row * size + col] = source[map(row) * from + map(col)];
            }
        }
        return out;
    }

    // Fewer squares: each takes the most common color of its block of old
    // squares. Ties go to the color that is rarer in the whole drawing, which
    // keeps eyes and other small details from disappearing.
    std::array<int, 128> total {};
    for (char c : source) total[static_cast<unsigned char>(c) & 127]++;
    // Block edges rounded to the nearest square, so blocks are laid out symmetrically
    auto edge = [&](int i) { return (2 * i * from + size) / (2 * size); };

    for (int row = 0; row < size; row++) {
        int r0 = edge(row);
        int r1 = std::max(r0 + 1, edge(row + 1));
        for (int col = 0; col < size; col++) {
            int c0 = edge(col);
            int c1 = std::max(c0 + 1, edge(col + 1));

            std::array<int, 128> count {};
            for (int r = r0; r < r1; r++) {
                for (int c = c0; c < c1; c++) count[static_cast<unsigned char>(source[r * from + c]) & 127]++;
            }
            char best = EMPTY;
            bool first = true;
            auto better = [&](char a, char b) {
                int ia = static_cast<unsigned char>(a) & 127;
                int ib = static_cast<unsigned char>(b) & 127;
                if (count[ia] != count[ib]) return count[ia] > count[ib];
                if (total[ia] != total[ib]) return total[ia] < total[ib];
                if ((a != EMPTY) != (b != EMPTY)) return a != EMPTY;
                return a > b;
            };
            for (int r = r0; r < r1; r++) {
                for (int c = c0; c < c1; c++) {
                    char ch = source[r * from + c];
                    if (first || better(ch, best)) {
                        best = ch;
                        first = false;
                    }
                }
            }
            out[row * size + col] = best;
        }
    }
    return out;
}

std::optional<std::string> avatar::parseDrawingCode(std::string_view text) {
    std::string code;
    for (char c : text) {
        if (c == EMPTY || slotIndex(c) >= 0) code += c;
    }
    for (int size : GRID_SIZES) {
        if (code.size() == static_cast<size_t>(size * size)) return code;
    }
    return std::nullopt;
}

int avatar::iconCount() {
    int count = GameManager::get()->countForType(IconType::Cube);
    // Fallback in case the game reports nothing (2.2 has 485 cubes)
    return count > 0 ? count : 485;
}

ccColor3B avatar::gdColor(int index) {
    return GameManager::get()->colorForIdx(std::clamp(index, 0, COLOR_COUNT - 1));
}

std::vector<int> avatar::pickerOrder() {
    struct Entry {
        int index;
        float hue;
        float saturation;
        float value;
    };
    std::vector<Entry> colors;
    std::vector<Entry> grays;
    for (int i = 0; i < COLOR_COUNT; i++) {
        auto c = gdColor(i);
        float r = c.r / 255.f, g = c.g / 255.f, b = c.b / 255.f;
        float max = std::max({ r, g, b });
        float min = std::min({ r, g, b });
        float delta = max - min;
        float hue = 0.f;
        if (delta > 0.f) {
            if (max == r) hue = std::fmod((g - b) / delta, 6.f);
            else if (max == g) hue = (b - r) / delta + 2.f;
            else hue = (r - g) / delta + 4.f;
            hue *= 60.f;
            if (hue < 0.f) hue += 360.f;
        }
        Entry entry { i, hue, max > 0.f ? delta / max : 0.f, max };
        (entry.saturation < .15f ? grays : colors).push_back(entry);
    }
    // Twelve hue bands, bright to dark inside each band
    std::stable_sort(colors.begin(), colors.end(), [](Entry const& a, Entry const& b) {
        int bandA = static_cast<int>(a.hue / 30.f + .5f) % 12;
        int bandB = static_cast<int>(b.hue / 30.f + .5f) % 12;
        if (bandA != bandB) return bandA < bandB;
        return a.value * (2.f - a.saturation) > b.value * (2.f - b.saturation);
    });
    std::stable_sort(grays.begin(), grays.end(), [](Entry const& a, Entry const& b) {
        return a.value > b.value;
    });

    std::vector<int> order;
    order.reserve(COLOR_COUNT);
    for (auto const& e : colors) order.push_back(e.index);
    for (auto const& e : grays) order.push_back(e.index);
    return order;
}

CCSprite* avatar::createDrawing(std::string_view pixels, float size) {
    auto normalized = normalizePixels(pixels);
    int grid = gridOf(normalized);
    // Textures stay power-of-two sized for older phones; the drawing sits in
    // the top-left corner and the sprite only shows that part
    int side = 8;
    while (side < grid) side *= 2;
    std::vector<GLubyte> data(static_cast<size_t>(side * side * 4), 0);
    for (int row = 0; row < grid; row++) {
        for (int col = 0; col < grid; col++) {
            int slot = slotIndex(normalized[row * grid + col]);
            if (slot < 0) continue;
            auto color = PALETTE[slot];
            size_t at = static_cast<size_t>((row * side + col) * 4);
            data[at + 0] = color.r;
            data[at + 1] = color.g;
            data[at + 2] = color.b;
            data[at + 3] = 255;
        }
    }

    auto texture = new CCTexture2D();
    if (!texture->initWithData(data.data(), kCCTexture2DPixelFormat_RGBA8888, side, side, CCSize(grid, grid))) {
        texture->release();
        return nullptr;
    }
    // Nearest-neighbour scaling keeps the pixels sharp
    texture->setAliasTexParameters();
    auto sprite = CCSprite::createWithTexture(texture);
    texture->release();
    if (!sprite) return nullptr;

    // Size from the sprite itself: on high-res texture settings a texture
    // pixel is smaller than one unit
    float width = sprite->getContentSize().width;
    sprite->setScale(width > 0.f ? size / width : 1.f);
    return sprite;
}

CCNode* avatar::create(AvatarLook const& look, float size) {
    if (look.mode == AvatarMode::Off) return nullptr;
    if (look.mode == AvatarMode::Drawn) {
        // A pixel drawing fills its whole square, so draw it a bit smaller
        // than a cube (which has empty corners) to look the same size
        return createDrawing(look.pixels, size * .9f);
    }

    auto gm = GameManager::get();
    bool mine = look.mode == AvatarMode::Player;
    int icon = std::clamp(mine ? gm->getPlayerFrame() : look.icon, 1, iconCount());

    auto player = SimplePlayer::create(icon);
    if (!player) return nullptr;
    player->updatePlayerFrame(icon, IconType::Cube);
    player->setColors(
        mine ? gm->colorForIdx(gm->getPlayerColor()) : gdColor(look.color1),
        mine ? gm->colorForIdx(gm->getPlayerColor2()) : gdColor(look.color2)
    );
    bool glow = mine ? gm->getPlayerGlow() : look.glow;
    if (glow) {
        player->setGlowOutline(mine ? gm->colorForIdx(gm->getPlayerGlowColor()) : gdColor(look.glowColor));
    }
    player->setScale(size / ICON_UNITS * .9f);
    return player;
}

char const* avatar::modeName(AvatarMode mode) {
    switch (mode) {
        case AvatarMode::Off: return "None";
        case AvatarMode::Drawn: return "Drawing";
        case AvatarMode::Player: return "My Icon";
        case AvatarMode::Icon: default: return "Icon Kit";
    }
}

Result<AvatarLook> matjson::Serialize<AvatarLook>::fromJson(matjson::Value const& value) {
    AvatarLook look;
    // Older versions stored just the mode, as "AI Icon" / "My Icon" / "None"
    if (value.isString()) {
        look.mode = parseMode(value.asString().unwrapOr(""));
        return Ok(look);
    }
    if (!value.isObject()) return Err("Avatar must be an object");

    look.mode = parseMode(value["mode"].asString().unwrapOr("icon"));
    look.icon = readInt(value, "icon", look.icon, 1, 10000);
    look.color1 = readInt(value, "color1", look.color1, 0, avatar::COLOR_COUNT - 1);
    look.color2 = readInt(value, "color2", look.color2, 0, avatar::COLOR_COUNT - 1);
    look.glow = value["glow"].asBool().unwrapOr(false);
    look.glowColor = readInt(value, "glow-color", look.glowColor, 0, avatar::COLOR_COUNT - 1);
    auto pixels = value["pixels"].asString().unwrapOr("");
    look.pixels = pixels.empty() ? "" : avatar::normalizePixels(pixels);
    return Ok(look);
}

matjson::Value matjson::Serialize<AvatarLook>::toJson(AvatarLook const& look) {
    auto obj = matjson::Value::object();
    obj["mode"] = modeKey(look.mode);
    obj["icon"] = look.icon;
    obj["color1"] = look.color1;
    obj["color2"] = look.color2;
    obj["glow"] = look.glow;
    obj["glow-color"] = look.glowColor;
    obj["pixels"] = look.pixels;
    return obj;
}
