#include "TextEditorPopup.hpp"
#include "Text.hpp"

#include <cmath>

using namespace geode::prelude;

namespace {
    constexpr float WIDTH = 440.f;
    constexpr float HEIGHT = 290.f;
    constexpr float BOX_WIDTH = 400.f;
    constexpr float BOX_HEIGHT = 146.f;
    constexpr float BOX_Y = 4.f;
    constexpr float PADDING = 10.f;
    // Text shrinks between these as it fills the box, so it never overflows
    constexpr float MAX_TEXT_SCALE = .6f;
    constexpr float MIN_TEXT_SCALE = .32f;

    constexpr ccColor3B HINT_GRAY { 200, 200, 200 };
    constexpr ccColor3B DIM_GRAY { 120, 120, 120 };
    constexpr ccColor3B ORANGE { 255, 190, 80 };
    constexpr ccColor3B RED { 255, 110, 110 };

    CCMenuItemSpriteExtra* textButton(char const* text, char const* bg, float scale, CCObject* target, SEL_MenuHandler selector) {
        auto sprite = ButtonSprite::create(text, "goldFont.fnt", bg, .8f);
        sprite->setScale(scale);
        return CCMenuItemSpriteExtra::create(sprite, target, selector);
    }

    std::string clipTo(std::string text, size_t max) {
        if (text.size() > max) text.resize(max);
        return text;
    }
}

TextEditorPopup* TextEditorPopup::create(TextEditorOptions options, std::string text, SaveCallback onSave) {
    auto ret = new TextEditorPopup();
    if (ret->init(std::move(options), std::move(text), std::move(onSave))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool TextEditorPopup::init(TextEditorOptions options, std::string text, SaveCallback onSave) {
    if (!Popup::init(WIDTH, HEIGHT)) return false;

    m_options = std::move(options);
    m_onSave = std::move(onSave);
    m_text = clipTo(text::toDisplayable(text), m_options.maxLength);
    m_original = m_text;

    this->setTitle(m_options.title);

    if (!m_options.hint.empty()) {
        auto hint = Label::create(m_options.hint, "chatFont.fnt");
        hint->setAlignment(Label::Alignment::Center);
        hint->setScale(.5f);
        hint->setMaxWidth((WIDTH - 50.f) / .5f);
        hint->setColor(HINT_GRAY);
        m_mainLayer->addChildAtPosition(hint, Anchor::Center, ccp(0, 96));
    }

    // The writing area
    m_box = NineSlice::create("square02b_001.png");
    m_box->setColor({ 0, 0, 0 });
    m_box->setOpacity(100);
    m_box->setScale(.5f);
    m_box->setContentSize(CCSize(BOX_WIDTH, BOX_HEIGHT) / .5f);
    m_mainLayer->addChildAtPosition(m_box, Anchor::Center, ccp(0, BOX_Y));

    // Tapping anywhere in the box starts typing
    auto boxMenu = CCMenu::create();
    boxMenu->setContentSize({ BOX_WIDTH, BOX_HEIGHT });
    boxMenu->ignoreAnchorPointForPosition(false);
    m_mainLayer->addChildAtPosition(boxMenu, Anchor::Center, ccp(0, BOX_Y));
    auto hitArea = CCNode::create();
    hitArea->setContentSize({ BOX_WIDTH, BOX_HEIGHT });
    hitArea->setAnchorPoint({ .5f, .5f });
    auto hitButton = CCMenuItemSpriteExtra::create(hitArea, this, menu_selector(TextEditorPopup::onFocus));
    hitButton->m_scaleMultiplier = 1.f;
    boxMenu->addChildAtPosition(hitButton, Anchor::Center);

    // The real input only receives keystrokes; its own one-line label is
    // hidden and the text is drawn wrapped by m_textLabel instead
    m_input = TextInput::create(BOX_WIDTH - PADDING * 2, "", "chatFont.fnt");
    m_input->setCommonFilter(CommonFilter::Any);
    m_input->setMaxCharCount(m_options.maxLength);
    m_input->hideBG();
    m_input->setString(m_text);
    m_input->setCallback([this](std::string const& value) {
        m_text = clipTo(value, m_options.maxLength);
        m_blink = 0.f;
        this->refresh();
    });
    m_mainLayer->addChildAtPosition(m_input, Anchor::Center, ccp(0, BOX_Y));
    this->hideNativeText();

    auto textTopLeft = ccp(-BOX_WIDTH / 2 + PADDING, BOX_Y + BOX_HEIGHT / 2 - PADDING);

    m_placeholderLabel = Label::create(m_options.placeholder, "chatFont.fnt");
    m_placeholderLabel->setAlignment(Label::Alignment::Left);
    m_placeholderLabel->setScale(MAX_TEXT_SCALE);
    m_placeholderLabel->setMaxWidth((BOX_WIDTH - PADDING * 2) / MAX_TEXT_SCALE);
    m_placeholderLabel->setColor(DIM_GRAY);
    m_placeholderLabel->setAnchorPoint({ 0.f, 1.f });
    m_mainLayer->addChildAtPosition(m_placeholderLabel, Anchor::Center, textTopLeft);

    m_textLabel = Label::create("", "chatFont.fnt");
    m_textLabel->setAlignment(Label::Alignment::Left);
    m_textLabel->setAnchorPoint({ 0.f, 1.f });
    m_mainLayer->addChildAtPosition(m_textLabel, Anchor::Center, textTopLeft);

    m_cursor = CCLabelBMFont::create("|", "chatFont.fnt");
    m_cursor->setAnchorPoint({ 0.f, .5f });
    m_cursor->setColor({ 120, 220, 255 });
    m_cursor->setVisible(false);
    m_mainLayer->addChild(m_cursor);

    m_tapHint = CCLabelBMFont::create("Tap to type", "bigFont.fnt");
    m_tapHint->setScale(.28f);
    m_tapHint->setOpacity(110);
    m_tapHint->setAnchorPoint({ 1.f, 0.f });
    m_mainLayer->addChildAtPosition(m_tapHint, Anchor::Center, ccp(BOX_WIDTH / 2 - 7, BOX_Y - BOX_HEIGHT / 2 + 5));

    // Character count on the left, helpers on the right
    float toolsY = BOX_Y - BOX_HEIGHT / 2 - 16.f;
    m_counter = CCLabelBMFont::create("", "bigFont.fnt");
    m_counter->setScale(.32f);
    m_counter->setAnchorPoint({ 0.f, .5f });
    m_mainLayer->addChildAtPosition(m_counter, Anchor::Center, ccp(-BOX_WIDTH / 2 + 2, toolsY));

    auto tools = CCMenu::create();
    tools->setContentSize({ 220.f, 26.f });
    tools->ignoreAnchorPointForPosition(false);
    tools->setAnchorPoint({ 1.f, .5f });
    m_mainLayer->addChildAtPosition(tools, Anchor::Center, ccp(BOX_WIDTH / 2, toolsY));
    tools->addChild(textButton("Clear", "GJ_button_06.png", .5f, this, menu_selector(TextEditorPopup::onClear)));
    tools->addChild(textButton("Paste", "GJ_button_04.png", .5f, this, menu_selector(TextEditorPopup::onPaste)));
    if (!m_options.examples.empty()) {
        tools->addChild(textButton("Example", "GJ_button_05.png", .5f, this, menu_selector(TextEditorPopup::onExample)));
    }
    tools->setLayout(
        RowLayout::create()
            ->setAxisAlignment(AxisAlignment::End)
            ->setAxisReverse(true)
            ->setGap(6.f)
    );

    auto bottom = CCMenu::create();
    bottom->setContentSize({ 260.f, 34.f });
    bottom->ignoreAnchorPointForPosition(false);
    m_mainLayer->addChildAtPosition(bottom, Anchor::Bottom, ccp(0, 24));
    bottom->addChild(textButton("Cancel", "GJ_button_06.png", .75f, this, menu_selector(TextEditorPopup::onClose)));
    bottom->addChild(textButton("Save", "GJ_button_01.png", .75f, this, menu_selector(TextEditorPopup::onSave)));
    bottom->setLayout(RowLayout::create()->setGap(14.f));

    this->refresh();
    this->scheduleUpdate();
    return true;
}

void TextEditorPopup::setText(std::string text) {
    m_text = clipTo(text::toDisplayable(text), m_options.maxLength);
    m_input->setString(m_text);
    this->refresh();
}

void TextEditorPopup::refresh() {
    m_placeholderLabel->setVisible(m_text.empty());

    // Start big and shrink until everything fits in the box
    float innerWidth = BOX_WIDTH - PADDING * 2;
    float innerHeight = BOX_HEIGHT - PADDING * 2;
    m_textLabel->setText(m_text);
    for (float scale = MAX_TEXT_SCALE;; scale -= .02f) {
        scale = std::max(scale, MIN_TEXT_SCALE);
        m_textLabel->setScale(scale);
        m_textLabel->setMaxWidth(innerWidth / scale);
        m_textLabel->validate();
        if (m_textLabel->getScaledContentHeight() <= innerHeight || scale <= MIN_TEXT_SCALE) break;
    }

    auto count = m_text.size();
    auto max = m_options.maxLength;
    m_counter->setString(fmt::format("{} / {}", count, max).c_str());
    m_counter->setColor(count >= max ? RED : count * 10 >= max * 9 ? ORANGE : DIM_GRAY);

    this->placeCursor();
}

void TextEditorPopup::placeCursor() {
    // Lines are lineHeight apart from the top of the label; snap the cursor
    // to the line of the last letter, just after it
    float lineHeight = std::max(m_textLabel->getLineHeight(), 1.f);
    float top = m_textLabel->getContentHeight();
    CCPoint local { 0.f, top - lineHeight / 2 };

    auto const& glyphs = m_textLabel->getGlyphInfos();
    if (!glyphs.empty()) {
        auto const& last = glyphs.back();
        int line = static_cast<int>((top - last.bounds.getMidY()) / lineHeight);
        local.y = top - (line + .5f) * lineHeight;
        local.x = last.bounds.getMaxX() + 1.f;

        // Spaces typed after the last letter don't produce glyphs
        size_t end = last.stringIndex + last.stringLength;
        size_t spaces = 0;
        for (size_t i = end; i < m_text.size() && m_text[i] == ' '; i++) spaces++;
        local.x += spaces * lineHeight * .28f;
        local.x = std::min(local.x, m_textLabel->getMaxWidth());
    }

    auto world = m_textLabel->convertToWorldSpace(local);
    m_cursor->setPosition(m_mainLayer->convertToNodeSpace(world));
    m_cursor->setScale(m_textLabel->getScale() * 1.15f);
}

void TextEditorPopup::hideNativeText() {
    auto node = m_input->getInputNode();
    if (!node) return;
    if (node->m_textLabel) node->m_textLabel->setOpacity(0);
    if (node->m_cursor) {
        node->m_cursor->setOpacity(0);
        node->m_cursor->setScale(0.f);
    }
}

void TextEditorPopup::update(float dt) {
    auto node = m_input->getInputNode();
    if (!node) return;

    // GD may restyle its label while typing; keep it hidden
    this->hideNativeText();

    bool focused = node->m_selected;
    if (focused && !m_wasFocused) m_blink = 0.f;
    m_wasFocused = focused;
    m_blink += dt;
    m_cursor->setVisible(focused && std::fmod(m_blink, 1.f) < .55f);
    m_tapHint->setVisible(!focused);
}

void TextEditorPopup::onFocus(CCObject*) {
    m_input->focus();
}

void TextEditorPopup::onPaste(CCObject*) {
    auto pasted = utils::string::trim(text::toDisplayable(utils::clipboard::read()));
    if (pasted.empty()) {
        Notification::create("Nothing to paste", NotificationIcon::Info)->show();
        return;
    }
    // Added to the end, so a long prompt can be pasted in parts
    auto combined = m_text.empty() ? pasted : m_text + (m_text.ends_with(' ') ? "" : " ") + pasted;
    if (combined.size() > m_options.maxLength) {
        Notification::create(
            fmt::format("Cut to {} characters", m_options.maxLength), NotificationIcon::Warning
        )->show();
    }
    this->setText(combined);
}

void TextEditorPopup::onClear(CCObject*) {
    this->setText("");
    m_input->focus();
}

void TextEditorPopup::onExample(CCObject*) {
    if (m_options.examples.empty()) return;
    this->setText(m_options.examples[m_nextExample % m_options.examples.size()]);
    m_nextExample++;
}

void TextEditorPopup::onSave(CCObject*) {
    m_input->defocus();
    if (m_onSave) m_onSave(utils::string::trim(m_text));
    Popup::onClose(nullptr);
}

void TextEditorPopup::onClose(CCObject*) {
    m_input->defocus();
    if (m_text == m_original) {
        Popup::onClose(nullptr);
        return;
    }
    Ref<TextEditorPopup> self = this;
    createQuickPopup(
        "Unsaved Text",
        "Close without saving your changes?",
        "Keep Editing", "Discard",
        [self](FLAlertLayer*, bool discard) {
            if (discard) self->Popup::onClose(nullptr);
        }
    );
}
