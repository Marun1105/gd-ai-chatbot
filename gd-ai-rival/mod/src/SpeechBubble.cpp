#include "SpeechBubble.hpp"

using namespace geode::prelude;

namespace {
    constexpr float PADDING = 8.f;
    constexpr float BODY_SCALE = .7f;
    constexpr float FADE_TIME = .4f;
    // Above the pause menu, so replies stay visible while paused
    constexpr int BUBBLE_Z = 1000;
}

void SpeechBubble::show(std::string const& name, std::string const& text, bool isError) {
    attach(create(name, text, isError ? Style::Error : Style::Reply));
}

void SpeechBubble::showTyping(std::string const& name) {
    attach(create(name, "...", Style::Typing));
}

SpeechBubble* SpeechBubble::create(std::string const& name, std::string const& text, Style style) {
    auto ret = new SpeechBubble();
    if (ret->initBubble(name, text, style)) {
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

bool SpeechBubble::initBubble(std::string const& name, std::string const& text, Style style) {
    if (!CCNode::init()) return false;

    this->setID("speech-bubble"_spr);

    auto winSize = CCDirector::get()->getWinSize();
    float width = std::min(winSize.width - 40.f, 360.f);

    m_nameLabel = Label::create(name, "goldFont.fnt");
    m_nameLabel->setScale(.5f);
    m_nameLabel->setAnchorPoint({ 0.f, 1.f });

    m_bodyLabel = Label::create(text, "chatFont.fnt");
    m_bodyLabel->setAlignment(Label::Alignment::Left);
    m_bodyLabel->setScale(BODY_SCALE);
    m_bodyLabel->setMaxWidth((width - PADDING * 2) / BODY_SCALE);
    m_bodyLabel->setAnchorPoint({ 0.f, 1.f });
    switch (style) {
        case Style::Error: m_bodyLabel->setColor({ 255, 140, 140 }); break;
        case Style::Typing: m_bodyLabel->setColor({ 190, 190, 190 }); break;
        case Style::Reply: break;
    }

    float nameHeight = m_nameLabel->getScaledContentHeight();
    float bodyHeight = m_bodyLabel->getScaledContentHeight();
    float height = PADDING + nameHeight + 3.f + bodyHeight + PADDING;

    this->setContentSize({ width, height });
    this->setAnchorPoint({ .5f, .5f });

    m_bg = NineSlice::create("square02b_001.png");
    m_bg->setColor({ 0, 0, 0 });
    m_bg->setOpacity(170);
    m_bg->setScale(.4f);
    m_bg->setContentSize(CCSize(width, height) / .4f);
    m_bg->setAnchorPoint({ 0.f, 0.f });
    m_bg->setPosition({ 0.f, 0.f });
    this->addChild(m_bg);

    m_nameLabel->setPosition({ PADDING, height - PADDING });
    this->addChild(m_nameLabel);

    m_bodyLabel->setPosition({ PADDING, height - PADDING - nameHeight - 3.f });
    this->addChild(m_bodyLabel);

    bool atBottom = Mod::get()->getSettingValue<std::string>("bubble-position") == "Bottom";
    this->setPosition({
        winSize.width / 2,
        atBottom ? 18.f + height / 2 : winSize.height - 28.f - height / 2
    });

    this->setScale(.9f);
    this->runAction(CCEaseBackOut::create(CCScaleTo::create(.15f, 1.f)));

    float duration;
    if (style == Style::Typing) {
        // Just past the request timeout, in case the reply never arrives
        duration = 35.f;
    }
    else {
        float base = Mod::get()->getSettingValue<double>("bubble-duration");
        duration = std::min(base + text.size() * .04f, base + 8.f);
    }
    this->runAction(CCSequence::create(
        CCDelayTime::create(duration),
        CCCallFunc::create(this, callfunc_selector(SpeechBubble::fadeOut)),
        nullptr
    ));

    return true;
}

void SpeechBubble::fadeOut() {
    m_bg->runAction(CCFadeOut::create(FADE_TIME));
    m_nameLabel->runAction(CCFadeOut::create(FADE_TIME));
    m_bodyLabel->runAction(CCFadeOut::create(FADE_TIME));
    this->runAction(CCSequence::create(
        CCDelayTime::create(FADE_TIME),
        CCRemoveSelf::create(),
        nullptr
    ));
}
