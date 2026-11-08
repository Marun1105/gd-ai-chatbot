#include "LookEditors.hpp"
#include "SpeechBubble.hpp"

#include <Geode/ui/ColorPickPopup.hpp>

#include <algorithm>
#include <array>
#include <cmath>

using namespace geode::prelude;

namespace {
    // ---- Shared ----

    constexpr char const* ON_BG = "GJ_button_01.png";
    constexpr char const* OFF_BG = "GJ_button_04.png";
    constexpr ccColor3B GREEN { 90, 230, 100 };
    constexpr ccColor3B HINT { 175, 175, 175 };
    constexpr char const* PREVIEW_TEXT = "63% again? That wave is eating you alive.";

    ButtonSprite* toggleSprite(char const* text, int width, bool active, float scale) {
        auto sprite = ButtonSprite::create(text, width, width, .7f, true, "bigFont.fnt", active ? ON_BG : OFF_BG);
        sprite->setScale(scale);
        return sprite;
    }

    CCMenuItemSpriteExtra* textButton(char const* text, char const* bg, float scale, CCObject* target, SEL_MenuHandler selector) {
        auto sprite = ButtonSprite::create(text, "goldFont.fnt", bg, .8f);
        sprite->setScale(scale);
        return CCMenuItemSpriteExtra::create(sprite, target, selector);
    }

    CCLayerColor* rect(ccColor4B color, CCSize size) {
        auto layer = CCLayerColor::create(color, size.width, size.height);
        layer->ignoreAnchorPointForPosition(false);
        layer->setAnchorPoint({ .5f, .5f });
        return layer;
    }

    // A node of `size` with a colored rim (green when selected) and `content` centered in it
    CCNode* chip(CCNode* content, CCSize size, bool selected, ccColor4B fill = { 0, 0, 0, 120 }) {
        auto node = CCNode::create();
        node->setContentSize(size);
        node->setAnchorPoint({ .5f, .5f });
        auto rim = rect(selected ? ccColor4B { GREEN.r, GREEN.g, GREEN.b, 255 } : ccColor4B { 0, 0, 0, 200 }, size);
        rim->setPosition(size / 2);
        node->addChild(rim);
        auto inner = rect(fill, size - CCSize(selected ? 4.f : 2.f, selected ? 4.f : 2.f));
        inner->setPosition(size / 2);
        node->addChild(inner);
        if (content) {
            content->setPosition(size / 2);
            node->addChild(content);
        }
        return node;
    }

    CCNode* colorSwatch(ccColor3B color, GLubyte alpha, CCSize size) {
        auto node = CCNode::create();
        node->setContentSize(size);
        node->setAnchorPoint({ .5f, .5f });
        // A checker behind shows how see-through the color is
        auto checker = CCDrawNode::create();
        float half = (size.width - 2.f) / 4;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 2; j++) {
                auto c = (i + j) % 2 == 0 ? ccc4f(.8f, .8f, .8f, 1.f) : ccc4f(.55f, .55f, .55f, 1.f);
                CCPoint from { 1.f + i * half, 1.f + j * (size.height - 2.f) / 2 };
                checker->drawRect(from, from + CCPoint(half, (size.height - 2.f) / 2), c, 0.f, c);
            }
        }
        auto rim = rect({ 0, 0, 0, 220 }, size);
        rim->setPosition(size / 2);
        node->addChild(rim);
        node->addChild(checker);
        auto fill = rect({ color.r, color.g, color.b, alpha }, size - CCSize(2.f, 2.f));
        fill->setPosition(size / 2);
        node->addChild(fill);
        return node;
    }

    CCNode* panel(CCSize size, GLubyte opacity = 75) {
        auto bg = NineSlice::create("square02b_001.png");
        bg->setColor({ 0, 0, 0 });
        bg->setOpacity(opacity);
        bg->setScale(.5f);
        bg->setContentSize(size / .5f);
        return bg;
    }

    void applyLook(Profile& p, BubbleLook const& l) {
        auto& b = p.bubble;
        b.anchor = l.anchor;
        b.offsetX = l.offsetX;
        b.offsetY = l.offsetY;
        b.width = l.width;
        b.textScale = l.textScale;
        b.font = look::fontFile(l.font);
        b.background = l.background;
        b.textColor = l.textColor;
        b.nameColor = l.nameColor;
        b.duration = l.duration;
        b.typewriter = l.typewriter;
        p.soundFile = look::soundFile(l.sound);
        p.soundVolume = static_cast<float>(l.volume) / 100.f;
    }

    // Wrap a node in a container scaled to fit `fit` (never enlarged)
    CCNode* fitted(CCNode* node, CCSize size, CCSize fit) {
        float scale = std::min({ 1.f, fit.width / std::max(size.width, 1.f), fit.height / std::max(size.height, 1.f) });
        auto frame = CCNode::create();
        frame->setContentSize(size * scale);
        frame->setAnchorPoint({ .5f, .5f });
        auto inner = CCNode::create();
        inner->setScale(scale);
        inner->setPosition(size * scale / 2);
        node->setPosition({ 0.f, 0.f });
        inner->addChild(node);
        frame->addChild(inner);
        return frame;
    }

    // A copy of the chat window at its real size, anchored at its center
    CCNode* chatMock(ChatLook const& l, Profile const& p) {
        float w = l.large ? 460.f : 380.f;
        float h = l.large ? 310.f : 250.f;
        auto node = CCNode::create();
        node->setContentSize({ w, h });
        node->setAnchorPoint({ .5f, .5f });
        node->ignoreAnchorPointForPosition(false);

        auto bg = NineSlice::create(l.customColor ? "square02b_001.png" : "GJ_square01.png");
        if (l.customColor) bg->setColor(l.color);
        bg->setContentSize({ w, h });
        bg->setPosition({ w / 2, h / 2 });
        node->addChild(bg);

        auto title = CCLabelBMFont::create(fmt::format("Chat with {}", p.name).c_str(), "goldFont.fnt");
        title->setScale(.7f);
        title->setPosition({ w / 2, h - 20.f });
        node->addChild(title);

        float historyW = w - 40.f;
        float historyH = h - 130.f;
        auto history = panel({ historyW + 12.f, historyH + 12.f }, 90);
        history->setPosition({ w / 2, h / 2 + 12.f });
        node->addChild(history);

        auto name = p.bubble.nameColor;
        bool white = name.r == 255 && name.g == 255 && name.b == 255;
        auto tag = white ? std::string("<cy>") : fmt::format("<c-{:02x}{:02x}{:02x}>", name.r, name.g, name.b);
        auto lines = Label::createRich(
            fmt::format(
                "You: how do I get past the wave?\n{}{}: Hold less. Way less.</c>\nYou: easy for you to say\n{}{}: It is. Watch and learn.</c>",
                tag, p.name, tag, p.name
            ),
            "chatFont.fnt"
        );
        lines->setAlignment(Label::Alignment::Left);
        lines->setScale(.6f);
        lines->setMaxWidth(historyW / .6f);
        lines->setAnchorPoint({ 0.f, 1.f });
        lines->setPosition({ w / 2 - historyW / 2, h / 2 + 12.f + historyH / 2 });
        node->addChild(lines);

        auto input = panel({ w - 120.f, 30.f }, 90);
        input->setPosition({ w / 2 - 45.f, 32.f });
        node->addChild(input);
        auto placeholder = CCLabelBMFont::create("Say something...", "chatFont.fnt");
        placeholder->setScale(.6f);
        placeholder->setColor({ 150, 150, 150 });
        placeholder->setPosition({ w / 2 - 45.f, 32.f });
        node->addChild(placeholder);

        auto send = ButtonSprite::create("Send", "goldFont.fnt", "GJ_button_01.png", .8f);
        send->setScale(.8f);
        send->setPosition({ w - 55.f, 32.f });
        node->addChild(send);
        return node;
    }

    struct Theme {
        ccColor4B background;
        ccColor3B text;
        ccColor3B name;
    };
    constexpr std::array<Theme, 6> THEMES {{
        { { 0, 0, 0, 170 }, { 255, 255, 255 }, { 255, 255, 255 } },     // classic
        { { 245, 240, 225, 235 }, { 40, 40, 40 }, { 210, 100, 40 } },  // paper
        { { 110, 55, 25, 230 }, { 255, 235, 200 }, { 255, 200, 90 } }, // GD brown
        { { 15, 0, 40, 215 }, { 120, 255, 255 }, { 255, 80, 255 } },   // neon
        { { 60, 10, 0, 220 }, { 255, 220, 150 }, { 255, 120, 40 } },   // lava
        { { 10, 60, 50, 215 }, { 200, 255, 230 }, { 120, 255, 180 } }, // mint
    }};

    constexpr std::array<ccColor3B, 9> CHAT_COLORS {{
        { 40, 40, 90 }, { 20, 20, 25 }, { 70, 30, 110 }, { 20, 90, 100 }, { 30, 90, 40 },
        { 120, 25, 30 }, { 150, 80, 20 }, { 150, 60, 120 }, { 80, 80, 90 },
    }};

    // Tags for the color rows
    enum ColorTarget { BACKGROUND = 0, TEXT = 1, NAME = 2 };

    // Layout of the bubble designer (offsets from the popup's center)
    constexpr float DESIGNER_W = 480.f;
    constexpr float DESIGNER_H = 300.f;
    const CCPoint CONTROLS_CENTER { -128.f, -8.f };
    constexpr float PREVIEW_X = 108.f;
    const CCSize SCREEN_FIT { 240.f, 122.f };
    constexpr float SCREEN_Y = 46.f;
    const CCSize CLOSE_UP_FIT { 236.f, 60.f };
    constexpr float CLOSE_UP_Y = -68.f;
}

// Touch layer over the small game screen: drags the bubble around
class BubbleDragArea : public CCLayer {
public:
    BubbleDesigner* m_owner = nullptr;

    static BubbleDragArea* create(BubbleDesigner* owner, CCSize size) {
        auto ret = new BubbleDragArea();
        if (ret->init(owner, size)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init(BubbleDesigner* owner, CCSize size) {
        if (!CCLayer::init()) return false;
        m_owner = owner;
        this->setContentSize(size);
        this->ignoreAnchorPointForPosition(false);
        this->setAnchorPoint({ .5f, .5f });
        this->setTouchEnabled(true);
        return true;
    }

    void registerWithTouchDispatcher() override {
        CCTouchDispatcher::get()->addTargetedDelegate(this, 0, true);
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        auto local = this->convertTouchToNodeSpace(touch);
        auto size = this->getContentSize();
        return local.x >= 0.f && local.y >= 0.f && local.x <= size.width && local.y <= size.height;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        m_owner->dragBy(touch->getLocation(), touch->getPreviousLocation());
    }

    void ccTouchEnded(CCTouch*, CCEvent*) override {
        m_owner->dragEnded();
    }

    void ccTouchCancelled(CCTouch*, CCEvent*) override {
        m_owner->dragEnded();
    }
};

// ---- Bubble designer ----

BubbleDesigner* BubbleDesigner::create(BubbleLook look, SaveCallback onSave) {
    auto ret = new BubbleDesigner();
    if (ret->init(std::move(look), std::move(onSave))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool BubbleDesigner::init(BubbleLook look, SaveCallback onSave) {
    if (!Popup::init(DESIGNER_W, DESIGNER_H)) return false;

    m_look = look::clamped(look);
    m_original = m_look;
    m_onSave = std::move(onSave);
    this->setTitle("Speech Bubble");

    // ---- Tabs and controls ----
    auto tabs = CCMenu::create();
    tabs->setContentSize({ 204.f, 22.f });
    tabs->ignoreAnchorPointForPosition(false);
    m_mainLayer->addChildAtPosition(tabs, Anchor::Center, ccp(CONTROLS_CENTER.x, 104));
    constexpr std::array<char const*, 4> TAB_NAMES { "Place", "Text", "Colors", "Voice" };
    for (int i = 0; i < 4; i++) {
        auto sprite = toggleSprite(TAB_NAMES[i], 92, i == 0, .5f);
        auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(BubbleDesigner::onTab));
        button->setTag(i);
        tabs->addChildAtPosition(button, Anchor::Center, ccp(-76.f + i * 50.6f, 0));
        m_tabSprites.push_back(sprite);
    }

    auto controlsBG = panel({ 214.f, 196.f }, 60);
    m_mainLayer->addChildAtPosition(controlsBG, Anchor::Center, CONTROLS_CENTER + CCPoint(0, -2));

    m_controls = CCNode::create();
    m_controls->setContentSize({ 204.f, 196.f });
    m_controls->setAnchorPoint({ .5f, .5f });
    m_mainLayer->addChildAtPosition(m_controls, Anchor::Center, CONTROLS_CENTER);

    // ---- Preview: a small game screen ----
    auto win = CCDirector::get()->getWinSize();
    m_screenScale = std::min(SCREEN_FIT.width / win.width, SCREEN_FIT.height / win.height);
    auto shown = win * m_screenScale;

    auto previewBG = panel({ 252.f, 230.f }, 60);
    m_mainLayer->addChildAtPosition(previewBG, Anchor::Center, ccp(PREVIEW_X, -2));

    auto frame = rect({ 255, 255, 255, 70 }, shown + CCSize(3.f, 3.f));
    m_mainLayer->addChildAtPosition(frame, Anchor::Center, ccp(PREVIEW_X, SCREEN_Y));

    m_screen = CCNode::create();
    m_screen->setContentSize(win);
    m_screen->setAnchorPoint({ .5f, .5f });
    m_screen->setScale(m_screenScale);
    m_mainLayer->addChildAtPosition(m_screen, Anchor::Center, ccp(PREVIEW_X, SCREEN_Y));

    auto sky = CCLayerGradient::create({ 40, 90, 220, 255 }, { 12, 24, 80, 255 });
    sky->setContentSize(win);
    m_screen->addChild(sky);
    constexpr float GROUND = 70.f;
    auto ground = CCLayerColor::create({ 0, 0, 30, 150 }, win.width, GROUND);
    m_screen->addChild(ground);
    auto groundLine = CCLayerColor::create({ 255, 255, 255, 140 }, win.width, 2.f);
    groundLine->setPositionY(GROUND);
    m_screen->addChild(groundLine);

    auto spikes = CCDrawNode::create();
    for (float x : { win.width * .58f, win.width * .58f + 30.f, win.width * .8f }) {
        std::array<CCPoint, 3> tri { CCPoint(x - 15.f, GROUND), CCPoint(x + 15.f, GROUND), CCPoint(x, GROUND + 30.f) };
        spikes->drawPolygon(tri.data(), 3, ccc4f(0.f, 0.f, 0.f, 1.f), 1.5f, ccc4f(1.f, 1.f, 1.f, 1.f));
    }
    m_screen->addChild(spikes);

    auto gm = GameManager::get();
    auto player = SimplePlayer::create(gm->getPlayerFrame());
    player->updatePlayerFrame(gm->getPlayerFrame(), IconType::Cube);
    player->setColors(gm->colorForIdx(gm->getPlayerColor()), gm->colorForIdx(gm->getPlayerColor2()));
    player->setPosition({ win.width * .3f, GROUND + 15.f });
    m_screen->addChild(player);

    // GD's progress bar
    auto barBG = CCLayerColor::create({ 0, 0, 0, 140 }, 200.f, 9.f);
    barBG->setPosition({ win.width / 2 - 100.f, win.height - 14.f });
    m_screen->addChild(barBG);
    auto barFill = CCLayerColor::create({ 125, 255, 0, 255 }, 126.f, 9.f);
    barFill->setPosition({ win.width / 2 - 100.f, win.height - 14.f });
    m_screen->addChild(barFill);
    auto percent = CCLabelBMFont::create("63%", "bigFont.fnt");
    percent->setScale(.35f);
    percent->setAnchorPoint({ 0.f, .5f });
    percent->setPosition({ win.width / 2 + 106.f, win.height - 9.5f });
    m_screen->addChild(percent);

    m_dragArea = BubbleDragArea::create(this, shown);
    m_mainLayer->addChildAtPosition(m_dragArea, Anchor::Center, ccp(PREVIEW_X, SCREEN_Y));

    auto dragHint = CCLabelBMFont::create("Drag the bubble to move it", "chatFont.fnt");
    dragHint->setScale(.45f);
    dragHint->setColor(HINT);
    m_mainLayer->addChildAtPosition(dragHint, Anchor::Center, ccp(PREVIEW_X, SCREEN_Y - shown.height / 2 - 9.f));

    // ---- Close-up ----
    m_closeUp = CCNode::create();
    m_mainLayer->addChildAtPosition(m_closeUp, Anchor::Center, ccp(PREVIEW_X, CLOSE_UP_Y));

    // ---- Bottom buttons ----
    auto left = CCMenu::create();
    left->setContentSize({ 204.f, 30.f });
    left->ignoreAnchorPointForPosition(false);
    m_mainLayer->addChildAtPosition(left, Anchor::Bottom, ccp(CONTROLS_CENTER.x, 20));
    left->addChild(textButton("Reset", "GJ_button_06.png", .55f, this, menu_selector(BubbleDesigner::onReset)));
    left->addChild(textButton("Replay", "GJ_button_05.png", .55f, this, menu_selector(BubbleDesigner::onReplay)));
    left->setLayout(RowLayout::create()->setGap(8.f));

    auto right = CCMenu::create();
    right->setContentSize({ 240.f, 30.f });
    right->ignoreAnchorPointForPosition(false);
    m_mainLayer->addChildAtPosition(right, Anchor::Bottom, ccp(PREVIEW_X, 20));
    right->addChild(textButton("Cancel", "GJ_button_06.png", .6f, this, menu_selector(BubbleDesigner::onClose)));
    right->addChild(textButton("Save", "GJ_button_01.png", .6f, this, menu_selector(BubbleDesigner::onSave)));
    right->setLayout(RowLayout::create()->setGap(10.f));

    this->rebuildControls();
    // Let the reply type itself out once when the designer opens
    this->refreshPreview(true);
    return true;
}

Profile BubbleDesigner::previewProfile() const {
    auto p = profile::current();
    applyLook(p, m_look);
    return p;
}

void BubbleDesigner::refreshPreview(bool animate) {
    auto profile = this->previewProfile();

    if (m_screenBubble) m_screenBubble->removeFromParent();
    m_screenBubble = SpeechBubble::createPreview(profile, PREVIEW_TEXT, false);
    if (m_screenBubble) m_screen->addChild(m_screenBubble, 10);

    m_closeUp->removeAllChildren();
    if (auto bubble = SpeechBubble::createPreview(profile, PREVIEW_TEXT, animate)) {
        m_closeUp->addChild(fitted(bubble, bubble->getContentSize(), CLOSE_UP_FIT));
    }
}

void BubbleDesigner::placeScreenBubble() {
    if (!m_screenBubble) return;
    BubbleStyle style;
    style.anchor = m_look.anchor;
    style.offsetX = m_look.offsetX;
    style.offsetY = m_look.offsetY;
    m_screenBubble->place(style);
}

void BubbleDesigner::dragBy(CCPoint location, CCPoint previous) {
    auto delta = m_screen->convertToNodeSpace(location) - m_screen->convertToNodeSpace(previous);
    m_look.offsetX = std::clamp(m_look.offsetX + delta.x, -look::MAX_OFFSET, look::MAX_OFFSET);
    m_look.offsetY = std::clamp(m_look.offsetY + delta.y, -look::MAX_OFFSET, look::MAX_OFFSET);
    this->placeScreenBubble();
    this->syncNudgeRows();
}

void BubbleDesigner::dragEnded() {
    if (!m_screenBubble) return;
    // Snap to the nearest anchor and keep the rest as a small nudge, so the
    // bubble lands in the same spot on screens of any shape
    auto win = CCDirector::get()->getWinSize();
    auto size = m_screenBubble->getContentSize();
    auto center = m_screenBubble->getPosition();

    constexpr std::array ANCHORS {
        BubbleAnchor::Top, BubbleAnchor::Bottom, BubbleAnchor::TopLeft,
        BubbleAnchor::TopRight, BubbleAnchor::BottomLeft, BubbleAnchor::BottomRight,
    };
    auto best = m_look.anchor;
    float bestDistance = INFINITY;
    for (auto anchor : ANCHORS) {
        float distance = ccpDistance(center, SpeechBubble::anchorPosition(anchor, size, win));
        if (distance < bestDistance) {
            bestDistance = distance;
            best = anchor;
        }
    }
    auto base = SpeechBubble::anchorPosition(best, size, win);
    m_look.anchor = best;
    m_look.offsetX = std::round(center.x - base.x);
    m_look.offsetY = std::round(center.y - base.y);
    this->placeScreenBubble();
    if (m_tab == Tab::Place) this->rebuildControls();
}

// ---- Controls ----

CCLabelBMFont* BubbleDesigner::addHeader(char const* title, float y) {
    auto label = CCLabelBMFont::create(title, "goldFont.fnt");
    label->setScale(.45f);
    m_controls->addChildAtPosition(label, Anchor::Center, ccp(0, y));
    return label;
}

CCMenu* BubbleDesigner::addMenu(CCSize size, CCPoint offset) {
    auto menu = CCMenu::create();
    menu->setContentSize(size);
    menu->ignoreAnchorPointForPosition(false);
    m_controls->addChildAtPosition(menu, Anchor::Center, offset);
    return menu;
}

SliderRow BubbleDesigner::addRow(
    char const* title, float y, float min, float max, float step, float value,
    std::function<std::string(float)> format, std::function<void(float)> onChange
) {
    SliderRow row;
    auto label = CCLabelBMFont::create(title, "goldFont.fnt");
    label->setScale(.34f);
    label->setAnchorPoint({ 0.f, .5f });
    m_controls->addChildAtPosition(label, Anchor::Center, ccp(-98, y));

    row.value = CCLabelBMFont::create(format(value).c_str(), "bigFont.fnt");
    row.value->setScale(.3f);
    row.value->setAnchorPoint({ 1.f, .5f });
    m_controls->addChildAtPosition(row.value, Anchor::Center, ccp(98, y));

    auto valueLabel = row.value;
    row.slider = SliderNode::create([format, onChange, valueLabel](SliderNode*, float v) {
        valueLabel->setString(format(v).c_str());
        onChange(v);
    });
    row.slider->setMin(min);
    row.slider->setMax(max);
    row.slider->setSnapStep(step);
    row.slider->setValue(value);
    row.slider->setContentSize({ 88.f, row.slider->getContentHeight() });
    row.slider->setAnchorPoint({ .5f, .5f });
    m_controls->addChildAtPosition(row.slider, Anchor::Center, ccp(14, y));
    return row;
}

void BubbleDesigner::rebuildControls() {
    m_controls->removeAllChildren();
    m_xRow = {};
    m_yRow = {};
    for (size_t i = 0; i < m_tabSprites.size(); i++) {
        m_tabSprites[i]->updateBGImage(static_cast<int>(i) == static_cast<int>(m_tab) ? ON_BG : OFF_BG);
    }

    switch (m_tab) {
        case Tab::Place: this->buildPlace(); break;
        case Tab::Text: this->buildText(); break;
        case Tab::Colors: this->buildColors(); break;
        case Tab::Voice: this->buildVoice(); break;
    }
    // New sliders and buttons need to sit above the popup for touches
    handleTouchPriority(this);
}

void BubbleDesigner::buildPlace() {
    this->addHeader("Position", 84);

    // Six little screens, each showing where the bubble goes
    auto grid = this->addMenu({ 160.f, 64.f }, ccp(0, 46));
    constexpr std::array ORDER {
        BubbleAnchor::TopLeft, BubbleAnchor::Top, BubbleAnchor::TopRight,
        BubbleAnchor::BottomLeft, BubbleAnchor::Bottom, BubbleAnchor::BottomRight,
    };
    for (size_t i = 0; i < ORDER.size(); i++) {
        auto anchor = ORDER[i];
        CCSize size { 46.f, 28.f };
        auto icon = chip(nullptr, size, anchor == m_look.anchor, { 30, 45, 110, 255 });
        bool top = anchor == BubbleAnchor::Top || anchor == BubbleAnchor::TopLeft || anchor == BubbleAnchor::TopRight;
        bool left = anchor == BubbleAnchor::TopLeft || anchor == BubbleAnchor::BottomLeft;
        bool right = anchor == BubbleAnchor::TopRight || anchor == BubbleAnchor::BottomRight;
        auto bar = rect({ 255, 255, 255, 230 }, { 18.f, 6.f });
        bar->setPosition({ left ? 13.f : right ? 33.f : 23.f, top ? 20.f : 8.f });
        icon->addChild(bar);

        auto button = CCMenuItemSpriteExtra::create(icon, this, menu_selector(BubbleDesigner::onAnchor));
        button->setTag(static_cast<int>(anchor));
        int col = static_cast<int>(i) % 3;
        int row = static_cast<int>(i) / 3;
        grid->addChildAtPosition(button, Anchor::Center, ccp(-52.f + col * 52.f, row == 0 ? 16.f : -16.f));
    }

    this->addRow("Width", 0, look::MIN_WIDTH, look::MAX_WIDTH, 10.f, m_look.width,
        [](float v) { return fmt::format("{}", static_cast<int>(v)); },
        [this](float v) { m_look.width = v; this->refreshPreview(false); }
    );

    this->addHeader("Nudge", -26);
    auto center = this->addMenu({ 60.f, 20.f }, ccp(72, -26));
    center->addChildAtPosition(
        textButton("Center", "GJ_button_04.png", .38f, this, menu_selector(BubbleDesigner::onCenter)), Anchor::Center
    );

    auto signedFormat = [](float v) { return fmt::format("{:+}", static_cast<int>(std::round(v))); };
    m_xRow = this->addRow("Sideways", -50, -look::MAX_OFFSET, look::MAX_OFFSET, 1.f, m_look.offsetX, signedFormat,
        [this](float v) { m_look.offsetX = v; this->placeScreenBubble(); }
    );
    m_yRow = this->addRow("Up/Down", -78, -look::MAX_OFFSET, look::MAX_OFFSET, 1.f, m_look.offsetY, signedFormat,
        [this](float v) { m_look.offsetY = v; this->placeScreenBubble(); }
    );
}

void BubbleDesigner::syncNudgeRows() {
    auto sync = [](SliderRow const& row, float v) {
        if (!row.slider) return;
        row.slider->setValue(v);
        row.value->setString(fmt::format("{:+}", static_cast<int>(std::round(v))).c_str());
    };
    sync(m_xRow, m_look.offsetX);
    sync(m_yRow, m_look.offsetY);
}

void BubbleDesigner::buildText() {
    this->addHeader("Font", 84);
    auto fonts = this->addMenu({ 190.f, 50.f }, ccp(0, 54));
    constexpr std::array FONTS { BubbleFont::Chat, BubbleFont::Big, BubbleFont::Gold };
    constexpr std::array<char const*, 3> NAMES { "Clean", "Bold", "Gold" };
    for (size_t i = 0; i < FONTS.size(); i++) {
        auto sample = CCLabelBMFont::create("Aa", look::fontFile(FONTS[i]));
        sample->setScale(FONTS[i] == BubbleFont::Chat ? .75f : .5f);
        auto icon = chip(sample, { 52.f, 30.f }, FONTS[i] == m_look.font);
        auto button = CCMenuItemSpriteExtra::create(icon, this, menu_selector(BubbleDesigner::onFont));
        button->setTag(static_cast<int>(FONTS[i]));
        fonts->addChildAtPosition(button, Anchor::Center, ccp(-62.f + i * 62.f, 6));

        auto name = CCLabelBMFont::create(NAMES[i], "bigFont.fnt");
        name->setScale(.26f);
        name->setColor(HINT);
        fonts->addChildAtPosition(name, Anchor::Center, ccp(-62.f + i * 62.f, -18));
    }

    this->addRow("Size", 12, look::MIN_TEXT_SCALE, look::MAX_TEXT_SCALE, .05f, m_look.textScale,
        [](float v) { return fmt::format("{:.2f}x", v); },
        [this](float v) { m_look.textScale = v; this->refreshPreview(false); }
    );
    this->addRow("Stay", -18, 2.f, 15.f, .5f, m_look.duration,
        [](float v) { return fmt::format("{:.1f}s", v); },
        [this](float v) { m_look.duration = v; }
    );

    auto label = CCLabelBMFont::create("Letter by letter", "goldFont.fnt");
    label->setScale(.38f);
    label->setAnchorPoint({ 0.f, .5f });
    m_controls->addChildAtPosition(label, Anchor::Center, ccp(-98, -50));
    auto toggle = this->addMenu({ 60.f, 22.f }, ccp(70, -50));
    toggle->addChildAtPosition(
        CCMenuItemSpriteExtra::create(
            toggleSprite(m_look.typewriter ? "On" : "Off", 80, m_look.typewriter, .45f),
            this, menu_selector(BubbleDesigner::onTypewriter)
        ),
        Anchor::Center
    );

    auto hint = CCLabelBMFont::create("Press Replay to watch it type", "chatFont.fnt");
    hint->setScale(.45f);
    hint->setColor(HINT);
    m_controls->addChildAtPosition(hint, Anchor::Center, ccp(0, -80));
}

void BubbleDesigner::buildColors() {
    this->addHeader("Themes", 84);
    auto themes = this->addMenu({ 204.f, 28.f }, ccp(0, 62));
    for (size_t i = 0; i < THEMES.size(); i++) {
        auto const& theme = THEMES[i];
        auto sample = CCLabelBMFont::create("Aa", "chatFont.fnt");
        sample->setScale(.6f);
        sample->setColor(theme.text);
        auto icon = chip(sample, { 30.f, 22.f }, false, theme.background);
        auto button = CCMenuItemSpriteExtra::create(icon, this, menu_selector(BubbleDesigner::onTheme));
        button->setTag(static_cast<int>(i));
        themes->addChildAtPosition(button, Anchor::Center, ccp(-85.f + i * 34.f, 0));
    }

    auto addColorRow = [this](char const* title, float y, int tag, ccColor3B color, GLubyte alpha) {
        auto label = CCLabelBMFont::create(title, "goldFont.fnt");
        label->setScale(.38f);
        label->setAnchorPoint({ 0.f, .5f });
        m_controls->addChildAtPosition(label, Anchor::Center, ccp(-98, y));
        auto menu = this->addMenu({ 50.f, 24.f }, ccp(76, y));
        auto button = CCMenuItemSpriteExtra::create(
            colorSwatch(color, alpha, { 40.f, 18.f }), this, menu_selector(BubbleDesigner::onPickColor)
        );
        button->setTag(tag);
        menu->addChildAtPosition(button, Anchor::Center);
    };

    auto const& bg = m_look.background;
    addColorRow("Background", 32, BACKGROUND, { bg.r, bg.g, bg.b }, bg.a);
    this->addRow("Opacity", 8, 0.f, 255.f, 5.f, bg.a,
        [](float v) { return fmt::format("{}%", static_cast<int>(std::round(v / 2.55f))); },
        [this](float v) { m_look.background.a = static_cast<GLubyte>(v); this->refreshPreview(false); }
    );
    addColorRow("Text", -22, TEXT, m_look.textColor, 255);
    addColorRow("Name", -50, NAME, m_look.nameColor, 255);

    auto hint = CCLabelBMFont::create("White keeps the name gold", "chatFont.fnt");
    hint->setScale(.45f);
    hint->setColor(HINT);
    m_controls->addChildAtPosition(hint, Anchor::Center, ccp(0, -80));
}

void BubbleDesigner::buildVoice() {
    this->addHeader("Talking Sound", 84);
    auto sounds = this->addMenu({ 190.f, 50.f }, ccp(0, 52));
    constexpr std::array SOUNDS { TalkSound::Blip, TalkSound::Chirp, TalkSound::Soft, TalkSound::Off };
    for (size_t i = 0; i < SOUNDS.size(); i++) {
        auto sprite = toggleSprite(look::soundName(SOUNDS[i]), 110, SOUNDS[i] == m_look.sound, .5f);
        auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(BubbleDesigner::onSound));
        button->setTag(static_cast<int>(SOUNDS[i]));
        float x = i % 2 == 0 ? -36.f : 36.f;
        float y = i < 2 ? 12.f : -12.f;
        sounds->addChildAtPosition(button, Anchor::Center, ccp(x, y));
    }

    this->addRow("Volume", 14, 0.f, 100.f, 5.f, static_cast<float>(m_look.volume),
        [](float v) { return fmt::format("{}%", static_cast<int>(v)); },
        [this](float v) { m_look.volume = static_cast<int>(v); }
    );

    auto hear = this->addMenu({ 120.f, 30.f }, ccp(0, -24));
    hear->addChildAtPosition(
        textButton("Hear It", "GJ_button_05.png", .6f, this, menu_selector(BubbleDesigner::onReplay)), Anchor::Center
    );

    auto hint = CCLabelBMFont::create("Plays while the reply types out", "chatFont.fnt");
    hint->setScale(.45f);
    hint->setColor(HINT);
    m_controls->addChildAtPosition(hint, Anchor::Center, ccp(0, -64));
}

// ---- Buttons ----

void BubbleDesigner::onTab(CCObject* sender) {
    auto tab = static_cast<Tab>(static_cast<CCNode*>(sender)->getTag());
    if (tab == m_tab) return;
    m_tab = tab;
    this->rebuildControls();
}

void BubbleDesigner::onAnchor(CCObject* sender) {
    m_look.anchor = static_cast<BubbleAnchor>(static_cast<CCNode*>(sender)->getTag());
    // A new spot starts without a nudge
    m_look.offsetX = 0.f;
    m_look.offsetY = 0.f;
    this->placeScreenBubble();
    this->rebuildControls();
}

void BubbleDesigner::onCenter(CCObject*) {
    m_look.offsetX = 0.f;
    m_look.offsetY = 0.f;
    this->placeScreenBubble();
    this->syncNudgeRows();
}

void BubbleDesigner::onFont(CCObject* sender) {
    m_look.font = static_cast<BubbleFont>(static_cast<CCNode*>(sender)->getTag());
    // GD's bold and gold fonts are much bigger; start them at a size that fits
    if (m_look.font != BubbleFont::Chat && m_look.textScale > .5f) m_look.textScale = .4f;
    if (m_look.font == BubbleFont::Chat && m_look.textScale < .5f) m_look.textScale = .7f;
    this->rebuildControls();
    this->refreshPreview(false);
}

void BubbleDesigner::onTypewriter(CCObject*) {
    m_look.typewriter = !m_look.typewriter;
    this->rebuildControls();
    if (m_look.typewriter) this->refreshPreview(true);
}

void BubbleDesigner::onTheme(CCObject* sender) {
    auto const& theme = THEMES[static_cast<size_t>(static_cast<CCNode*>(sender)->getTag()) % THEMES.size()];
    m_look.background = theme.background;
    m_look.textColor = theme.text;
    m_look.nameColor = theme.name;
    this->rebuildControls();
    this->refreshPreview(false);
}

void BubbleDesigner::onPickColor(CCObject* sender) {
    int target = static_cast<CCNode*>(sender)->getTag();
    Ref<BubbleDesigner> self = this;
    ColorPickPopup* popup = nullptr;
    if (target == BACKGROUND) {
        popup = ColorPickPopup::create(m_look.background);
    }
    else {
        popup = ColorPickPopup::create(target == TEXT ? m_look.textColor : m_look.nameColor);
    }
    popup->setCallback([self, target](ccColor4B const& color) {
        if (target == BACKGROUND) self->m_look.background = color;
        else if (target == TEXT) self->m_look.textColor = { color.r, color.g, color.b };
        else self->m_look.nameColor = { color.r, color.g, color.b };
        self->rebuildControls();
        self->refreshPreview(false);
    });
    popup->show();
}

void BubbleDesigner::onSound(CCObject* sender) {
    m_look.sound = static_cast<TalkSound>(static_cast<CCNode*>(sender)->getTag());
    this->rebuildControls();
    auto file = look::soundFile(m_look.sound);
    if (!file.empty() && m_look.volume > 0) {
        FMODAudioEngine::get()->playEffect(file, 1.f, 0.f, m_look.volume / 100.f);
    }
}

void BubbleDesigner::onReplay(CCObject*) {
    this->refreshPreview(true);
}

void BubbleDesigner::onReset(CCObject*) {
    m_look = BubbleLook {};
    this->rebuildControls();
    this->refreshPreview(true);
}

void BubbleDesigner::onSave(CCObject*) {
    if (m_onSave) m_onSave(look::clamped(m_look));
    Popup::onClose(nullptr);
}

void BubbleDesigner::onClose(CCObject*) {
    if (m_look == m_original) {
        Popup::onClose(nullptr);
        return;
    }
    Ref<BubbleDesigner> self = this;
    createQuickPopup(
        "Unsaved Changes",
        "Close without saving the new bubble look?",
        "Keep Editing", "Discard",
        [self](FLAlertLayer*, bool discard) {
            if (discard) self->Popup::onClose(nullptr);
        }
    );
}

// ---- Chat window look ----

namespace {
    constexpr float CHAT_W = 420.f;
    constexpr float CHAT_H = 270.f;
    const CCPoint CHAT_CONTROLS { -108.f, 0.f };
    const CCPoint CHAT_PREVIEW { 100.f, 4.f };
    const CCSize CHAT_FIT { 196.f, 168.f };
}

ChatLookEditor* ChatLookEditor::create(ChatLook look, SaveCallback onSave) {
    auto ret = new ChatLookEditor();
    if (ret->init(std::move(look), std::move(onSave))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool ChatLookEditor::init(ChatLook look, SaveCallback onSave) {
    if (!Popup::init(CHAT_W, CHAT_H)) return false;
    m_look = look;
    m_original = look;
    m_onSave = std::move(onSave);
    this->setTitle("Chat Window");

    auto controlsBG = panel({ 190.f, 196.f }, 60);
    m_mainLayer->addChildAtPosition(controlsBG, Anchor::Center, CHAT_CONTROLS + CCPoint(0, -2));
    m_controls = CCNode::create();
    m_controls->setContentSize({ 180.f, 196.f });
    m_controls->setAnchorPoint({ .5f, .5f });
    m_mainLayer->addChildAtPosition(m_controls, Anchor::Center, CHAT_CONTROLS);

    auto previewBG = panel({ 210.f, 196.f }, 60);
    m_mainLayer->addChildAtPosition(previewBG, Anchor::Center, ccp(CHAT_PREVIEW.x, -2));
    m_preview = CCNode::create();
    m_mainLayer->addChildAtPosition(m_preview, Anchor::Center, CHAT_PREVIEW);

    auto bottom = CCMenu::create();
    bottom->setContentSize({ 220.f, 30.f });
    bottom->ignoreAnchorPointForPosition(false);
    m_mainLayer->addChildAtPosition(bottom, Anchor::Bottom, ccp(0, 18));
    bottom->addChild(textButton("Cancel", "GJ_button_06.png", .6f, this, menu_selector(ChatLookEditor::onClose)));
    bottom->addChild(textButton("Save", "GJ_button_01.png", .6f, this, menu_selector(ChatLookEditor::onSave)));
    bottom->setLayout(RowLayout::create()->setGap(10.f));

    this->rebuildControls();
    this->refreshPreview();
    return true;
}

void ChatLookEditor::rebuildControls() {
    m_controls->removeAllChildren();
    auto header = [this](char const* title, float y) {
        auto label = CCLabelBMFont::create(title, "goldFont.fnt");
        label->setScale(.45f);
        m_controls->addChildAtPosition(label, Anchor::Center, ccp(0, y));
    };
    auto menu = [this](CCSize size, CCPoint offset) {
        auto m = CCMenu::create();
        m->setContentSize(size);
        m->ignoreAnchorPointForPosition(false);
        m_controls->addChildAtPosition(m, Anchor::Center, offset);
        return m;
    };

    // Theme: two little windows to pick from
    header("Theme", 84);
    auto themes = menu({ 170.f, 56.f }, ccp(0, 54));
    for (int i = 0; i < 2; i++) {
        bool custom = i == 1;
        auto window = NineSlice::create(custom ? "square02b_001.png" : "GJ_square01.png");
        if (custom) window->setColor(m_look.color);
        window->setScale(.25f);
        window->setContentSize(CCSize(56.f, 32.f) / .25f);
        auto icon = chip(window, { 62.f, 38.f }, m_look.customColor == custom, { 0, 0, 0, 60 });
        auto button = CCMenuItemSpriteExtra::create(icon, this, menu_selector(ChatLookEditor::onTheme));
        button->setTag(i);
        themes->addChildAtPosition(button, Anchor::Center, ccp(custom ? 40.f : -40.f, 6));
        auto name = CCLabelBMFont::create(custom ? "Color" : "Classic", "bigFont.fnt");
        name->setScale(.28f);
        name->setColor(HINT);
        themes->addChildAtPosition(name, Anchor::Center, ccp(custom ? 40.f : -40.f, -21));
    }

    // Color presets and a picker for anything else
    header("Color", 10);
    auto colors = menu({ 180.f, 48.f }, ccp(0, -16));
    for (size_t i = 0; i <= CHAT_COLORS.size(); i++) {
        int col = static_cast<int>(i) % 5;
        int row = static_cast<int>(i) / 5;
        CCPoint pos { -72.f + col * 36.f, row == 0 ? 11.f : -11.f };
        if (i == CHAT_COLORS.size()) {
            auto plus = CCLabelBMFont::create("+", "bigFont.fnt");
            plus->setScale(.5f);
            auto icon = chip(plus, { 28.f, 18.f }, false, { 60, 60, 60, 255 });
            colors->addChildAtPosition(
                CCMenuItemSpriteExtra::create(icon, this, menu_selector(ChatLookEditor::onPickColor)), Anchor::Center, pos
            );
            continue;
        }
        auto c = CHAT_COLORS[i];
        bool selected = m_look.customColor && c.r == m_look.color.r && c.g == m_look.color.g && c.b == m_look.color.b;
        auto icon = chip(nullptr, { 28.f, 18.f }, selected, { c.r, c.g, c.b, 255 });
        auto button = CCMenuItemSpriteExtra::create(icon, this, menu_selector(ChatLookEditor::onPreset));
        button->setTag(static_cast<int>(i));
        colors->addChildAtPosition(button, Anchor::Center, pos);
    }

    header("Size", -54);
    auto sizes = menu({ 170.f, 26.f }, ccp(0, -76));
    for (int i = 0; i < 2; i++) {
        bool large = i == 1;
        auto button = CCMenuItemSpriteExtra::create(
            toggleSprite(large ? "Large" : "Normal", 110, m_look.large == large, .5f),
            this, menu_selector(ChatLookEditor::onSize)
        );
        button->setTag(i);
        sizes->addChildAtPosition(button, Anchor::Center, ccp(large ? 38.f : -38.f, 0));
    }
    handleTouchPriority(this);
}

void ChatLookEditor::refreshPreview() {
    m_preview->removeAllChildren();
    auto mock = chatMock(m_look, profile::current());
    m_preview->addChild(fitted(mock, mock->getContentSize(), CHAT_FIT));
}

void ChatLookEditor::onTheme(CCObject* sender) {
    m_look.customColor = static_cast<CCNode*>(sender)->getTag() == 1;
    this->rebuildControls();
    this->refreshPreview();
}

void ChatLookEditor::onPreset(CCObject* sender) {
    m_look.customColor = true;
    m_look.color = CHAT_COLORS[static_cast<size_t>(static_cast<CCNode*>(sender)->getTag()) % CHAT_COLORS.size()];
    this->rebuildControls();
    this->refreshPreview();
}

void ChatLookEditor::onPickColor(CCObject*) {
    Ref<ChatLookEditor> self = this;
    auto popup = ColorPickPopup::create(m_look.color);
    popup->setCallback([self](ccColor4B const& color) {
        self->m_look.customColor = true;
        self->m_look.color = { color.r, color.g, color.b };
        self->rebuildControls();
        self->refreshPreview();
    });
    popup->show();
}

void ChatLookEditor::onSize(CCObject* sender) {
    m_look.large = static_cast<CCNode*>(sender)->getTag() == 1;
    this->rebuildControls();
    this->refreshPreview();
}

void ChatLookEditor::onSave(CCObject*) {
    if (m_onSave) m_onSave(m_look);
    Popup::onClose(nullptr);
}

void ChatLookEditor::onClose(CCObject*) {
    if (m_look == m_original) {
        Popup::onClose(nullptr);
        return;
    }
    Ref<ChatLookEditor> self = this;
    createQuickPopup(
        "Unsaved Changes",
        "Close without saving the new chat look?",
        "Keep Editing", "Discard",
        [self](FLAlertLayer*, bool discard) {
            if (discard) self->Popup::onClose(nullptr);
        }
    );
}

// ---- Thumbnails for the settings rows ----

CCNode* look::bubbleThumbnail(BubbleLook const& bubbleLook, CCSize fit) {
    auto p = profile::current();
    applyLook(p, bubbleLook);
    auto bubble = SpeechBubble::createPreview(p, "Nice try. Again?", false);
    if (!bubble) return CCNode::create();
    return fitted(bubble, bubble->getContentSize(), fit);
}

CCNode* look::chatThumbnail(ChatLook const& chatLook, CCSize fit) {
    auto mock = chatMock(chatLook, profile::current());
    return fitted(mock, mock->getContentSize(), fit);
}
