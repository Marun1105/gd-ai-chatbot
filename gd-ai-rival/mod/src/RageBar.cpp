#include "RageBar.hpp"
#include "Rage.hpp"
#include "RageEvent.hpp"

#include <random>

using namespace geode::prelude;

namespace {
    constexpr float WIDTH = 120.f;
    constexpr float HEIGHT = 9.f;

    ccColor3B colorFor(float level) {
        // Yellow, through orange, to deep red
        auto mix = [](ccColor3B a, ccColor3B b, float t) {
            return ccColor3B {
                static_cast<GLubyte>(a.r + (b.r - a.r) * t),
                static_cast<GLubyte>(a.g + (b.g - a.g) * t),
                static_cast<GLubyte>(a.b + (b.b - a.b) * t),
            };
        };
        constexpr ccColor3B YELLOW { 255, 220, 60 };
        constexpr ccColor3B ORANGE { 255, 130, 30 };
        constexpr ccColor3B RED { 220, 20, 25 };
        return level < .5f ? mix(YELLOW, ORANGE, level * 2) : mix(ORANGE, RED, (level - .5f) * 2);
    }
}

RageBar* RageBar::create() {
    auto ret = new RageBar();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool RageBar::init() {
    if (!CCNode::init()) return false;
    this->setID("rage-bar"_spr);
    this->setAnchorPoint({ 0.f, 1.f });
    this->setContentSize({ WIDTH + 30.f, 26.f });

    m_body = CCNode::create();
    this->addChild(m_body);

    m_face = RageEvent::createFace(24.f);
    m_face->setPosition({ 12.f, 13.f });
    m_body->addChild(m_face);

    auto frame = NineSlice::create("square02b_001.png");
    frame->setColor({ 0, 0, 0 });
    frame->setOpacity(160);
    frame->setScale(.25f);
    frame->setContentSize(CCSize(WIDTH + 4.f, HEIGHT + 4.f) / .25f);
    frame->setAnchorPoint({ 0.f, .5f });
    frame->setPosition({ 26.f, 9.f });
    m_body->addChild(frame);

    m_fill = CCLayerColor::create({ 255, 220, 60, 255 }, WIDTH, HEIGHT);
    m_fill->setPosition({ 28.f, 9.f - HEIGHT / 2 });
    m_fill->setScaleX(0.f);
    m_fill->ignoreAnchorPointForPosition(true);
    m_fill->setAnchorPoint({ 0.f, 0.f });
    m_body->addChild(m_fill);

    m_mood = CCLabelBMFont::create("calm", "bigFont.fnt");
    m_mood->setScale(.26f);
    m_mood->setAnchorPoint({ 0.f, .5f });
    m_mood->setPosition({ 28.f, 21.f });
    m_body->addChild(m_mood);

    this->refresh();
    this->scheduleUpdate();
    return true;
}

void RageBar::refresh(bool bump) {
    float level = rage::level();
    m_fill->stopAllActions();
    m_fill->runAction(CCEaseOut::create(CCScaleTo::create(.35f, std::max(level, .001f), 1.f), 2.f));
    m_fill->setColor(colorFor(level));
    m_mood->setString(rage::mood());
    m_mood->setColor(colorFor(level));
    if (bump) {
        m_face->stopAllActions();
        float base = 24.f / m_face->getContentSize().width;
        m_face->setScale(base * 1.35f);
        m_face->runAction(CCEaseBackOut::create(CCScaleTo::create(.25f, base)));
    }
}

void RageBar::update(float dt) {
    // Cool down only while actually playing: not paused, not dead
    auto playLayer = PlayLayer::get();
    if (playLayer && !playLayer->m_isPaused && playLayer->m_player1 && !playLayer->m_player1->m_isDead
        && !RageEvent::isActive()) {
        float before = rage::level();
        rage::coolDown(dt);
        if (std::abs(before - rage::level()) > .01f) {
            m_fill->setScaleX(std::max(rage::level(), .001f));
            m_fill->setColor(colorFor(rage::level()));
            m_mood->setString(rage::mood());
        }
    }

    // Trembles when it's close to snapping
    float level = rage::level();
    if (level >= .8f) {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> jitter(-1.f, 1.f);
        float strength = (level - .8f) * 6.f;
        m_body->setPosition({ jitter(gen) * strength, jitter(gen) * strength });
    }
    else {
        m_body->setPosition({ 0.f, 0.f });
    }
}
