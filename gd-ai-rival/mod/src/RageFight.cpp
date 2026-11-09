#include "RageFight.hpp"
#include "GrinLook.hpp"
#include "RageEvent.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

using namespace geode::prelude;

namespace {
    constexpr float CHARGE_TIME = 1.5f;
    constexpr int HITS_TO_STAGGER = 4;
    constexpr float BODY_DAMAGE = 7.f;
    constexpr float HEAD_DAMAGE = 11.f;
    constexpr float FIGHT_TIME = 25.f;

    std::mt19937& rng() {
        static std::mt19937 gen(std::random_device{}());
        return gen;
    }

    float randomIn(float a, float b) {
        return std::uniform_real_distribution<float>(a, b)(rng());
    }

    void playSound(std::string const& file, float speed = 1.f, float loudness = 1.f) {
        float volume = static_cast<float>(Mod::get()->getSettingValue<int64_t>("rage-volume")) / 100.f * loudness;
        if (volume > 0.f) FMODAudioEngine::get()->playEffect(file, speed, 0.f, volume);
    }

    CCLabelBMFont* popText(char const* text, char const* font, float scale, ccColor3B color) {
        auto label = CCLabelBMFont::create(text, font);
        label->setColor(color);
        label->setScale(scale * .5f);
        label->runAction(CCEaseBackOut::create(CCScaleTo::create(.25f, scale)));
        return label;
    }
}

RageFight* RageFight::create(std::function<void(Result)> onDone) {
    auto ret = new RageFight();
    if (ret->init(std::move(onDone))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool RageFight::init(std::function<void(Result)> onDone) {
    if (!CCNode::init()) return false;
    m_onDone = std::move(onDone);
    m_win = CCDirector::get()->getWinSize();
    m_timeLeft = FIGHT_TIME;
    this->setContentSize(m_win);

    // Everything that shakes when you're hit
    m_world = CCNode::create();
    this->addChild(m_world);

    // The world behind your broken screen, cropped to fill any screen shape
    if (auto arena = CCSprite::create("grin-arena.png"_spr)) {
        auto size = arena->getContentSize();
        arena->setScale(std::max(m_win.width / size.width, m_win.height / size.height) * 1.06f);
        arena->setPosition(m_win / 2);
        arena->setOpacity(0);
        arena->runAction(CCFadeIn::create(.4f));
        m_world->addChild(arena);
    }
    else {
        m_world->addChild(CCLayerColor::create({ 8, 4, 14, 255 }, m_win.width, m_win.height));
    }

    // Grin rises into view
    m_grin = grin_look::sprite(grin_look::Pose::Body, m_win.height * .76f);
    if (!m_grin) m_grin = RageEvent::createFace(m_win.height * .5f);
    m_baseScale = m_grin->getScale();
    m_grin->setPosition({ m_win.width / 2, -m_win.height * .4f });
    m_grin->runAction(CCEaseBackOut::create(CCMoveTo::create(.6f, { m_win.width / 2, m_win.height * .45f })));
    m_world->addChild(m_grin, 1);

    // The hit flash: the same picture drawn as a flat hot-white shape on top
    // (tinting alone barely shows on a creature this dark). GD's built-in
    // text shader does exactly that: its color, the picture's alpha. Built-in
    // means nothing is compiled here, and it survives a lost GL context.
    if (auto shader = CCShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionTextureA8Color)) {
        m_grinFlash = CCSprite::createWithTexture(m_grin->getTexture());
        m_grinFlash->setFlipY(m_grin->isFlipY());
        m_grinFlash->setShaderProgram(shader);
        m_grinFlash->setOpacityModifyRGB(false);
        m_grinFlash->setBlendFunc({ GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA });
        m_grinFlash->setColor({ 255, 236, 200 });
        m_grinFlash->setOpacity(0);
        m_grinFlash->setAnchorPoint({ 0.f, 0.f });
        m_grinFlash->setPosition({ 0.f, 0.f });
        m_grin->addChild(m_grinFlash);
    }

    m_effects = CCNode::create();
    this->addChild(m_effects, 2);

    m_screenCracks = CCDrawNode::create();
    this->addChild(m_screenCracks, 3);

    m_hurtFlash = CCLayerColor::create({ 200, 0, 0, 0 }, m_win.width, m_win.height);
    this->addChild(m_hurtFlash, 4);

    // ---- HUD ----
    auto name = CCLabelBMFont::create("GRIN", "goldFont.fnt");
    name->setScale(.55f);
    name->setPosition({ m_win.width / 2, m_win.height - 14.f });
    this->addChild(name, 5);

    auto hpBack = CCLayerColor::create({ 0, 0, 0, 190 }, 204.f, 11.f);
    hpBack->setPosition({ m_win.width / 2 - 102.f, m_win.height - 36.f });
    this->addChild(hpBack, 5);
    m_hpFill = CCLayerColor::create({ 235, 45, 30, 255 }, 200.f, 7.f);
    m_hpFill->setAnchorPoint({ 0.f, 0.f });
    m_hpFill->setPosition({ m_win.width / 2 - 100.f, m_win.height - 34.f });
    this->addChild(m_hpFill, 6);

    m_hearts = CCNode::create();
    m_hearts->setPosition({ 18.f, 18.f });
    this->addChild(m_hearts, 5);

    m_timer = CCLabelBMFont::create("25", "bigFont.fnt");
    m_timer->setScale(.45f);
    m_timer->setAnchorPoint({ 1.f, 1.f });
    m_timer->setPosition({ m_win.width - 12.f, m_win.height - 8.f });
    this->addChild(m_timer, 5);

    auto hint = CCLabelBMFont::create("TAP GRIN TO FIGHT BACK!", "bigFont.fnt");
    hint->setScale(.5f);
    hint->setPosition({ m_win.width / 2, m_win.height * .14f });
    hint->runAction(CCSequence::create(
        CCRepeat::create(CCSequence::create(CCFadeTo::create(.35f, 120), CCFadeTo::create(.35f, 255), nullptr), 3),
        CCFadeOut::create(.3f), CCRemoveSelf::create(), nullptr
    ));
    this->addChild(hint, 5);

    this->refreshHud();
    playSound("roar.wav"_spr, 1.15f, .8f);
    this->runAction(CCSequence::create(
        CCDelayTime::create(.8f), CallFuncExt::create([this] { this->begin(); }), nullptr
    ));
    this->scheduleUpdate();
    return true;
}

void RageFight::begin() {
    m_state = State::Fight;
    auto fight = popText("FIGHT!", "goldFont.fnt", 1.1f, { 255, 255, 255 });
    fight->setPosition(m_win / 2 + CCPoint(0.f, m_win.height * .2f));
    fight->runAction(CCSequence::create(CCDelayTime::create(.5f), CCFadeOut::create(.25f), CCRemoveSelf::create(), nullptr));
    this->addChild(fight, 7);
}

// ---- Shooting ----

void RageFight::shoot(CCPoint location) {
    if (m_state != State::Fight) return;
    auto local = this->convertToNodeSpace(location);
    playSound("shot.wav"_spr, randomIn(.94f, 1.06f), .7f);
    this->shakeScreen(2.f, .08f);

    // Where the shot lands
    auto ring = CCDrawNode::create();
    for (int i = 0; i < 16; i++) {
        float a = 6.2832f * i / 16, b = 6.2832f * (i + 1) / 16;
        ring->drawSegment({ std::cos(a) * 10.f, std::sin(a) * 10.f }, { std::cos(b) * 10.f, std::sin(b) * 10.f }, .8f, ccc4f(1.f, 1.f, 1.f, .9f));
    }
    ring->drawDot({ 0.f, 0.f }, 3.f, ccc4f(1.f, .95f, .7f, 1.f));
    ring->setPosition(local);
    ring->setScale(.6f);
    ring->runAction(CCSequence::create(CCScaleTo::create(.15f, 1.5f), CCRemoveSelf::create(), nullptr));
    m_effects->addChild(ring);

    // Hit test in the picture's own coordinates (0..1, measured from the
    // render): the head, or the thin body
    auto size = m_grin->getContentSize();
    auto onGrin = m_grin->convertToNodeSpace(location);
    float hx = onGrin.x / size.width, hy = onGrin.y / size.height;
    bool head = false, body = false;
    if (grin_look::usingOwnPicture()) {
        // Someone's own picture: the middle is the weak spot, anywhere else on it still hits
        float dx = (hx - .5f) / .22f, dy = (hy - .55f) / .22f;
        head = dx * dx + dy * dy < 1.f;
        body = hx > 0.f && hx < 1.f && hy > 0.f && hy < 1.f;
    }
    else {
        // Measured on grin-full.png (y up): the head, then the torso
        float dx = (hx - .555f) / .075f, dy = (hy - .905f) / .07f;
        head = dx * dx + dy * dy < 1.f;
        body = hx > .42f && hx < .72f && hy > .45f && hy < .86f;
    }
    if (head || body) {
        m_hits++;
        this->damageGrin(head ? HEAD_DAMAGE : BODY_DAMAGE, local);
        if (m_charging && ++m_chargeHits >= HITS_TO_STAGGER && m_state == State::Fight) this->stagger();
    }
}

void RageFight::damageGrin(float amount, CCPoint where) {
    m_grinHealth = std::max(0.f, m_grinHealth - amount);
    m_tint = .1f;
    if (m_grinFlash) {
        m_grinFlash->stopAllActions();
        m_grinFlash->setOpacity(amount >= HEAD_DAMAGE ? 210 : 150);
        m_grinFlash->runAction(CCFadeOut::create(.12f));
    }
    playSound("hit.wav"_spr, randomIn(.92f, 1.08f), .8f);
    this->sparks(where, ccc4f(1.f, .85f, .2f, 1.f), 7);
    this->refreshHud();
    if (m_grinHealth <= 0.f) this->win();
}

void RageFight::sparks(CCPoint where, ccColor4F color, int count) {
    for (int i = 0; i < count; i++) {
        auto spark = CCLayerColor::create({
            static_cast<GLubyte>(color.r * 255), static_cast<GLubyte>(color.g * 255),
            static_cast<GLubyte>(color.b * 255), 255,
        }, 3.f, 3.f);
        spark->setPosition(where);
        float angle = randomIn(0.f, 6.2832f), distance = randomIn(25.f, 75.f);
        spark->runAction(CCSequence::create(
            CCSpawn::create(
                CCEaseOut::create(CCMoveBy::create(.35f, { std::cos(angle) * distance, std::sin(angle) * distance }), 2.f),
                CCFadeOut::create(.35f), nullptr
            ),
            CCRemoveSelf::create(), nullptr
        ));
        m_effects->addChild(spark);
    }
}

// ---- Grin's charges ----

void RageFight::startCharge() {
    m_charging = true;
    m_chargeTime = 0.f;
    m_chargeHits = 0;
    playSound("roar.wav"_spr, 1.3f, .55f);
}

void RageFight::stagger() {
    m_charging = false;
    m_nextCharge = 2.f + 1.2f * (m_grinHealth / 100.f);
    playSound("hit.wav"_spr, .75f, 1.f);
    this->shakeScreen(4.f, .15f);
    auto label = popText("KNOCKED BACK!", "bigFont.fnt", .45f, { 255, 230, 120 });
    label->setPosition(m_grin->getPosition() + CCPoint(0.f, m_win.height * .25f));
    label->runAction(CCSequence::create(CCDelayTime::create(.45f), CCFadeOut::create(.2f), CCRemoveSelf::create(), nullptr));
    this->addChild(label, 7);
    this->damageGrin(6.f, m_grin->getPosition());
}

void RageFight::hurtPlayer() {
    m_charging = false;
    m_nextCharge = 2.f + 1.2f * (m_grinHealth / 100.f);
    m_playerHearts = std::max(0, m_playerHearts - 1);
    playSound("crack.wav"_spr, .9f, 1.f);
    this->shakeScreen(9.f, .3f);
    m_hurtFlash->stopAllActions();
    m_hurtFlash->setOpacity(130);
    m_hurtFlash->runAction(CCFadeTo::create(.45f, 0));

    // Its claws crack your screen a little more each time
    CCPoint center { randomIn(m_win.width * .2f, m_win.width * .8f), randomIn(m_win.height * .25f, m_win.height * .75f) };
    for (int i = 0; i < 7; i++) {
        float angle = randomIn(0.f, 6.2832f);
        CCPoint from = center;
        float length = randomIn(40.f, 110.f);
        for (int k = 0; k < 3; k++) {
            angle += randomIn(-.35f, .35f);
            CCPoint to = from + CCPoint(std::cos(angle), std::sin(angle)) * (length / 3);
            m_screenCracks->drawSegment(from, to, .7f, ccc4f(1.f, 1.f, 1.f, .7f));
            from = to;
        }
    }
    this->refreshHud();
    if (m_playerHearts <= 0) this->lose();
}

// ---- Endings ----

void RageFight::win() {
    if (m_state == State::Over) return;
    m_state = State::Over;
    m_won = true;
    m_charging = false;
    playSound("shatter.wav"_spr, .85f, 1.f);
    this->sparks(m_grin->getPosition() + CCPoint(0.f, m_grin->getScaledContentSize().height * .2f), ccc4f(1.f, .8f, .1f, 1.f), 40);
    m_grin->stopAllActions();
    m_grin->runAction(CCSpawn::create(
        CCScaleTo::create(.5f, m_grin->getScale() * 1.35f), CCFadeOut::create(.5f), nullptr
    ));
    auto label = popText("YOU BEAT GRIN!", "goldFont.fnt", .9f, { 255, 255, 255 });
    label->setPosition(m_win / 2);
    this->addChild(label, 7);
    this->runAction(CCSequence::create(CCDelayTime::create(1.6f), CallFuncExt::create([this] { this->done(); }), nullptr));
}

void RageFight::lose() {
    if (m_state == State::Over) return;
    m_state = State::Over;
    m_won = false;
    m_charging = false;
    playSound("roar.wav"_spr, .9f, 1.f);
    m_hurtFlash->stopAllActions();
    m_hurtFlash->runAction(CCFadeTo::create(.4f, 150));

    // Right in your face
    auto face = grin_look::sprite(grin_look::Pose::Roar, m_win.height * 1.15f);
    if (!face) face = RageEvent::createFace(m_win.height * 1.15f);
    float target = face->getScale();
    face->setScale(target * .3f);
    face->setPosition(m_win / 2);
    face->runAction(CCEaseIn::create(CCScaleTo::create(.45f, target), 2.f));
    this->addChild(face, 6);

    auto label = popText("GRIN WINS", "goldFont.fnt", .9f, { 255, 80, 60 });
    label->setPosition({ m_win.width / 2, m_win.height * .15f });
    this->addChild(label, 7);
    this->runAction(CCSequence::create(CCDelayTime::create(1.6f), CallFuncExt::create([this] { this->done(); }), nullptr));
}

void RageFight::done() {
    if (!m_onDone) return;
    auto onDone = std::move(m_onDone);
    m_onDone = nullptr;
    onDone({ m_won, m_hits });
}

// ---- Every frame ----

void RageFight::update(float dt) {
    m_time += dt;

    if (m_state == State::Fight) {
        m_timeLeft -= dt;
        m_timer->setString(std::to_string(std::max(0, static_cast<int>(std::ceil(m_timeLeft)))).c_str());
        if (m_timeLeft <= 0.f) {
            this->lose();
            return;
        }

        if (m_charging) {
            m_chargeTime += dt;
            float progress = std::min(1.f, m_chargeTime / CHARGE_TIME);
            if (progress >= 1.f) this->hurtPlayer();
        }
        else {
            m_nextCharge -= dt;
            if (m_nextCharge <= 0.f) this->startCharge();
        }
    }

    // Drift around, and grow huge while charging at you
    if (m_state != State::Over && m_grin->numberOfRunningActions() == 0) {
        float progress = m_charging ? std::min(1.f, m_chargeTime / CHARGE_TIME) : 0.f;
        CCPoint drift {
            m_win.width / 2 + std::sin(m_time * 1.1f) * m_win.width * .18f * (1.f - progress),
            m_win.height * .45f + std::sin(m_time * 2.3f) * m_win.height * .03f - progress * m_win.height * .08f,
        };
        m_grin->setPosition(drift);
        float breathe = 1.f + .03f * std::sin(m_time * 4.f);
        m_grin->setScale(m_baseScale * breathe * (1.f + 1.7f * progress * progress));
    }

    // Red when hit, pulsing red while charging
    m_tint = std::max(0.f, m_tint - dt);
    if (m_state != State::Over) {
        if (m_tint > 0.f) {
            m_grin->setColor({ 255, 110, 110 });
        }
        else if (m_charging) {
            auto g = static_cast<GLubyte>(170 + 85 * (.5f + .5f * std::sin(m_time * 22.f)));
            m_grin->setColor({ 255, g, g });
        }
        else {
            m_grin->setColor({ 255, 255, 255 });
        }
    }

    // Shake
    if (m_shakeTime > 0.f) {
        m_shakeTime -= dt;
        m_world->setPosition({ randomIn(-m_shakeStrength, m_shakeStrength), randomIn(-m_shakeStrength, m_shakeStrength) });
    }
    else {
        m_world->setPosition({ 0.f, 0.f });
    }
}

void RageFight::shakeScreen(float strength, float time) {
    m_shakeStrength = std::max(strength, m_shakeTime > 0.f ? m_shakeStrength : 0.f);
    m_shakeTime = std::max(m_shakeTime, time);
}

// ---- HUD ----

CCNode* RageFight::heart(bool full) {
    auto node = CCDrawNode::create();
    auto color = full ? ccc4f(.92f, .12f, .18f, 1.f) : ccc4f(.3f, .3f, .32f, .8f);
    node->drawDot({ -4.f, 3.f }, 5.f, color);
    node->drawDot({ 4.f, 3.f }, 5.f, color);
    std::array<CCPoint, 3> tip { CCPoint(-8.6f, 1.2f), CCPoint(8.6f, 1.2f), CCPoint(0.f, -9.f) };
    node->drawPolygon(tip.data(), 3, color, 0.f, color);
    return node;
}

void RageFight::refreshHud() {
    m_hpFill->setScaleX(std::max(.001f, m_grinHealth / 100.f));
    m_hearts->removeAllChildren();
    for (int i = 0; i < 3; i++) {
        auto h = this->heart(i < m_playerHearts);
        h->setPosition({ i * 24.f, 0.f });
        m_hearts->addChild(h);
    }
}
