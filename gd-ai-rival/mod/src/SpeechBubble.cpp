#include "SpeechBubble.hpp"
#include "Rage.hpp"
#include "RageEvent.hpp"

#include <random>

using namespace geode::prelude;

namespace {
    constexpr float PADDING = 8.f;
    constexpr float AVATAR_SIZE = 30.f;
    constexpr float FADE_TIME = .4f;
    // About 40 letters a second, quick enough not to hold up a fast level
    constexpr float REVEAL_INTERVAL = .025f;
    constexpr size_t BLIP_EVERY = 3;
    // Above the pause menu, so replies stay visible while paused
    constexpr int BUBBLE_Z = 1000;

    std::mt19937& rng() {
        static std::mt19937 gen(std::random_device{}());
        return gen;
    }
}

void SpeechBubble::show(Profile const& profile, std::string const& text, bool isError) {
    attach(create(profile, text, isError ? Style::Error : Style::Reply));
}

void SpeechBubble::showTyping(Profile const& profile) {
    attach(create(profile, "...", Style::Typing));
}

SpeechBubble* SpeechBubble::createPreview(Profile const& profile, std::string const& text, bool animate) {
    auto ret = new SpeechBubble();
    ret->m_preview = true;
    ret->m_animate = animate;
    if (ret->initBubble(profile, text, Style::Reply)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

SpeechBubble* SpeechBubble::create(Profile const& profile, std::string const& text, Style style) {
    auto ret = new SpeechBubble();
    if (ret->initBubble(profile, text, style)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void SpeechBubble::attach(SpeechBubble* bubble) {
    // Attach to the level's scene rather than the running scene, which may
    // be a transition or another layer when the reply arrives
    auto playLayer = PlayLayer::get();
    if (!bubble || !playLayer || !playLayer->getParent()) return;

    auto scene = playLayer->getParent();
    if (auto old = scene->getChildByID("speech-bubble"_spr)) {
        old->removeFromParent();
    }
    scene->addChild(bubble, BUBBLE_Z);
}

CCNode* SpeechBubble::createAvatar(BubbleStyle const& style) {
    return avatar::create(style.avatar, AVATAR_SIZE);
}

bool SpeechBubble::initBubble(Profile const& profile, std::string const& text, Style style) {
    if (!CCNode::init()) return false;

    this->setID("speech-bubble"_spr);
    auto const& look = profile.bubble;
    auto winSize = CCDirector::get()->getWinSize();

    float width = std::clamp(look.width, 150.f, winSize.width - 20.f);
    bool hasAvatar = look.avatar.mode != AvatarMode::Off;
    float textLeft = PADDING + (hasAvatar ? AVATAR_SIZE + PADDING : 0.f);
    float textWidth = width - textLeft - PADDING;
    float textScale = std::clamp(look.textScale, .3f, 1.5f);

    m_nameLabel = Label::create(profile.name, "goldFont.fnt");
    m_nameLabel->setScale(.5f);
    m_nameLabel->setAnchorPoint({ 0.f, 1.f });
    m_nameLabel->setColor(look.nameColor);

    // Typing dots reserve room for "..." so the bubble doesn't resize as they cycle
    m_bodyLabel = Label::create(text, look.font);
    m_bodyLabel->setAlignment(Label::Alignment::Left);
    m_bodyLabel->setScale(textScale);
    m_bodyLabel->setMaxWidth(textWidth / textScale);
    m_bodyLabel->setAnchorPoint({ 0.f, 1.f });
    switch (style) {
        case Style::Error: m_textColor = { 255, 140, 140 }; break;
        case Style::Typing: m_textColor = { 190, 190, 190 }; break;
        case Style::Reply: m_textColor = look.textColor; break;
    }
    m_bodyLabel->setColor(m_textColor);

    float nameHeight = m_nameLabel->getScaledContentHeight();
    float bodyHeight = m_bodyLabel->getScaledContentHeight();
    float height = std::max(
        PADDING + nameHeight + 3.f + bodyHeight + PADDING,
        hasAvatar ? AVATAR_SIZE + PADDING * 2 : 0.f
    );

    this->setContentSize({ width, height });
    this->setAnchorPoint({ .5f, .5f });

    m_bg = NineSlice::create("square02b_001.png");
    m_bg->setColor({ look.background.r, look.background.g, look.background.b });
    m_bg->setOpacity(look.background.a);
    m_bg->setScale(.4f);
    m_bg->setContentSize(CCSize(width, height) / .4f);
    m_bg->setAnchorPoint({ 0.f, 0.f });
    m_bg->setPosition({ 0.f, 0.f });
    this->addChild(m_bg);

    if (hasAvatar) {
        // Grin shows its own face (unless the player picked a drawing or their
        // own icon), going redder the angrier it gets
        if (profile.character == "grin" && look.avatar.mode == AvatarMode::Icon) {
            auto face = RageEvent::createFace(AVATAR_SIZE);
            float anger = rage::level();
            face->setColor({ 255, static_cast<GLubyte>(255 - 150 * anger), static_cast<GLubyte>(255 - 150 * anger) });
            m_avatar = face;
        }
        else {
            m_avatar = this->createAvatar(look);
        }
        if (m_avatar) {
            m_avatar->setPosition({ PADDING + AVATAR_SIZE / 2, height / 2 });
            this->addChild(m_avatar);
        }
    }

    m_nameLabel->setPosition({ textLeft, height - PADDING });
    this->addChild(m_nameLabel);

    m_bodyLabel->setPosition({ textLeft, height - PADDING - nameHeight - 3.f });
    this->addChild(m_bodyLabel);

    this->place(look);
    if (!m_preview || m_animate) {
        this->setScale(.9f);
        this->runAction(CCEaseBackOut::create(CCScaleTo::create(.15f, 1.f)));
    }

    m_soundFile = style == Style::Reply ? profile.soundFile : "";
    m_volume = std::clamp(profile.soundVolume, 0.f, 1.f);

    float base = std::clamp(look.duration, 1.f, 20.f);
    float readTime = std::min(base + text.size() * .04f, base + 8.f);

    if (style == Style::Typing) {
        m_bodyLabel->setText(".");
        this->schedule(schedule_selector(SpeechBubble::typingStep), .3f);
        // Just past the request timeout, in case the reply never arrives
        this->startTimer(35.f);
    }
    else if (m_preview && !m_animate) {
        // Designer preview while editing: the whole line at once, silently
    }
    else if (style == Style::Reply && look.typewriter) {
        // Hide every letter, then fade them in one by one; changing colours
        // instead of text keeps the line wrapping from jumping around
        m_charCount = m_bodyLabel->getCharCount();
        for (size_t i = 0; i < m_charCount; i++) {
            m_bodyLabel->setCharColor(i, { m_textColor.r, m_textColor.g, m_textColor.b, 0 });
        }
        m_holdTime = readTime;
        this->schedule(schedule_selector(SpeechBubble::revealStep), REVEAL_INTERVAL);
    }
    else {
        this->playBlip();
        this->startTimer(readTime);
    }
    return true;
}

CCPoint SpeechBubble::anchorPosition(BubbleAnchor anchor, CCSize size, CCSize screen) {
    float left = 10.f + size.width / 2;
    float right = screen.width - 10.f - size.width / 2;
    float center = screen.width / 2;
    // Top positions leave room for GD's progress bar
    float top = screen.height - 28.f - size.height / 2;
    float bottom = 18.f + size.height / 2;

    switch (anchor) {
        case BubbleAnchor::Bottom: return ccp(center, bottom);
        case BubbleAnchor::TopLeft: return ccp(left, top);
        case BubbleAnchor::TopRight: return ccp(right, top);
        case BubbleAnchor::BottomLeft: return ccp(left, bottom);
        case BubbleAnchor::BottomRight: return ccp(right, bottom);
        case BubbleAnchor::Top: default: return ccp(center, top);
    }
}

void SpeechBubble::place(BubbleStyle const& look) {
    auto winSize = CCDirector::get()->getWinSize();
    auto size = this->getContentSize();

    auto pos = anchorPosition(look.anchor, size, winSize);
    pos.x += look.offsetX;
    pos.y += look.offsetY;

    // Offsets can't push the bubble off screen
    pos.x = std::clamp(pos.x, size.width / 2, std::max(size.width / 2, winSize.width - size.width / 2));
    pos.y = std::clamp(pos.y, size.height / 2, std::max(size.height / 2, winSize.height - size.height / 2));
    this->setPosition(pos);
}

void SpeechBubble::revealStep(float) {
    if (m_revealed < m_charCount) {
        m_bodyLabel->setCharColor(m_revealed, { m_textColor.r, m_textColor.g, m_textColor.b, 255 });
        if (m_revealed % BLIP_EVERY == 0) {
            this->playBlip();
        }
        m_revealed++;
        return;
    }
    this->unschedule(schedule_selector(SpeechBubble::revealStep));
    this->startTimer(m_holdTime);
}

void SpeechBubble::typingStep(float) {
    m_typingDots = m_typingDots % 3 + 1;
    m_bodyLabel->setText(std::string(m_typingDots, '.'));
    m_bodyLabel->setColor(m_textColor);
}

void SpeechBubble::playBlip() {
    if (m_soundFile.empty() || m_volume <= 0.f) return;
    // A little pitch wobble makes the blips sound like a voice instead of a beep
    std::uniform_real_distribution<float> speed(.94f, 1.08f);
    FMODAudioEngine::get()->playEffect(m_soundFile, speed(rng()), 0.f, m_volume);
}

void SpeechBubble::startTimer(float seconds) {
    // Previews stay until the designer replaces them
    if (m_preview) return;
    this->runAction(CCSequence::create(
        CCDelayTime::create(seconds),
        CCCallFunc::create(this, callfunc_selector(SpeechBubble::fadeOut)),
        nullptr
    ));
}

void SpeechBubble::fadeOut() {
    this->unscheduleAllSelectors();
    for (auto node : std::initializer_list<CCNode*> { m_bg, m_nameLabel, m_bodyLabel, m_avatar }) {
        if (node) node->runAction(CCFadeOut::create(FADE_TIME));
    }
    this->runAction(CCSequence::create(
        CCDelayTime::create(FADE_TIME),
        CCRemoveSelf::create(),
        nullptr
    ));
}
