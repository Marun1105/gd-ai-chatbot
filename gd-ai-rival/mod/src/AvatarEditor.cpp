#include "AvatarEditor.hpp"
#include "Profile.hpp"

#include <algorithm>
#include <array>

using namespace geode::prelude;

namespace {
    constexpr float WIDTH = 470.f;
    constexpr float HEIGHT = 290.f;

    // Preview panel on the left, editing area on the right (centers, in the popup)
    const CCPoint LEFT_CENTER { -158.f, -2.f };
    const CCSize LEFT_SIZE { 136.f, 220.f };
    const CCPoint RIGHT_CENTER { 69.f, -2.f };
    const CCSize RIGHT_SIZE { 300.f, 220.f };

    constexpr int CUBE_COLUMNS = 7;
    constexpr int CUBE_ROWS = 4;
    constexpr int CUBES_PER_PAGE = CUBE_COLUMNS * CUBE_ROWS;
    constexpr float CUBE_CELL = 38.f;

    constexpr int COLOR_COLUMNS = 13;
    constexpr float COLOR_SPACING = 20.f;
    constexpr float COLOR_SIZE = 17.f;
    constexpr float COLOR_TOP = 74.f;
    // Tag for the "no glow" cell in the glow grid
    constexpr int NO_GLOW = -1;

    constexpr float CANVAS_CELL = 10.f;
    const CCPoint CANVAS_CENTER { -62.f, 4.f };
    constexpr int PALETTE_COLUMNS = 3;
    constexpr float PALETTE_SPACING = 19.f;
    constexpr float PALETTE_SIZE = 16.f;
    const CCPoint PALETTE_TOP_LEFT { 40.f, 80.f };
    constexpr float TOOLS_X = 118.f;

    constexpr char const* ON_BG = "GJ_button_01.png";
    constexpr char const* OFF_BG = "GJ_button_04.png";
    constexpr ccColor3B GRAY { 170, 170, 170 };

    ButtonSprite* fixedSprite(char const* text, int width, bool active, float scale) {
        auto sprite = ButtonSprite::create(text, width, width, .7f, true, "bigFont.fnt", active ? ON_BG : OFF_BG);
        sprite->setScale(scale);
        return sprite;
    }

    CCMenuItemSpriteExtra* smallButton(char const* text, char const* bg, float scale, CCObject* target, SEL_MenuHandler selector) {
        auto sprite = ButtonSprite::create(text, "goldFont.fnt", bg, .8f);
        sprite->setScale(scale);
        return CCMenuItemSpriteExtra::create(sprite, target, selector);
    }

    CCLayerColor* square(ccColor4B color, float size) {
        auto layer = CCLayerColor::create(color, size, size);
        layer->ignoreAnchorPointForPosition(false);
        layer->setAnchorPoint({ .5f, .5f });
        return layer;
    }

    // A color square with a dark rim, centered in a node of the same size
    CCNode* swatch(ccColor3B color, float size) {
        auto node = CCNode::create();
        node->setContentSize({ size, size });
        node->setAnchorPoint({ .5f, .5f });
        auto rim = square({ 0, 0, 0, 200 }, size);
        rim->setPosition({ size / 2, size / 2 });
        node->addChild(rim);
        auto fill = square({ color.r, color.g, color.b, 255 }, size - 2.f);
        fill->setPosition({ size / 2, size / 2 });
        node->addChild(fill);
        return node;
    }

    // Checkerboard square with a red slash: "nothing" (eraser, no glow)
    CCNode* emptySwatch(float size) {
        auto node = CCNode::create();
        node->setContentSize({ size, size });
        node->setAnchorPoint({ .5f, .5f });
        auto rim = square({ 0, 0, 0, 200 }, size);
        rim->setPosition({ size / 2, size / 2 });
        node->addChild(rim);
        auto draw = CCDrawNode::create();
        float inner = size - 2.f;
        float half = inner / 2;
        for (int y = 0; y < 2; y++) {
            for (int x = 0; x < 2; x++) {
                bool light = (x + y) % 2 == 0;
                auto c = light ? ccc4f(.75f, .75f, .75f, 1.f) : ccc4f(.5f, .5f, .5f, 1.f);
                CCPoint from { 1.f + x * half, 1.f + y * half };
                draw->drawRect(from, from + CCPoint(half, half), c, 0.f, c);
            }
        }
        draw->drawSegment({ 2.5f, 2.5f }, { size - 2.5f, size - 2.5f }, 1.f, ccc4f(.9f, .15f, .15f, 1.f));
        node->addChild(draw);
        return node;
    }

    CCMenu* plainMenu(CCSize size) {
        auto menu = CCMenu::create();
        menu->setContentSize(size);
        menu->ignoreAnchorPointForPosition(false);
        menu->setAnchorPoint({ .5f, .5f });
        return menu;
    }
}

// ---- PixelCanvas ----

PixelCanvas* PixelCanvas::create(float cellSize, std::string const& pixels) {
    auto ret = new PixelCanvas();
    if (ret->init(cellSize, pixels)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool PixelCanvas::init(float cellSize, std::string const& pixels) {
    if (!CCLayer::init()) return false;

    m_cell = cellSize;
    m_pixels = avatar::normalizePixels(pixels);
    float size = cellSize * avatar::GRID;
    this->setContentSize({ size, size });
    this->ignoreAnchorPointForPosition(false);
    this->setAnchorPoint({ .5f, .5f });

    // Checkerboard shows which pixels are see-through
    auto checker = CCDrawNode::create();
    float check = cellSize * 2;
    for (int y = 0; y < avatar::GRID / 2; y++) {
        for (int x = 0; x < avatar::GRID / 2; x++) {
            auto c = (x + y) % 2 == 0 ? ccc4f(.24f, .24f, .28f, 1.f) : ccc4f(.17f, .17f, .2f, 1.f);
            CCPoint from { x * check, y * check };
            checker->drawRect(from, from + CCPoint(check, check), c, 0.f, c);
        }
    }
    this->addChild(checker, 0);

    m_guides = CCDrawNode::create();
    this->addChild(m_guides, 2);

    this->redraw();
    this->drawGuides();
    this->setTouchEnabled(true);
    return true;
}

void PixelCanvas::registerWithTouchDispatcher() {
    // The popup moves this above itself once it's shown (handleTouchPriority)
    CCTouchDispatcher::get()->addTargetedDelegate(this, 0, true);
}

void PixelCanvas::redraw() {
    if (m_sprite) m_sprite->removeFromParent();
    float size = m_cell * avatar::GRID;
    m_sprite = avatar::createDrawing(m_pixels, size);
    if (m_sprite) {
        m_sprite->setPosition({ size / 2, size / 2 });
        this->addChild(m_sprite, 1);
    }
}

void PixelCanvas::drawGuides() {
    m_guides->clear();
    float size = m_cell * avatar::GRID;
    auto line = ccc4f(1.f, 1.f, 1.f, .10f);
    for (int i = 1; i < avatar::GRID; i++) {
        float p = i * m_cell;
        m_guides->drawSegment({ p, 0.f }, { p, size }, .25f, line);
        m_guides->drawSegment({ 0.f, p }, { size, p }, .25f, line);
    }
    if (m_mirror) {
        m_guides->drawSegment({ size / 2, 0.f }, { size / 2, size }, .6f, ccc4f(.4f, .85f, 1.f, .8f));
    }
    // Outline
    auto edge = ccc4f(0.f, 0.f, 0.f, .8f);
    m_guides->drawSegment({ 0.f, 0.f }, { size, 0.f }, .8f, edge);
    m_guides->drawSegment({ 0.f, size }, { size, size }, .8f, edge);
    m_guides->drawSegment({ 0.f, 0.f }, { 0.f, size }, .8f, edge);
    m_guides->drawSegment({ size, 0.f }, { size, size }, .8f, edge);
}

void PixelCanvas::setMirror(bool mirror) {
    m_mirror = mirror;
    this->drawGuides();
}

void PixelCanvas::setPixels(std::string const& pixels, bool remember) {
    auto normalized = avatar::normalizePixels(pixels);
    if (normalized == m_pixels) return;
    if (remember) {
        m_history.push_back(m_pixels);
        if (m_history.size() > 40) m_history.erase(m_history.begin());
    }
    m_pixels = normalized;
    this->redraw();
    this->drawGuides();
    if (m_onChange) m_onChange(m_pixels);
}

void PixelCanvas::undo() {
    if (m_history.empty()) return;
    m_pixels = m_history.back();
    m_history.pop_back();
    this->redraw();
    if (m_onChange) m_onChange(m_pixels);
}

bool PixelCanvas::setPixel(int row, int col, char value) {
    bool changed = false;
    auto put = [&](int c) {
        auto& pixel = m_pixels[row * avatar::GRID + c];
        if (pixel != value) {
            pixel = value;
            changed = true;
        }
    };
    put(col);
    if (m_mirror) put(avatar::GRID - 1 - col);
    return changed;
}

bool PixelCanvas::fill(int row, int col, char value) {
    char target = m_pixels[row * avatar::GRID + col];
    if (target == value) return false;
    std::vector<int> stack { row * avatar::GRID + col };
    while (!stack.empty()) {
        int index = stack.back();
        stack.pop_back();
        if (m_pixels[index] != target) continue;
        m_pixels[index] = value;
        int r = index / avatar::GRID;
        int c = index % avatar::GRID;
        if (r > 0) stack.push_back(index - avatar::GRID);
        if (r < avatar::GRID - 1) stack.push_back(index + avatar::GRID);
        if (c > 0) stack.push_back(index - 1);
        if (c < avatar::GRID - 1) stack.push_back(index + 1);
    }
    return true;
}

bool PixelCanvas::paintAt(CCPoint local) {
    float size = m_cell * avatar::GRID;
    if (local.x < 0.f || local.y < 0.f || local.x >= size || local.y >= size) return false;
    int col = static_cast<int>(local.x / m_cell);
    // Row 0 is the top
    int row = avatar::GRID - 1 - static_cast<int>(local.y / m_cell);
    char value = avatar::slotChar(m_slot);

    bool changed = m_tool == Tool::Fill ? fill(row, col, value) : setPixel(row, col, value);
    if (changed) {
        this->redraw();
        if (m_onChange) m_onChange(m_pixels);
    }
    return true;
}

bool PixelCanvas::ccTouchBegan(CCTouch* touch, CCEvent*) {
    auto local = this->convertTouchToNodeSpace(touch);
    float size = m_cell * avatar::GRID;
    if (!this->isVisible() || local.x < 0.f || local.y < 0.f || local.x >= size || local.y >= size) return false;
    m_strokeStart = m_pixels;
    this->paintAt(local);
    return true;
}

void PixelCanvas::ccTouchMoved(CCTouch* touch, CCEvent*) {
    if (m_tool == Tool::Pen) this->paintAt(this->convertTouchToNodeSpace(touch));
}

void PixelCanvas::ccTouchEnded(CCTouch*, CCEvent*) {
    // One undo step per stroke
    if (!m_strokeStart.empty() && m_strokeStart != m_pixels) {
        m_history.push_back(m_strokeStart);
        if (m_history.size() > 40) m_history.erase(m_history.begin());
    }
    m_strokeStart.clear();
}

void PixelCanvas::ccTouchCancelled(CCTouch* touch, CCEvent* event) {
    this->ccTouchEnded(touch, event);
}

// ---- AvatarEditor ----

AvatarEditor* AvatarEditor::create(AvatarLook look, SaveCallback onSave) {
    auto ret = new AvatarEditor();
    if (ret->init(std::move(look), std::move(onSave))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool AvatarEditor::init(AvatarLook look, SaveCallback onSave) {
    if (!Popup::init(WIDTH, HEIGHT)) return false;

    m_look = look;
    m_original = look;
    m_onSave = std::move(onSave);
    m_colorOrder = avatar::pickerOrder();
    m_page = std::clamp(m_look.icon - 1, 0, avatar::iconCount() - 1) / CUBES_PER_PAGE;

    this->setTitle("Avatar Editor");

    // ---- Preview panel ----
    auto panel = NineSlice::create("square02b_001.png");
    panel->setColor({ 0, 0, 0 });
    panel->setOpacity(70);
    panel->setScale(.5f);
    panel->setContentSize(LEFT_SIZE / .5f);
    m_mainLayer->addChildAtPosition(panel, Anchor::Center, LEFT_CENTER);

    auto previewTitle = CCLabelBMFont::create("Preview", "goldFont.fnt");
    previewTitle->setScale(.45f);
    m_mainLayer->addChildAtPosition(previewTitle, Anchor::Center, LEFT_CENTER + CCPoint(0, 96));

    m_previewHolder = CCNode::create();
    m_mainLayer->addChildAtPosition(m_previewHolder, Anchor::Center, LEFT_CENTER + CCPoint(0, 58));

    m_bubbleHolder = CCNode::create();
    m_mainLayer->addChildAtPosition(m_bubbleHolder, Anchor::Center, LEFT_CENTER + CCPoint(0, 8));

    auto styleTitle = CCLabelBMFont::create("Style", "goldFont.fnt");
    styleTitle->setScale(.45f);
    m_mainLayer->addChildAtPosition(styleTitle, Anchor::Center, LEFT_CENTER + CCPoint(0, -36));

    auto modeMenu = plainMenu({ LEFT_SIZE.width, 60.f });
    m_mainLayer->addChildAtPosition(modeMenu, Anchor::Center, LEFT_CENTER + CCPoint(0, -72));
    constexpr std::array MODES { AvatarMode::Icon, AvatarMode::Drawn, AvatarMode::Player, AvatarMode::Off };
    m_modeSprites.resize(MODES.size());
    for (size_t i = 0; i < MODES.size(); i++) {
        auto sprite = fixedSprite(avatar::modeName(MODES[i]), 120, MODES[i] == m_look.mode, .5f);
        auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(AvatarEditor::onMode));
        button->setTag(static_cast<int>(MODES[i]));
        float x = i % 2 == 0 ? -33.f : 33.f;
        float y = i < 2 ? 13.f : -13.f;
        modeMenu->addChildAtPosition(button, Anchor::Center, ccp(x, y));
        m_modeSprites[static_cast<size_t>(MODES[i])] = sprite;
    }

    // ---- Editing area ----
    m_right = CCNode::create();
    m_right->setContentSize(RIGHT_SIZE);
    m_right->setAnchorPoint({ .5f, .5f });
    m_mainLayer->addChildAtPosition(m_right, Anchor::Center, RIGHT_CENTER);

    auto bottom = plainMenu({ 240.f, 30.f });
    m_mainLayer->addChildAtPosition(bottom, Anchor::Bottom, ccp(0, 18));
    bottom->addChild(smallButton("Cancel", "GJ_button_06.png", .62f, this, menu_selector(AvatarEditor::onClose)));
    bottom->addChild(smallButton("Save", "GJ_button_01.png", .62f, this, menu_selector(AvatarEditor::onSave)));
    bottom->setLayout(RowLayout::create()->setGap(12.f));

    this->rebuildRight();
    this->refreshPreview();
    return true;
}

void AvatarEditor::setMode(AvatarMode mode) {
    m_look.mode = mode;
    if (mode == AvatarMode::Drawn && m_look.pixels.empty()) {
        m_look.pixels = avatar::starterDrawing();
    }
    for (size_t i = 0; i < m_modeSprites.size(); i++) {
        if (m_modeSprites[i]) m_modeSprites[i]->updateBGImage(i == static_cast<size_t>(mode) ? ON_BG : OFF_BG);
    }
    this->rebuildRight();
    this->refreshPreview();
}

void AvatarEditor::rebuildRight() {
    m_right->removeAllChildren();
    m_gridMenu = nullptr;
    m_highlight = nullptr;
    m_pageLabel = nullptr;
    m_canvas = nullptr;
    m_slotHighlight = nullptr;
    m_paletteMenu = nullptr;
    m_penSprite = m_fillSprite = m_mirrorSprite = nullptr;
    m_tabSprites.clear();

    switch (m_look.mode) {
        case AvatarMode::Icon: this->buildIconKit(); break;
        case AvatarMode::Drawn: this->buildDrawing(); break;
        case AvatarMode::Player:
            this->buildInfo(
                "Your Icon",
                "The AI uses your own cube, colors and glow from the icon kit, like a mirror rival. "
                "It changes whenever you change yours.",
                true
            );
            break;
        case AvatarMode::Off:
            this->buildInfo("No Avatar", "The bubble shows just the AI's name and its reply.", false);
            break;
    }
    // New buttons and the canvas need to sit above the popup for touches
    handleTouchPriority(this);
}

// ---- Icon kit ----

void AvatarEditor::buildIconKit() {
    auto tabs = plainMenu({ RIGHT_SIZE.width, 24.f });
    m_right->addChildAtPosition(tabs, Anchor::Center, ccp(0, 98));
    constexpr std::array<char const*, 4> NAMES { "Cube", "Color 1", "Color 2", "Glow" };
    for (int i = 0; i < 4; i++) {
        auto sprite = fixedSprite(NAMES[i], 110, static_cast<int>(m_tab) == i, .5f);
        auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(AvatarEditor::onTab));
        button->setTag(i);
        tabs->addChildAtPosition(button, Anchor::Center, ccp(-108.f + i * 72.f, 0));
        m_tabSprites.push_back(sprite);
    }

    if (m_tab == Tab::Cube) this->buildCubePage();
    else this->buildColorGrid();
}

void AvatarEditor::buildCubePage() {
    int count = avatar::iconCount();
    int pages = (count + CUBES_PER_PAGE - 1) / CUBES_PER_PAGE;
    m_page = std::clamp(m_page, 0, pages - 1);

    m_gridMenu = plainMenu({ CUBE_COLUMNS * CUBE_CELL, CUBE_ROWS * CUBE_CELL });
    m_right->addChildAtPosition(m_gridMenu, Anchor::Center, ccp(0, -2));

    // Highlight sits behind the selected cube
    auto highlight = NineSlice::create("square02b_001.png");
    highlight->setColor({ 255, 255, 255 });
    highlight->setOpacity(70);
    highlight->setScale(.5f);
    highlight->setContentSize(CCSize(CUBE_CELL - 2.f, CUBE_CELL - 2.f) / .5f);
    m_gridMenu->addChild(highlight, -1);
    m_highlight = highlight;

    for (int i = 0; i < CUBES_PER_PAGE; i++) {
        int icon = m_page * CUBES_PER_PAGE + i + 1;
        if (icon > count) break;

        auto cell = CCNode::create();
        cell->setContentSize({ CUBE_CELL - 4.f, CUBE_CELL - 4.f });
        cell->setAnchorPoint({ .5f, .5f });
        auto player = SimplePlayer::create(icon);
        player->updatePlayerFrame(icon, IconType::Cube);
        player->setColors(avatar::gdColor(m_look.color1), avatar::gdColor(m_look.color2));
        player->setScale(.85f);
        player->setPosition(cell->getContentSize() / 2);
        cell->addChild(player);

        auto button = CCMenuItemSpriteExtra::create(cell, this, menu_selector(AvatarEditor::onCube));
        button->setTag(icon);
        int col = i % CUBE_COLUMNS;
        int row = i / CUBE_COLUMNS;
        button->setPosition({ (col + .5f) * CUBE_CELL, (CUBE_ROWS - row - .5f) * CUBE_CELL });
        m_gridMenu->addChild(button);
    }

    auto arrows = plainMenu({ RIGHT_SIZE.width + 10.f, 40.f });
    m_right->addChildAtPosition(arrows, Anchor::Center, ccp(0, -2));
    auto prev = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    prev->setScale(.45f);
    auto prevButton = CCMenuItemSpriteExtra::create(prev, this, menu_selector(AvatarEditor::onPage));
    prevButton->setTag(-1);
    arrows->addChildAtPosition(prevButton, Anchor::Left, ccp(8, 0));
    auto next = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    next->setFlipX(true);
    next->setScale(.45f);
    auto nextButton = CCMenuItemSpriteExtra::create(next, this, menu_selector(AvatarEditor::onPage));
    nextButton->setTag(1);
    arrows->addChildAtPosition(nextButton, Anchor::Right, ccp(-8, 0));

    m_pageLabel = CCLabelBMFont::create(fmt::format("Page {} / {}", m_page + 1, pages).c_str(), "goldFont.fnt");
    m_pageLabel->setScale(.4f);
    m_right->addChildAtPosition(m_pageLabel, Anchor::Center, ccp(0, -94));

    // Jump five pages at a time: there are a lot of cubes
    auto jumps = plainMenu({ 150.f, 20.f });
    m_right->addChildAtPosition(jumps, Anchor::Center, ccp(0, -94));
    for (int step : { -5, 5 }) {
        auto label = CCLabelBMFont::create(step < 0 ? "<<" : ">>", "bigFont.fnt");
        label->setScale(.4f);
        auto button = CCMenuItemSpriteExtra::create(label, this, menu_selector(AvatarEditor::onPage));
        button->setTag(step);
        jumps->addChildAtPosition(button, step < 0 ? Anchor::Left : Anchor::Right, ccp(step < 0 ? 12 : -12, 0));
    }

    this->moveCubeHighlight();
}

void AvatarEditor::moveCubeHighlight() {
    if (!m_highlight) return;
    int index = m_look.icon - 1 - m_page * CUBES_PER_PAGE;
    bool here = index >= 0 && index < CUBES_PER_PAGE;
    m_highlight->setVisible(here);
    if (!here) return;
    int col = index % CUBE_COLUMNS;
    int row = index / CUBE_COLUMNS;
    m_highlight->setPosition({ (col + .5f) * CUBE_CELL, (CUBE_ROWS - row - .5f) * CUBE_CELL });
}

int AvatarEditor::selectedColor() const {
    switch (m_tab) {
        case Tab::Color1: return m_look.color1;
        case Tab::Color2: return m_look.color2;
        case Tab::Glow: return m_look.glow ? m_look.glowColor : NO_GLOW;
        default: return m_look.color1;
    }
}

void AvatarEditor::buildColorGrid() {
    // The glow grid starts with an "off" cell
    std::vector<int> cells;
    if (m_tab == Tab::Glow) cells.push_back(NO_GLOW);
    cells.insert(cells.end(), m_colorOrder.begin(), m_colorOrder.end());

    int rows = (static_cast<int>(cells.size()) + COLOR_COLUMNS - 1) / COLOR_COLUMNS;
    m_gridMenu = plainMenu({ COLOR_COLUMNS * COLOR_SPACING, rows * COLOR_SPACING });
    m_gridMenu->setAnchorPoint({ .5f, 1.f });
    m_right->addChildAtPosition(m_gridMenu, Anchor::Center, ccp(0, COLOR_TOP + COLOR_SPACING / 2));

    auto highlight = square({ 255, 255, 255, 255 }, COLOR_SIZE + 4.f);
    m_gridMenu->addChild(highlight, -1);
    m_highlight = highlight;

    for (size_t i = 0; i < cells.size(); i++) {
        int index = cells[i];
        auto node = index == NO_GLOW ? emptySwatch(COLOR_SIZE) : swatch(avatar::gdColor(index), COLOR_SIZE);
        auto button = CCMenuItemSpriteExtra::create(node, this, menu_selector(AvatarEditor::onColor));
        button->setTag(index);
        button->setSizeMult(1.2f);
        int col = static_cast<int>(i) % COLOR_COLUMNS;
        int row = static_cast<int>(i) / COLOR_COLUMNS;
        button->setPosition({ (col + .5f) * COLOR_SPACING, (rows - row - .5f) * COLOR_SPACING });
        m_gridMenu->addChild(button);
    }

    if (m_tab == Tab::Glow) {
        auto note = CCLabelBMFont::create("Pick a color to turn the glow on", "chatFont.fnt");
        note->setScale(.5f);
        note->setColor(GRAY);
        m_right->addChildAtPosition(note, Anchor::Center, ccp(0, -100));
    }
    this->moveColorHighlight();
}

void AvatarEditor::moveColorHighlight() {
    if (!m_highlight || !m_gridMenu) return;
    int selected = this->selectedColor();
    m_highlight->setVisible(false);
    for (auto child : CCArrayExt<CCNode*>(m_gridMenu->getChildren())) {
        if (child == m_highlight || child->getTag() != selected) continue;
        if (!typeinfo_cast<CCMenuItemSpriteExtra*>(child)) continue;
        m_highlight->setPosition(child->getPosition());
        m_highlight->setVisible(true);
        break;
    }
}

// ---- Drawing ----

void AvatarEditor::buildDrawing() {
    auto title = CCLabelBMFont::create("Tap or drag to draw", "chatFont.fnt");
    title->setScale(.5f);
    title->setColor(GRAY);
    m_right->addChildAtPosition(title, Anchor::Center, CANVAS_CENTER + CCPoint(0, 94));

    m_canvas = PixelCanvas::create(CANVAS_CELL, m_look.pixels);
    m_canvas->m_onChange = [this](std::string const& pixels) {
        m_look.pixels = pixels;
        this->refreshPreview();
    };
    m_right->addChildAtPosition(m_canvas, Anchor::Center, CANVAS_CENTER);

    // Palette: eraser first, then the colors
    auto palette = avatar::drawPalette();
    int slots = static_cast<int>(palette.size()) + 1;
    int rows = (slots + PALETTE_COLUMNS - 1) / PALETTE_COLUMNS;
    m_paletteMenu = plainMenu({ PALETTE_COLUMNS * PALETTE_SPACING, rows * PALETTE_SPACING });
    m_paletteMenu->setAnchorPoint({ 0.f, 1.f });
    m_right->addChildAtPosition(
        m_paletteMenu, Anchor::Center,
        PALETTE_TOP_LEFT + CCPoint(-PALETTE_SPACING / 2, PALETTE_SPACING / 2)
    );

    m_slotHighlight = square({ 255, 255, 255, 255 }, PALETTE_SIZE + 4.f);
    m_paletteMenu->addChild(m_slotHighlight, -1);

    for (int i = 0; i < slots; i++) {
        int slot = i - 1;
        auto node = slot < 0 ? emptySwatch(PALETTE_SIZE) : swatch(palette[slot], PALETTE_SIZE);
        auto button = CCMenuItemSpriteExtra::create(node, this, menu_selector(AvatarEditor::onSlot));
        button->setTag(slot);
        button->setSizeMult(1.2f);
        int col = i % PALETTE_COLUMNS;
        int row = i / PALETTE_COLUMNS;
        button->setPosition({ (col + .5f) * PALETTE_SPACING, (rows - row - .5f) * PALETTE_SPACING });
        m_paletteMenu->addChild(button);
    }

    // Tool column
    auto tools = plainMenu({ 60.f, 200.f });
    m_right->addChildAtPosition(tools, Anchor::Center, ccp(TOOLS_X, 0));
    m_penSprite = fixedSprite("Pen", 100, true, .45f);
    auto pen = CCMenuItemSpriteExtra::create(m_penSprite, this, menu_selector(AvatarEditor::onTool));
    pen->setTag(static_cast<int>(PixelCanvas::Tool::Pen));
    tools->addChildAtPosition(pen, Anchor::Center, ccp(0, 74));
    m_fillSprite = fixedSprite("Fill", 100, false, .45f);
    auto fillButton = CCMenuItemSpriteExtra::create(m_fillSprite, this, menu_selector(AvatarEditor::onTool));
    fillButton->setTag(static_cast<int>(PixelCanvas::Tool::Fill));
    tools->addChildAtPosition(fillButton, Anchor::Center, ccp(0, 52));
    m_mirrorSprite = fixedSprite("Mirror", 100, false, .45f);
    tools->addChildAtPosition(
        CCMenuItemSpriteExtra::create(m_mirrorSprite, this, menu_selector(AvatarEditor::onMirror)),
        Anchor::Center, ccp(0, 30)
    );

    auto codeLabel = CCLabelBMFont::create("Code", "goldFont.fnt");
    codeLabel->setScale(.4f);
    m_right->addChildAtPosition(codeLabel, Anchor::Center, ccp(TOOLS_X, -24));
    tools->addChildAtPosition(
        smallButton("Copy", "GJ_button_05.png", .45f, this, menu_selector(AvatarEditor::onCopyCode)),
        Anchor::Center, ccp(0, -42)
    );
    tools->addChildAtPosition(
        smallButton("Paste", "GJ_button_05.png", .45f, this, menu_selector(AvatarEditor::onPasteCode)),
        Anchor::Center, ccp(0, -64)
    );

    // Under the canvas
    auto actions = plainMenu({ 170.f, 24.f });
    m_right->addChildAtPosition(actions, Anchor::Center, CANVAS_CENTER + CCPoint(0, -98));
    actions->addChild(smallButton("Undo", "GJ_button_04.png", .45f, this, menu_selector(AvatarEditor::onUndo)));
    actions->addChild(smallButton("Clear", "GJ_button_06.png", .45f, this, menu_selector(AvatarEditor::onClearCanvas)));
    actions->addChild(smallButton("Face", "GJ_button_04.png", .45f, this, menu_selector(AvatarEditor::onStarter)));
    actions->setLayout(RowLayout::create()->setGap(5.f));

    this->refreshToolButtons();
}

void AvatarEditor::refreshToolButtons() {
    if (!m_canvas) return;
    if (m_penSprite) m_penSprite->updateBGImage(m_canvas->m_tool == PixelCanvas::Tool::Pen ? ON_BG : OFF_BG);
    if (m_fillSprite) m_fillSprite->updateBGImage(m_canvas->m_tool == PixelCanvas::Tool::Fill ? ON_BG : OFF_BG);
    if (m_mirrorSprite) m_mirrorSprite->updateBGImage(m_canvas->m_mirror ? ON_BG : OFF_BG);

    if (m_slotHighlight && m_paletteMenu) {
        for (auto child : CCArrayExt<CCNode*>(m_paletteMenu->getChildren())) {
            if (child == m_slotHighlight || child->getTag() != m_canvas->m_slot) continue;
            m_slotHighlight->setPosition(child->getPosition());
            break;
        }
    }
}

// ---- Info pages ----

void AvatarEditor::buildInfo(char const* title, char const* text, bool withCopyButton) {
    auto heading = CCLabelBMFont::create(title, "goldFont.fnt");
    heading->setScale(.7f);
    m_right->addChildAtPosition(heading, Anchor::Center, ccp(0, 50));

    auto body = Label::create(text, "chatFont.fnt");
    body->setAlignment(Label::Alignment::Center);
    body->setScale(.6f);
    body->setMaxWidth((RIGHT_SIZE.width - 40.f) / .6f);
    body->setColor({ 220, 220, 220 });
    m_right->addChildAtPosition(body, Anchor::Center, ccp(0, 10));

    if (withCopyButton) {
        auto menu = plainMenu({ 200.f, 30.f });
        m_right->addChildAtPosition(menu, Anchor::Center, ccp(0, -45));
        menu->addChildAtPosition(
            smallButton("Customize A Copy", "GJ_button_05.png", .55f, this, menu_selector(AvatarEditor::onCopyMine)),
            Anchor::Center
        );
    }
}

// ---- Preview ----

void AvatarEditor::refreshPreview() {
    m_previewHolder->removeAllChildren();
    if (auto big = avatar::create(m_look, 56.f)) {
        m_previewHolder->addChild(big);
    }
    else {
        auto none = CCLabelBMFont::create("No avatar", "bigFont.fnt");
        none->setScale(.35f);
        none->setOpacity(130);
        m_previewHolder->addChild(none);
    }

    // A small copy of the in-game bubble with the player's bubble settings
    m_bubbleHolder->removeAllChildren();
    auto profile = profile::current();
    auto const& look = profile.bubble;
    constexpr float W = 126.f;
    constexpr float H = 36.f;
    constexpr float AVATAR = 22.f;

    auto bg = NineSlice::create("square02b_001.png");
    bg->setColor({ look.background.r, look.background.g, look.background.b });
    bg->setOpacity(std::max<GLubyte>(look.background.a, 90));
    bg->setScale(.3f);
    bg->setContentSize(CCSize(W, H) / .3f);
    m_bubbleHolder->addChild(bg);

    float textLeft = -W / 2 + 6.f;
    if (auto mini = avatar::create(m_look, AVATAR)) {
        mini->setPosition({ -W / 2 + 6.f + AVATAR / 2, 0.f });
        m_bubbleHolder->addChild(mini);
        textLeft += AVATAR + 5.f;
    }

    auto name = CCLabelBMFont::create(profile.name.c_str(), "goldFont.fnt");
    name->setColor(look.nameColor);
    name->setAnchorPoint({ 0.f, .5f });
    name->limitLabelWidth(W / 2 - textLeft - 4.f, .38f, .1f);
    name->setPosition({ textLeft, 7.f });
    m_bubbleHolder->addChild(name);

    auto line = CCLabelBMFont::create("Nice try. Again?", "chatFont.fnt");
    line->setColor(look.textColor);
    line->setAnchorPoint({ 0.f, .5f });
    line->limitLabelWidth(W / 2 - textLeft - 4.f, .45f, .1f);
    line->setPosition({ textLeft, -7.f });
    m_bubbleHolder->addChild(line);
}

// ---- Buttons ----

void AvatarEditor::onMode(CCObject* sender) {
    auto mode = static_cast<AvatarMode>(static_cast<CCNode*>(sender)->getTag());
    if (mode != m_look.mode) this->setMode(mode);
}

void AvatarEditor::onTab(CCObject* sender) {
    auto tab = static_cast<Tab>(static_cast<CCNode*>(sender)->getTag());
    if (tab == m_tab) return;
    m_tab = tab;
    this->rebuildRight();
}

void AvatarEditor::onCube(CCObject* sender) {
    m_look.icon = static_cast<CCNode*>(sender)->getTag();
    this->moveCubeHighlight();
    this->refreshPreview();
}

void AvatarEditor::onPage(CCObject* sender) {
    int pages = (avatar::iconCount() + CUBES_PER_PAGE - 1) / CUBES_PER_PAGE;
    m_page = (m_page + static_cast<CCNode*>(sender)->getTag() + pages) % pages;
    this->rebuildRight();
}

void AvatarEditor::onColor(CCObject* sender) {
    int index = static_cast<CCNode*>(sender)->getTag();
    switch (m_tab) {
        case Tab::Color1: m_look.color1 = index; break;
        case Tab::Color2: m_look.color2 = index; break;
        case Tab::Glow:
            m_look.glow = index != NO_GLOW;
            if (index != NO_GLOW) m_look.glowColor = index;
            break;
        default: break;
    }
    this->moveColorHighlight();
    this->refreshPreview();
}

void AvatarEditor::onSlot(CCObject* sender) {
    if (!m_canvas) return;
    m_canvas->m_slot = static_cast<CCNode*>(sender)->getTag();
    this->refreshToolButtons();
}

void AvatarEditor::onTool(CCObject* sender) {
    if (!m_canvas) return;
    m_canvas->m_tool = static_cast<PixelCanvas::Tool>(static_cast<CCNode*>(sender)->getTag());
    this->refreshToolButtons();
}

void AvatarEditor::onMirror(CCObject*) {
    if (!m_canvas) return;
    m_canvas->setMirror(!m_canvas->m_mirror);
    this->refreshToolButtons();
}

void AvatarEditor::onUndo(CCObject*) {
    if (m_canvas) m_canvas->undo();
}

void AvatarEditor::onClearCanvas(CCObject*) {
    if (m_canvas) m_canvas->setPixels(std::string(avatar::PIXEL_COUNT, avatar::EMPTY), true);
}

void AvatarEditor::onStarter(CCObject*) {
    if (m_canvas) m_canvas->setPixels(avatar::starterDrawing(), true);
}

void AvatarEditor::onCopyCode(CCObject*) {
    if (!m_canvas) return;
    utils::clipboard::write(m_canvas->getPixels());
    Notification::create("Drawing code copied", NotificationIcon::Success)->show();
}

void AvatarEditor::onPasteCode(CCObject*) {
    if (!m_canvas) return;
    std::string code;
    for (char c : utils::clipboard::read()) {
        if (c == avatar::EMPTY || avatar::slotIndex(c) >= 0) code += c;
    }
    if (code.size() != static_cast<size_t>(avatar::PIXEL_COUNT)) {
        Notification::create("The clipboard doesn't hold a drawing code", NotificationIcon::Error)->show();
        return;
    }
    m_canvas->setPixels(code, true);
}

void AvatarEditor::onCopyMine(CCObject*) {
    auto gm = GameManager::get();
    m_look.icon = std::clamp(gm->getPlayerFrame(), 1, avatar::iconCount());
    m_look.color1 = gm->getPlayerColor();
    m_look.color2 = gm->getPlayerColor2();
    m_look.glow = gm->getPlayerGlow();
    m_look.glowColor = gm->getPlayerGlowColor();
    m_page = (m_look.icon - 1) / CUBES_PER_PAGE;
    m_tab = Tab::Cube;
    this->setMode(AvatarMode::Icon);
}

void AvatarEditor::onSave(CCObject*) {
    if (m_onSave) m_onSave(m_look);
    Popup::onClose(nullptr);
}

void AvatarEditor::onClose(CCObject*) {
    if (m_look == m_original) {
        Popup::onClose(nullptr);
        return;
    }
    Ref<AvatarEditor> self = this;
    createQuickPopup(
        "Unsaved Avatar",
        "Close without saving your changes?",
        "Keep Editing", "Discard",
        [self](FLAlertLayer*, bool discard) {
            if (discard) self->Popup::onClose(nullptr);
        }
    );
}
