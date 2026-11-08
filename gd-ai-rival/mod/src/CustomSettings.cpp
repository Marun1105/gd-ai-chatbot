#include "CustomSettings.hpp"
#include "AvatarEditor.hpp"
#include "Text.hpp"
#include "TextEditorPopup.hpp"

using namespace geode::prelude;

namespace {
    constexpr ccColor3B PREVIEW_GRAY { 175, 175, 175 };
    constexpr ccColor3B EMPTY_GRAY { 110, 110, 110 };

    CCMenuItemSpriteExtra* editButton(CCObject* target, SEL_MenuHandler selector) {
        auto sprite = ButtonSprite::create("Edit", "goldFont.fnt", "GJ_button_04.png", .8f);
        sprite->setScale(.55f);
        return CCMenuItemSpriteExtra::create(sprite, target, selector);
    }

    // Shorten text with "..." until the label fits in maxWidth
    void fitLabel(CCLabelBMFont* label, std::string text, float maxWidth) {
        label->setString(text.c_str());
        if (label->getScaledContentWidth() <= maxWidth) return;
        size_t keep = static_cast<size_t>(text.size() * maxWidth / label->getScaledContentWidth());
        while (keep > 0) {
            auto cut = utils::string::trimRight(text.substr(0, keep)) + "...";
            label->setString(cut.c_str());
            if (label->getScaledContentWidth() <= maxWidth) return;
            keep--;
        }
    }
}

// ---- Avatar row ----

class AvatarSettingNode : public SettingValueNodeV3<AvatarSetting> {
protected:
    CCNode* m_preview = nullptr;
    CCLabelBMFont* m_modeLabel = nullptr;
    CCMenuItemSpriteExtra* m_editButton = nullptr;

    bool init(std::shared_ptr<AvatarSetting> setting, float width) {
        if (!SettingValueNodeV3::init(setting, width)) return false;

        // A little taller than other rows, to show the avatar itself
        this->setContentSize({ width, 42.f });

        m_editButton = editButton(this, menu_selector(AvatarSettingNode::onEdit));
        this->getButtonMenu()->addChildAtPosition(m_editButton, Anchor::Right, ccp(-20, 0));

        m_modeLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_modeLabel->setScale(.3f);
        m_modeLabel->setColor(PREVIEW_GRAY);
        m_modeLabel->setAnchorPoint({ 1.f, .5f });
        this->addChildAtPosition(m_modeLabel, Anchor::Right, ccp(-102, 0));

        this->updateState(nullptr);
        return true;
    }

    void updateState(CCNode* invoker) override {
        SettingValueNodeV3::updateState(invoker);
        if (!m_modeLabel) return;

        auto look = this->getValue();
        if (m_preview) m_preview->removeFromParent();
        m_preview = avatar::create(look, 30.f);
        if (m_preview) this->addChildAtPosition(m_preview, Anchor::Right, ccp(-78, 0));
        m_modeLabel->setString(avatar::modeName(look.mode));

        bool enable = this->getSetting()->shouldEnable();
        m_editButton->setEnabled(enable);
        m_editButton->setOpacity(enable ? 255 : 120);
    }

    void onEdit(CCObject*) {
        Ref<AvatarSettingNode> self = this;
        AvatarEditor::create(this->getValue(), [self](AvatarLook const& look) {
            self->setValue(look, nullptr);
            // The editor has its own Save button, so don't wait for "Apply"
            self->commit();
        })->show();
    }

public:
    static AvatarSettingNode* create(std::shared_ptr<AvatarSetting> setting, float width) {
        auto ret = new AvatarSettingNode();
        if (ret->init(std::move(setting), width)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

Result<std::shared_ptr<SettingV3>> AvatarSetting::parse(std::string key, std::string modID, matjson::Value const& json) {
    auto ret = std::make_shared<AvatarSetting>();
    auto root = checkJson(json, "AvatarSetting");
    ret->parseBaseProperties(std::move(key), std::move(modID), root);
    return root.ok(std::static_pointer_cast<SettingV3>(ret));
}

SettingNodeV3* AvatarSetting::createNode(float width) {
    return AvatarSettingNode::create(std::static_pointer_cast<AvatarSetting>(shared_from_this()), width);
}

// ---- Long text row ----

class LongTextSettingNode : public SettingValueNodeV3<LongTextSetting> {
protected:
    CCLabelBMFont* m_preview = nullptr;
    CCMenuItemSpriteExtra* m_editButton = nullptr;
    float m_previewWidth = 0.f;

    bool init(std::shared_ptr<LongTextSetting> setting, float width) {
        if (!SettingValueNodeV3::init(setting, width)) return false;

        // Name on top, a one-line preview of the text under it
        this->setContentSize({ width, 44.f });
        this->getNameMenu()->updateAnchoredPosition(Anchor::TopLeft, ccp(10, -13), ccp(0, .5f));

        m_editButton = editButton(this, menu_selector(LongTextSettingNode::onEdit));
        this->getButtonMenu()->addChildAtPosition(m_editButton, Anchor::Right, ccp(-20, 0));

        m_previewWidth = width - 85.f;
        m_preview = CCLabelBMFont::create("", "chatFont.fnt");
        m_preview->setScale(.5f);
        m_preview->setAnchorPoint({ 0.f, .5f });
        this->addChildAtPosition(m_preview, Anchor::BottomLeft, ccp(12, 12));

        this->updateState(nullptr);
        return true;
    }

    void updateState(CCNode* invoker) override {
        SettingValueNodeV3::updateState(invoker);
        if (!m_preview) return;

        auto value = text::toDisplayable(this->getValue());
        if (value.empty()) {
            m_preview->setColor(EMPTY_GRAY);
            fitLabel(m_preview, "Empty. Tap Edit to write something.", m_previewWidth);
        }
        else {
            m_preview->setColor(PREVIEW_GRAY);
            fitLabel(m_preview, fmt::format("\"{}\"", value), m_previewWidth);
        }

        bool enable = this->getSetting()->shouldEnable();
        m_editButton->setEnabled(enable);
        m_editButton->setOpacity(enable ? 255 : 120);
        m_preview->setOpacity(enable ? 255 : 120);
    }

    void onEdit(CCObject*) {
        auto setting = this->getSetting();
        TextEditorOptions options;
        options.title = setting->getDisplayName();
        options.hint = setting->getHint();
        options.placeholder = setting->getPlaceholder();
        options.examples = setting->getExamples();
        options.maxLength = setting->getMaxLength();

        Ref<LongTextSettingNode> self = this;
        TextEditorPopup::create(std::move(options), this->getValue(), [self](std::string const& text) {
            self->setValue(text, nullptr);
            self->commit();
        })->show();
    }

public:
    static LongTextSettingNode* create(std::shared_ptr<LongTextSetting> setting, float width) {
        auto ret = new LongTextSettingNode();
        if (ret->init(std::move(setting), width)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

Result<std::shared_ptr<SettingV3>> LongTextSetting::parse(std::string key, std::string modID, matjson::Value const& json) {
    auto ret = std::make_shared<LongTextSetting>();
    auto root = checkJson(json, "LongTextSetting");
    ret->parseBaseProperties(std::move(key), std::move(modID), root);

    int maxLength = static_cast<int>(ret->m_maxLength);
    root.has("max-length").into(maxLength);
    ret->m_maxLength = static_cast<size_t>(std::clamp(maxLength, 1, 5000));
    root.has("placeholder").into(ret->m_placeholder);
    root.has("hint").into(ret->m_hint);
    root.has("examples").into(ret->m_examples);
    return root.ok(std::static_pointer_cast<SettingV3>(ret));
}

SettingNodeV3* LongTextSetting::createNode(float width) {
    return LongTextSettingNode::create(std::static_pointer_cast<LongTextSetting>(shared_from_this()), width);
}

Result<> LongTextSetting::isValid(std::string value) const {
    if (value.size() > m_maxLength) {
        return Err(fmt::format("At most {} characters", m_maxLength));
    }
    return Ok();
}

// ---- Registration and access ----

$execute {
    if (auto res = Mod::get()->registerCustomSettingType("avatar", &AvatarSetting::parse); !res) {
        log::error("Couldn't register the avatar setting: {}", res.unwrapErr());
    }
    if (auto res = Mod::get()->registerCustomSettingType("long-text", &LongTextSetting::parse); !res) {
        log::error("Couldn't register the long text setting: {}", res.unwrapErr());
    }
}

std::string settings::longText(std::string_view key) {
    if (auto setting = typeinfo_pointer_cast<LongTextSetting>(Mod::get()->getSetting(key))) {
        return setting->getValue();
    }
    return "";
}

AvatarLook settings::avatar() {
    return Mod::get()->getSettingValue<AvatarLook>("avatar");
}
