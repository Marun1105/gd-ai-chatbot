#include "RageFight.hpp"
#include "GrinLook.hpp"
#include "RageEvent.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

using namespace geode::prelude;

namespace {
    constexpr float MAX_HEALTH = 260.f;
    constexpr int PLAYER_HEARTS = 4;
    constexpr float FIGHT_TIME = 35.f;
    constexpr float HEAD_DAMAGE = 10.f;
    constexpr float BODY_DAMAGE = 5.f;
    constexpr float STAGGER_DAMAGE = 12.f;
    // Taps faster than this don't fire, so mashing isn't the whole strategy
    constexpr float SHOT_COOLDOWN = .11f;
    constexpr float DASH_TIME = .22f;
    constexpr float SHARD_TIME = 1.35f;
    constexpr float PI2 = 6.2832f;

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

    // The same picture drawn as a flat shape in one color (GD's built-in text
    // shader: its color, the picture's alpha). For the hit flash, the glow
    // and afterimages; nothing is compiled, and it survives a lost GL context.
    CCSprite* silhouetteOf(CCSprite* source, ccColor3B color, bool additive) {
        auto shader = CCShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionTextureA8Color);
        if (!shader || !source || !source->getTexture()) return nullptr;
        auto sprite = CCSprite::createWithTexture(source->getTexture());
        sprite->setFlipY(source->isFlipY());
        sprite->setShaderProgram(shader);
        sprite->setOpacityModifyRGB(false);
        sprite->setBlendFunc({ GL_SRC_ALPHA, static_cast<GLenum>(additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA) });
        sprite->setColor(color);
        return sprite;
    }

    GLubyte toByte(float value) {
        return static_cast<GLubyte>(std::clamp(value, 0.f, 255.f));
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
    m_grinHealth = MAX_HEALTH;
    m_playerHearts = PLAYER_HEARTS;
    m_timeLeft = FIGHT_TIME;
    m_homeX = m_win.width / 2;
    this->setContentSize(m_win);

    // Everything that shakes when you're hit
    m_world = CCNode::create();
    this->addChild(m_world, 0);

    // The world behind your broken screen, cropped to fill any screen shape
    m_arena = CCSprite::create("grin-arena.png"_spr);
    if (m_arena) {
        auto size = m_arena->getContentSize();
        m_arena->setScale(std::max(m_win.width / size.width, m_win.height / size.height) * 1.08f);
        m_arena->setPosition(m_win / 2);
        m_arena->setOpacity(0);
        m_arena->runAction(CCFadeIn::create(.4f));
        m_world->addChild(m_arena, -2);
    }
    else {
        m_world->addChild(CCLayerColor::create({ 8, 4, 14, 255 }, m_win.width, m_win.height), -2);
    }

    // Embers drifting up through the arena
    m_embers = CCNode::create();
    m_world->addChild(m_embers, -1);

    // Grin rises into view
    m_grin = grin_look::sprite(grin_look::Pose::Body, m_win.height * .76f);
    if (!m_grin) m_grin = RageEvent::createFace(m_win.height * .5f);
    m_baseScale = m_grin->getScale();
    m_grin->setCascadeOpacityEnabled(true);
    m_grin->setPosition({ m_homeX, -m_win.height * .4f });
    m_grin->runAction(CCEaseBackOut::create(CCMoveTo::create(.6f, { m_homeX, m_win.height * .45f })));
    m_world->addChild(m_grin, 1);

    auto content = m_grin->getContentSize();
    // A red glow around it while it charges, and all the time once it's enraged
    m_aura = silhouetteOf(m_grin, { 255, 45, 25 }, true);
    if (m_aura) {
        m_aura->setPosition(content / 2);
        m_aura->setScale(1.07f);
        m_aura->setOpacity(0);
        m_grin->addChild(m_aura, -1);
    }
    // The hit flash: a hot white shape on top (tinting barely shows on a creature this dark)
    m_grinFlash = silhouetteOf(m_grin, { 255, 240, 210 }, false);
    if (m_grinFlash) {
        m_grinFlash->setPosition(content / 2);
        m_grinFlash->setOpacity(0);
        m_grin->addChild(m_grinFlash, 1);
    }

    m_shardLayer = CCNode::create();
    this->addChild(m_shardLayer, 2);

    m_effects = CCNode::create();
    this->addChild(m_effects, 3);

    m_screenCracks = CCDrawNode::create();
    this->addChild(m_screenCracks, 4);

    m_hurtFlash = CCLayerColor::create({ 200, 0, 0, 0 }, m_win.width, m_win.height);
    this->addChild(m_hurtFlash, 5);

    // Red edges: a charge coming, low on hearts
    float bandX = m_win.width * .2f, bandY = m_win.height * .25f;
    ccColor4B red { 190, 10, 0, 255 }, clear { 190, 10, 0, 0 };
    struct Band { CCSize size; CCPoint position; CCPoint direction; };
    for (auto const& band : {
        Band { { bandX, m_win.height }, { 0.f, 0.f }, { 1.f, 0.f } },
        Band { { bandX, m_win.height }, { m_win.width - bandX, 0.f }, { -1.f, 0.f } },
        Band { { m_win.width, bandY }, { 0.f, 0.f }, { 0.f, 1.f } },
        Band { { m_win.width, bandY }, { 0.f, m_win.height - bandY }, { 0.f, -1.f } },
    }) {
        auto gradient = CCLayerGradient::create(red, clear, band.direction);
        gradient->setContentSize(band.size);
        gradient->setPosition(band.position);
        gradient->setOpacity(0);
        this->addChild(gradient, 5);
        m_vignette.push_back(gradient);
    }

    // ---- HUD ----
    auto name = CCLabelBMFont::create("GRIN", "goldFont.fnt");
    name->setScale(.55f);
    name->setPosition({ m_win.width / 2, m_win.height - 14.f });
    this->addChild(name, 6);

    auto hpBack = CCLayerColor::create({ 0, 0, 0, 190 }, 204.f, 11.f);
    hpBack->setPosition({ m_win.width / 2 - 102.f, m_win.height - 36.f });
    this->addChild(hpBack, 6);
    m_hpFill = CCLayerColor::create({ 235, 45, 30, 255 }, 200.f, 7.f);
    m_hpFill->setAnchorPoint({ 0.f, 0.f });
    m_hpFill->ignoreAnchorPointForPosition(false);
    m_hpFill->setPosition({ m_win.width / 2 - 100.f, m_win.height - 34.f });
    this->addChild(m_hpFill, 7);

    m_rageLabel = CCLabelBMFont::create("ENRAGED", "bigFont.fnt");
    m_rageLabel->setScale(.3f);
    m_rageLabel->setColor({ 255, 70, 50 });
    m_rageLabel->setPosition({ m_win.width / 2, m_win.height - 48.f });
    m_rageLabel->setVisible(false);
    this->addChild(m_rageLabel, 6);

    m_hearts = CCNode::create();
    m_hearts->setPosition({ 18.f, 18.f });
    this->addChild(m_hearts, 6);

    m_timer = CCLabelBMFont::create(std::to_string(static_cast<int>(FIGHT_TIME)).c_str(), "bigFont.fnt");
    m_timer->setScale(.45f);
    m_timer->setAnchorPoint({ 1.f, 1.f });
    m_timer->setPosition({ m_win.width - 12.f, m_win.height - 8.f });
    this->addChild(m_timer, 6);

    auto hint = CCLabelBMFont::create("TAP GRIN TO FIGHT BACK! THE HEAD HURTS MORE", "bigFont.fnt");
    hint->setScale(.4f);
    hint->setPosition({ m_win.width / 2, m_win.height * .14f });
    hint->runAction(CCSequence::create(
        CCRepeat::create(CCSequence::create(CCFadeTo::create(.35f, 120), CCFadeTo::create(.35f, 255), nullptr), 3),
        CCFadeOut::create(.3f), CCRemoveSelf::create(), nullptr
    ));
    this->addChild(hint, 7);

    this->refreshHud();
    playSound("grin-nowhere.ogg"_spr);
    for (int i = 0; i < 14; i++) this->spawnEmber();
    this->runAction(CCSequence::create(
        CCDelayTime::create(.8f), CallFuncExt::create([this] { this->begin(); }), nullptr
    ));
    this->scheduleUpdate();
    return true;
}

void RageFight::begin() {
    m_state = State::Fight;
    m_moveTime = 0.f;
    auto fight = popText("FIGHT!", "goldFont.fnt", 1.1f, { 255, 255, 255 });
    fight->setPosition(m_win / 2 + CCPoint(0.f, m_win.height * .2f));
    fight->runAction(CCSequence::create(CCDelayTime::create(.5f), CCFadeOut::create(.25f), CCRemoveSelf::create(), nullptr));
    this->addChild(fight, 8);
}

float RageFight::chargeDuration() const {
    return m_enraged ? 1.05f : 1.35f;
}

int RageFight::hitsToStagger() const {
    return m_enraged ? 6 : 5;
}

// ---- Shooting ----

void RageFight::shoot(CCPoint location) {
    if (m_state != State::Fight || m_time - m_lastShot < SHOT_COOLDOWN) return;
    m_lastShot = m_time;
    auto local = this->convertToNodeSpace(location);
    playSound("shot.ogg"_spr, randomIn(.94f, 1.06f));

    // Where the shot lands: a ring and a bright pop
    auto ring = CCDrawNode::create();
    for (int i = 0; i < 16; i++) {
        float a = PI2 * i / 16, b = PI2 * (i + 1) / 16;
        ring->drawSegment({ std::cos(a) * 10.f, std::sin(a) * 10.f }, { std::cos(b) * 10.f, std::sin(b) * 10.f }, .8f, ccc4f(1.f, 1.f, 1.f, .9f));
    }
    ring->drawDot({ 0.f, 0.f }, 4.f, ccc4f(1.f, .95f, .7f, 1.f));
    ring->setPosition(local);
    ring->setScale(.6f);
    ring->runAction(CCSequence::create(
        CCSpawn::create(CCScaleTo::create(.15f, 1.6f), CCFadeOut::create(.15f), nullptr), CCRemoveSelf::create(), nullptr
    ));
    m_effects->addChild(ring);

    // Glass flying at you comes first
    if (this->shootShard(local)) return;

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
    if (!head && !body) return;

    m_hits++;
    this->damageGrin(head ? HEAD_DAMAGE : BODY_DAMAGE, head, local);
    if (m_charging && m_state == State::Fight && ++m_chargeHits >= this->hitsToStagger()) this->stagger();
}

void RageFight::damageGrin(float amount, bool head, CCPoint where) {
    if (m_state == State::Over) return;
    m_grinHealth = std::max(0.f, m_grinHealth - amount);
    if (m_grinFlash) {
        m_grinFlash->stopAllActions();
        m_grinFlash->setOpacity(head ? 230 : 160);
        m_grinFlash->runAction(CCFadeOut::create(.14f));
    }
    playSound("hit.ogg"_spr, randomIn(.92f, 1.08f));
    // It grunts when it really hurts, but not on every single shot
    if (head && m_time - m_lastGrunt > .7f) {
        m_lastGrunt = m_time;
        playSound("grin-hurt1.ogg"_spr, randomIn(.95f, 1.08f));
    }
    this->sparks(where, ccc4f(1.f, .85f, .2f, 1.f), head ? 12 : 7);
    this->sparks(where, ccc4f(1.f, .35f, .1f, 1.f), head ? 6 : 3, 45.f, .5f);
    this->damageNumber(static_cast<int>(amount), head, where);
    this->refreshHud();
    if (m_grinHealth <= 0.f) this->win();
    else if (!m_enraged && m_grinHealth <= MAX_HEALTH / 2) this->enrage();
}

void RageFight::sparks(CCPoint where, ccColor4F color, int count, float reach, float time) {
    for (int i = 0; i < count; i++) {
        float size = randomIn(2.f, 4.f);
        auto spark = CCLayerColor::create({ toByte(color.r * 255), toByte(color.g * 255), toByte(color.b * 255), 255 }, size, size);
        spark->setBlendFunc({ GL_SRC_ALPHA, GL_ONE });
        spark->setPosition(where);
        float angle = randomIn(0.f, PI2), distance = randomIn(reach * .35f, reach);
        spark->runAction(CCSequence::create(
            CCSpawn::create(
                CCEaseOut::create(CCMoveBy::create(time, { std::cos(angle) * distance, std::sin(angle) * distance }), 2.f),
                CCFadeOut::create(time), nullptr
            ),
            CCRemoveSelf::create(), nullptr
        ));
        m_effects->addChild(spark);
    }
}

void RageFight::damageNumber(int amount, bool head, CCPoint where) {
    auto label = CCLabelBMFont::create(fmt::format("-{}", amount).c_str(), head ? "goldFont.fnt" : "bigFont.fnt");
    label->setScale(head ? .55f : .35f);
    label->setPosition(where + CCPoint(randomIn(-8.f, 8.f), 14.f));
    label->runAction(CCSequence::create(
        CCSpawn::create(CCEaseOut::create(CCMoveBy::create(.55f, { 0.f, 28.f }), 2.f), CCFadeOut::create(.55f), nullptr),
        CCRemoveSelf::create(), nullptr
    ));
    m_effects->addChild(label);
    if (head) {
        auto crit = CCLabelBMFont::create("HEADSHOT", "bigFont.fnt");
        crit->setScale(.25f);
        crit->setColor({ 255, 220, 90 });
        crit->setPosition(where + CCPoint(0.f, 34.f));
        crit->runAction(CCSequence::create(
            CCSpawn::create(CCMoveBy::create(.5f, { 0.f, 18.f }), CCFadeOut::create(.5f), nullptr), CCRemoveSelf::create(), nullptr
        ));
        m_effects->addChild(crit);
    }
}

// ---- Grin's moves ----

void RageFight::enrage() {
    m_enraged = true;
    playSound("roar.ogg"_spr, 1.08f);
    this->shakeScreen(9.f, .6f);
    m_hurtFlash->stopAllActions();
    m_hurtFlash->setOpacity(110);
    m_hurtFlash->runAction(CCFadeTo::create(.5f, 0));
    if (m_arena) m_arena->runAction(CCTintTo::create(.6f, 255, 150, 140));
    m_rageLabel->setVisible(true);
    m_rageLabel->setScale(.1f);
    m_rageLabel->runAction(CCEaseBackOut::create(CCScaleTo::create(.3f, .3f)));

    auto banner = popText("GRIN IS ENRAGED!", "goldFont.fnt", .85f, { 255, 90, 60 });
    banner->setPosition(m_win / 2 + CCPoint(0.f, m_win.height * .18f));
    banner->runAction(CCSequence::create(CCDelayTime::create(1.f), CCFadeOut::create(.3f), CCRemoveSelf::create(), nullptr));
    this->addChild(banner, 8);
    this->sparks(m_grin->getPosition(), ccc4f(1.f, .3f, .1f, 1.f), 30, 160.f, .6f);
    m_nextShard = 1.f;
}

void RageFight::startCharge() {
    m_charging = true;
    m_chargeTime = 0.f;
    m_chargeHits = 0;
    playSound("grin-snarl.ogg"_spr, randomIn(.95f, 1.05f));
    if (!m_explainedCharge) {
        m_explainedCharge = true;
        auto hint = popText("SHOOT IT TO STOP THE CHARGE!", "bigFont.fnt", .4f, { 255, 220, 120 });
        hint->setPosition({ m_win.width / 2, m_win.height * .14f });
        hint->runAction(CCSequence::create(CCDelayTime::create(1.f), CCFadeOut::create(.25f), CCRemoveSelf::create(), nullptr));
        this->addChild(hint, 8);
    }
}

void RageFight::chargeLanded() {
    m_charging = false;
    m_nextCharge = m_enraged ? randomIn(1.3f, 2.2f) : randomIn(1.9f, 3.1f);
    this->hurtPlayer({ randomIn(m_win.width * .25f, m_win.width * .75f), randomIn(m_win.height * .3f, m_win.height * .7f) });
}

void RageFight::stagger() {
    m_charging = false;
    m_nextCharge = m_enraged ? randomIn(1.3f, 2.2f) : randomIn(1.9f, 3.1f);
    playSound("hit.ogg"_spr, .75f);
    playSound("grin-hurt2.ogg"_spr);
    this->shakeScreen(5.f, .18f);
    auto label = popText("KNOCKED BACK!", "bigFont.fnt", .45f, { 255, 230, 120 });
    label->setPosition(m_grin->getPosition() + CCPoint(0.f, m_win.height * .25f));
    label->runAction(CCSequence::create(CCDelayTime::create(.45f), CCFadeOut::create(.2f), CCRemoveSelf::create(), nullptr));
    this->addChild(label, 8);
    this->sparks(m_grin->getPosition(), ccc4f(1.f, .9f, .4f, 1.f), 18, 120.f, .45f);
    this->damageGrin(STAGGER_DAMAGE, true, m_grin->getPosition());
}

void RageFight::dash() {
    // A burst to another spot, leaving red afterimages behind
    m_dashFrom = m_homeX;
    float left = m_win.width * .22f, right = m_win.width * .78f;
    m_dashTo = randomIn(left, right);
    if (std::abs(m_dashTo - m_dashFrom) < m_win.width * .2f) {
        m_dashTo = m_dashFrom < m_win.width / 2 ? randomIn(m_win.width * .6f, right) : randomIn(left, m_win.width * .4f);
    }
    m_dashTime = 0.f;
    m_afterimageClock = 0.f;
    m_nextDash = m_enraged ? randomIn(1.1f, 2.f) : randomIn(2.f, 3.4f);
    playSound("hit.ogg"_spr, .55f, .6f);
}

void RageFight::afterimage() {
    auto ghost = silhouetteOf(m_grin, { 255, 60, 35 }, true);
    if (!ghost) return;
    ghost->setPosition(m_grin->getPosition());
    ghost->setScale(m_grin->getScale());
    ghost->setOpacity(130);
    ghost->runAction(CCSequence::create(CCFadeOut::create(.28f), CCRemoveSelf::create(), nullptr));
    m_world->addChild(ghost, 0);
}

// ---- Thrown glass (enraged) ----

void RageFight::throwShard() {
    m_nextShard = randomIn(1.8f, 2.9f);

    auto glass = CCDrawNode::create();
    std::array<CCPoint, 5> outline {
        CCPoint(0.f, 15.f), CCPoint(7.f, 3.f), CCPoint(3.f, -13.f), CCPoint(-5.f, -9.f), CCPoint(-7.f, 4.f),
    };
    glass->drawDot({ 0.f, 0.f }, 18.f, ccc4f(1.f, .25f, .15f, .18f));
    glass->drawPolygon(outline.data(), outline.size(), ccc4f(.82f, .93f, 1.f, .85f), 1.f, ccc4f(1.f, 1.f, 1.f, 1.f));

    // Thrown from one of its hands
    float side = randomIn(0.f, 1.f) < .5f ? -1.f : 1.f;
    auto hand = m_grin->getPosition() + CCPoint(
        side * m_grin->getScaledContentSize().width * .32f, m_grin->getScaledContentSize().height * .12f
    );
    Shard shard;
    shard.node = glass;
    shard.from = hand;
    shard.to = CCPoint(randomIn(m_win.width * .18f, m_win.width * .82f), randomIn(m_win.height * .22f, m_win.height * .72f));
    shard.duration = SHARD_TIME;
    shard.spin = randomIn(-420.f, 420.f);
    glass->setPosition(hand);
    glass->setScale(.35f);
    m_shardLayer->addChild(glass);
    m_shards.push_back(shard);
    playSound("shot.ogg"_spr, .6f, .7f);
}

void RageFight::updateShards(float dt) {
    for (size_t i = 0; i < m_shards.size();) {
        auto& shard = m_shards[i];
        shard.time += dt;
        float t = std::min(1.f, shard.time / shard.duration);
        // Slow at first, then rushing at the glass
        float eased = t * t;
        shard.node->setPosition(shard.from + (shard.to - shard.from) * eased);
        shard.node->setScale(.35f + 2.3f * eased);
        shard.node->setRotation(shard.node->getRotation() + shard.spin * dt);
        if (t >= 1.f) {
            auto where = shard.to;
            shard.node->removeFromParent();
            m_shards.erase(m_shards.begin() + static_cast<std::ptrdiff_t>(i));
            playSound("shatter.ogg"_spr, 1.2f, .8f);
            this->sparks(where, ccc4f(.85f, .95f, 1.f, 1.f), 16, 90.f, .4f);
            this->hurtPlayer(where);
            if (m_state == State::Over) return;
            continue;
        }
        i++;
    }
}

bool RageFight::shootShard(CCPoint local) {
    // The closest (biggest) shard under the shot breaks
    for (size_t i = m_shards.size(); i-- > 0;) {
        auto& shard = m_shards[i];
        float reach = 16.f * shard.node->getScale() + 14.f;
        if (ccpDistance(local, shard.node->getPosition()) > reach) continue;
        auto where = shard.node->getPosition();
        shard.node->removeFromParent();
        m_shards.erase(m_shards.begin() + static_cast<std::ptrdiff_t>(i));
        playSound("crack.ogg"_spr, 1.5f, .9f);
        this->sparks(where, ccc4f(.85f, .95f, 1.f, 1.f), 12, 60.f, .3f);
        return true;
    }
    return false;
}

void RageFight::clearShards() {
    for (auto& shard : m_shards) {
        shard.node->runAction(CCSequence::create(CCFadeOut::create(.2f), CCRemoveSelf::create(), nullptr));
    }
    m_shards.clear();
}

// ---- Hurting you ----

void RageFight::hurtPlayer(CCPoint where) {
    if (m_state == State::Over) return;
    m_playerHearts = std::max(0, m_playerHearts - 1);
    playSound("crack.ogg"_spr, .9f);
    playSound("hit.ogg"_spr, .7f, .8f);
    this->shakeScreen(10.f, .35f);
    m_hurtFlash->stopAllActions();
    m_hurtFlash->setOpacity(140);
    m_hurtFlash->runAction(CCFadeTo::create(.45f, 0));
    this->crackScreen(where);
    this->refreshHud();

    // The heart you lost pops
    if (auto lost = m_hearts->getChildByTag(m_playerHearts)) {
        lost->runAction(CCSequence::create(CCScaleTo::create(.08f, 1.6f), CCScaleTo::create(.15f, 1.f), nullptr));
    }
    if (m_playerHearts <= 0) this->lose();
}

void RageFight::crackScreen(CCPoint center) {
    // Your screen cracks a little more each time
    m_screenCracks->drawDot(center, 5.f, ccc4f(1.f, 1.f, 1.f, .35f));
    for (int i = 0; i < 9; i++) {
        float angle = PI2 * i / 9 + randomIn(-.3f, .3f);
        CCPoint from = center;
        float length = randomIn(50.f, 130.f);
        for (int k = 0; k < 3; k++) {
            angle += randomIn(-.35f, .35f);
            CCPoint to = from + CCPoint(std::cos(angle), std::sin(angle)) * (length / 3);
            m_screenCracks->drawSegment(from + CCPoint(.7f, -.7f), to + CCPoint(.7f, -.7f), 1.f, ccc4f(0.f, 0.f, 0.f, .4f));
            m_screenCracks->drawSegment(from, to, .7f, ccc4f(1.f, 1.f, 1.f, .75f));
            from = to;
        }
    }
}

// ---- Endings ----

void RageFight::win() {
    if (m_state == State::Over) return;
    m_state = State::Over;
    m_won = true;
    m_charging = false;
    this->clearShards();
    playSound("grin-dies.ogg"_spr);
    playSound("shatter.ogg"_spr, .8f);
    playSound("roar.ogg"_spr, .7f, .6f);
    this->shakeScreen(12.f, .6f);

    // It burns white-hot and bursts into embers
    auto flash = CCLayerColor::create({ 255, 240, 220, 200 }, m_win.width, m_win.height);
    flash->runAction(CCSequence::create(CCFadeOut::create(.5f), CCRemoveSelf::create(), nullptr));
    this->addChild(flash, 5);
    auto center = m_grin->getPosition() + CCPoint(0.f, m_grin->getScaledContentSize().height * .15f);
    this->sparks(center, ccc4f(1.f, .8f, .2f, 1.f), 50, 200.f, .7f);
    this->sparks(center, ccc4f(1.f, .35f, .1f, 1.f), 40, 260.f, 1.f);
    if (m_grinFlash) {
        m_grinFlash->stopAllActions();
        m_grinFlash->setOpacity(255);
    }
    if (m_aura) m_aura->setOpacity(0);
    m_grin->stopAllActions();
    m_grin->runAction(CCSpawn::create(
        CCEaseIn::create(CCScaleTo::create(.6f, m_grin->getScale() * 1.3f), 2.f), CCFadeOut::create(.6f), nullptr
    ));
    this->setVignette(0.f);

    auto label = popText("YOU BEAT GRIN!", "goldFont.fnt", .9f, { 255, 255, 255 });
    label->setPosition(m_win / 2);
    this->addChild(label, 8);
    this->runAction(CCSequence::create(CCDelayTime::create(2.f), CallFuncExt::create([this] { this->done(); }), nullptr));
}

void RageFight::lose() {
    if (m_state == State::Over) return;
    m_state = State::Over;
    m_won = false;
    m_charging = false;
    this->clearShards();
    playSound("roar.ogg"_spr, .95f);
    this->shakeScreen(12.f, .5f);
    m_hurtFlash->stopAllActions();
    m_hurtFlash->runAction(CCFadeTo::create(.4f, 150));

    // Right in your face
    auto face = grin_look::sprite(grin_look::Pose::Roar, m_win.height * 1.15f);
    if (!face) face = RageEvent::createFace(m_win.height * 1.15f);
    float target = face->getScale();
    face->setScale(target * .3f);
    face->setPosition(m_win / 2);
    face->runAction(CCEaseIn::create(CCScaleTo::create(.45f, target), 2.f));
    this->addChild(face, 7);

    auto label = popText("GRIN WINS", "goldFont.fnt", .9f, { 255, 80, 60 });
    label->setPosition({ m_win.width / 2, m_win.height * .15f });
    this->addChild(label, 8);
    // "Game over", in its own voice, once the roar dies down, then a laugh
    this->runAction(CCSequence::create(
        CCDelayTime::create(.7f), CallFuncExt::create([] { playSound("grin-gameover.ogg"_spr); }),
        CCDelayTime::create(1.2f), CallFuncExt::create([] { playSound("grin-laugh.ogg"_spr, 1.f, .8f); }),
        CCDelayTime::create(.8f), CallFuncExt::create([this] { this->done(); }), nullptr
    ));
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
        m_moveTime += dt;
        m_timeLeft -= dt;
        m_timer->setString(std::to_string(std::max(0, static_cast<int>(std::ceil(m_timeLeft)))).c_str());
        if (m_timeLeft <= 0.f) {
            this->lose();
            return;
        }

        if (m_charging) {
            m_chargeTime += dt;
            if (m_chargeTime >= this->chargeDuration()) this->chargeLanded();
        }
        else if (m_dashTime < 0.f) {
            m_nextCharge -= dt;
            m_nextDash -= dt;
            if (m_nextCharge <= 0.f) this->startCharge();
            else if (m_nextDash <= 0.f) this->dash();
        }
        if (m_enraged) {
            m_nextShard -= dt;
            if (m_nextShard <= 0.f) this->throwShard();
        }
        this->updateShards(dt);
        if (m_state != State::Fight) return;
    }

    // The dash itself: fast out, easing into its new spot
    if (m_dashTime >= 0.f) {
        m_dashTime += dt;
        float t = std::min(1.f, m_dashTime / DASH_TIME);
        float eased = 1.f - (1.f - t) * (1.f - t) * (1.f - t);
        m_homeX = m_dashFrom + (m_dashTo - m_dashFrom) * eased;
        m_afterimageClock -= dt;
        if (m_afterimageClock <= 0.f) {
            this->afterimage();
            m_afterimageClock = .035f;
        }
        if (t >= 1.f) m_dashTime = -1.f;
    }

    // Charge size follows the charge, then eases back down instead of snapping
    float charge = m_charging ? std::min(1.f, m_chargeTime / this->chargeDuration()) : 0.f;
    m_chargeLook = charge >= m_chargeLook ? charge : std::max(charge, m_chargeLook - dt * 2.8f);

    if (m_state == State::Fight) {
        float c = m_chargeLook;
        float sway = std::sin(m_moveTime * (m_enraged ? 1.9f : 1.3f)) * m_win.width * .06f;
        CCPoint position {
            m_homeX + sway * (1.f - c),
            m_win.height * .45f + std::sin(m_moveTime * 2.3f) * m_win.height * .025f - c * m_win.height * .08f,
        };
        // Charging pulls it toward the middle of your screen
        position.x += (m_win.width / 2 - position.x) * c * .6f;
        m_grin->setPosition(position);
        float breathe = 1.f + .025f * std::sin(m_moveTime * 4.f);
        m_grin->setScale(m_baseScale * breathe * (1.f + 1.6f * c * c));
        // Smoothly redder as the charge builds; no flickering
        m_grin->setColor({ 255, toByte(255 - 130 * c), toByte(255 - 130 * c) });
        if (m_aura) {
            float glow = std::max(c, m_enraged ? .45f + .2f * std::sin(m_time * 5.f) : 0.f);
            m_aura->setOpacity(toByte(glow * 200));
        }
    }

    // Red edges warn of a charge, and beat like a heart on your last heart
    if (m_state != State::Over) {
        float edges = m_chargeLook * .85f;
        if (m_playerHearts == 1) edges = std::max(edges, .35f + .25f * std::sin(m_time * 6.f));
        else if (m_enraged) edges = std::max(edges, .15f);
        this->setVignette(edges);
    }

    // Embers rise faster once it's enraged
    m_emberClock -= dt;
    if (m_emberClock <= 0.f) {
        this->spawnEmber();
        m_emberClock = m_enraged ? .05f : .11f;
    }

    // Shake
    if (m_shakeTime > 0.f) {
        m_shakeTime -= dt;
        float strength = m_shakeStrength * std::clamp(m_shakeTime / .25f + .25f, 0.f, 1.f);
        m_world->setPosition({ randomIn(-strength, strength), randomIn(-strength, strength) });
    }
    else {
        m_world->setPosition({ 0.f, 0.f });
    }
}

void RageFight::spawnEmber() {
    float size = randomIn(1.5f, 3.5f);
    auto ember = CCLayerColor::create({ 255, toByte(randomIn(90.f, 190.f)), 30, 255 }, size, size);
    ember->setBlendFunc({ GL_SRC_ALPHA, GL_ONE });
    ember->setPosition({ randomIn(0.f, m_win.width), randomIn(-10.f, m_win.height * .35f) });
    ember->setOpacity(0);
    float time = randomIn(1.8f, 3.2f);
    ember->runAction(CCSequence::create(
        CCSpawn::create(
            CCMoveBy::create(time, { randomIn(-40.f, 40.f), m_win.height * randomIn(.35f, .7f) }),
            CCSequence::create(CCFadeTo::create(time * .2f, toByte(randomIn(140.f, 230.f))), CCFadeOut::create(time * .8f), nullptr),
            nullptr
        ),
        CCRemoveSelf::create(), nullptr
    ));
    m_embers->addChild(ember);
}

void RageFight::setVignette(float strength) {
    for (auto band : m_vignette) band->setOpacity(toByte(strength * 255));
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
    m_hpFill->setScaleX(std::max(.001f, m_grinHealth / MAX_HEALTH));
    if (m_enraged) m_hpFill->setColor({ 255, 90, 20 });
    m_hearts->removeAllChildren();
    for (int i = 0; i < PLAYER_HEARTS; i++) {
        auto h = this->heart(i < m_playerHearts);
        h->setPosition({ i * 24.f, 0.f });
        h->setTag(i);
        m_hearts->addChild(h);
    }
}
