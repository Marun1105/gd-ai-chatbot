#include "RageEvent.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

using namespace geode::prelude;

namespace {
    RageEvent* s_current = nullptr;

    // Above the speech bubble (1000) and everything else in the level scene
    constexpr int OVERLAY_Z = 2000;
    // Edge padding so shaking never shows the level around the dark overlay
    constexpr float PAD = 60.f;
    constexpr float FACE_BASE = 340.f;
    constexpr float PI = 3.14159265f;

    // Timings, in seconds from the moment of death
    constexpr float ROAR_AT = .12f;
    constexpr float LUNGE_AFTER = 1.f;
    constexpr float IMPACT_AFTER = .15f;
    constexpr float SHATTER_AFTER = .7f;
    constexpr float FADE_AFTER = 1.25f;
    constexpr float FADE_TIME = .5f;
    constexpr float CRACK_TIME = .35f;

    std::mt19937& rng() {
        static std::mt19937 gen(std::random_device{}());
        return gen;
    }

    float randomIn(float a, float b) {
        return std::uniform_real_distribution<float>(a, b)(rng());
    }

    void playSound(std::string const& file) {
        float volume = static_cast<float>(Mod::get()->getSettingValue<int64_t>("rage-volume")) / 100.f;
        if (volume > 0.f) FMODAudioEngine::get()->playEffect(file, 1.f, 0.f, volume);
    }

    std::vector<CCPoint> ellipse(CCPoint center, float rx, float ry, float angle, int segments) {
        std::vector<CCPoint> points;
        float c = std::cos(angle), s = std::sin(angle);
        for (int i = 0; i < segments; i++) {
            float t = 2 * PI * i / segments;
            float x = std::cos(t) * rx, y = std::sin(t) * ry;
            points.push_back(center + CCPoint(x * c - y * s, x * s + y * c));
        }
        return points;
    }

    void fillPolygon(CCDrawNode* node, std::vector<CCPoint> const& points, ccColor4F color) {
        node->drawPolygon(const_cast<CCPoint*>(points.data()), static_cast<unsigned>(points.size()), color, 0.f, color);
    }
}

// ---- The creature ----

CCSprite* RageEvent::createFace(float size) {
    auto draw = CCDrawNode::create();
    CCPoint c { FACE_BASE / 2, FACE_BASE / 2 };

    auto yellow = ccc4f(.96f, .80f, .16f, 1.f);
    auto rim = ccc4f(.45f, .30f, .04f, 1.f);
    auto black = ccc4f(.02f, .01f, .01f, 1.f);
    auto red = ccc4f(1.f, .12f, .08f, 1.f);
    auto tooth = ccc4f(.96f, .93f, .84f, 1.f);

    // Head, with a dark rim and a few grimy stains
    auto head = ellipse(c, 150.f, 150.f, 0.f, 72);
    draw->drawPolygon(head.data(), static_cast<unsigned>(head.size()), yellow, 5.f, rim);
    for (auto [x, y, r] : std::initializer_list<std::tuple<float, float, float>> {
        { -95.f, -60.f, 26.f }, { 88.f, -82.f, 18.f }, { 110.f, 40.f, 14.f }, { -40.f, 118.f, 12.f },
    }) {
        draw->drawDot(c + CCPoint(x, y), r, ccc4f(.55f, .40f, .05f, .22f));
    }

    // Hollow eyes tilted into a glare, with heavy brows
    for (float side : { -1.f, 1.f }) {
        auto eye = ellipse(c + CCPoint(side * 56.f, 36.f), 31.f, 45.f, side * -.22f, 40);
        fillPolygon(draw, eye, black);
        std::vector<CCPoint> brow {
            c + CCPoint(side * 102.f, 104.f), c + CCPoint(side * 24.f, 74.f),
            c + CCPoint(side * 22.f, 90.f), c + CCPoint(side * 98.f, 122.f),
        };
        fillPolygon(draw, brow, black);
        // Tiny burning pupils
        draw->drawDot(c + CCPoint(side * 50.f, 22.f), 17.f, ccc4f(1.f, .1f, .05f, .28f));
        draw->drawDot(c + CCPoint(side * 50.f, 22.f), 7.f, red);
    }

    // A grin far too wide: drawn as thin strips so the curve fills correctly
    constexpr int STRIPS = 24;
    auto upper = [&](float t) { return c.y - 30.f + 36.f * t * t; };
    auto lower = [&](float t) { return c.y - 30.f - 72.f * (1.f - t * t) + 36.f * t * t; };
    auto xAt = [&](float t) { return c.x + 120.f * t; };
    for (int i = 0; i < STRIPS; i++) {
        float a = -1.f + 2.f * i / STRIPS;
        float b = -1.f + 2.f * (i + 1) / STRIPS;
        fillPolygon(draw, {
            { xAt(a), upper(a) }, { xAt(b), upper(b) }, { xAt(b), lower(b) }, { xAt(a), lower(a) },
        }, black);
    }
    // Sharp teeth along both lips
    for (int i = 1; i + 2 < STRIPS; i += 2) {
        float a = -1.f + 2.f * i / STRIPS;
        float b = -1.f + 2.f * (i + 2) / STRIPS;
        float m = (a + b) / 2;
        float length = 18.f * (1.f - std::abs(m) * .55f);
        fillPolygon(draw, { { xAt(a), upper(a) }, { xAt(b), upper(b) }, { xAt(m), upper(m) - length } }, tooth);
        if (std::abs(m) < .8f) {
            fillPolygon(draw, { { xAt(a), lower(a) }, { xAt(b), lower(b) }, { xAt(m), lower(m) + length * .8f } }, tooth);
        }
    }

    // Render once into a texture so the face can fade and scale like a sprite
    auto texture = CCRenderTexture::create(static_cast<int>(FACE_BASE), static_cast<int>(FACE_BASE));
    texture->beginWithClear(0.f, 0.f, 0.f, 0.f);
    draw->visit();
    texture->end();
    auto sprite = CCSprite::createWithTexture(texture->getSprite()->getTexture());
    sprite->setFlipY(true);
    sprite->setScale(size / FACE_BASE);
    return sprite;
}

// ---- Setup ----

void RageEvent::play(std::function<void()> onDone) {
    auto playLayer = PlayLayer::get();
    if (s_current || !playLayer || !playLayer->getParent()) {
        if (onDone) onDone();
        return;
    }
    auto event = new RageEvent();
    if (event->init(std::move(onDone))) {
        event->autorelease();
        s_current = event;
        playLayer->getParent()->addChild(event, OVERLAY_Z);
    }
    else {
        delete event;
    }
}

bool RageEvent::isActive() {
    return s_current != nullptr;
}

bool RageEvent::init(std::function<void()> onDone) {
    if (!CCLayer::init()) return false;
    m_onDone = std::move(onDone);
    m_win = CCDirector::get()->getWinSize();
    this->setID("rage-event"_spr);

    // The screen as it was the moment you died, before anything goes dark
    m_snapshot = CCRenderTexture::create(static_cast<int>(m_win.width), static_cast<int>(m_win.height));
    m_snapshot->retain();
    m_snapshot->beginWithClear(0.f, 0.f, 0.f, 1.f);
    PlayLayer::get()->getParent()->visit();
    m_snapshot->end();

    m_root = CCNode::create();
    this->addChild(m_root);

    // Everything goes dark at once
    m_black = CCLayerColor::create({ 0, 0, 0, 255 }, m_win.width + PAD * 2, m_win.height + PAD * 2);
    m_black->setPosition({ -PAD, -PAD });
    m_root->addChild(m_black, 0);

    m_screen = CCSprite::createWithTexture(m_snapshot->getSprite()->getTexture());
    m_screen->setFlipY(true);
    m_screen->setAnchorPoint({ 0.f, 0.f });
    m_screen->setVisible(false);
    m_root->addChild(m_screen, 1);

    m_cracks = CCDrawNode::create();
    m_root->addChild(m_cracks, 2);

    m_shardLayer = CCNode::create();
    m_root->addChild(m_shardLayer, 3);

    m_face = createFace(m_win.height * .9f);
    m_face->setPosition(m_win / 2);
    m_face->setVisible(false);
    m_root->addChild(m_face, 4);

    m_flash = CCLayerColor::create({ 255, 255, 255, 0 }, m_win.width + PAD * 2, m_win.height + PAD * 2);
    m_flash->setPosition({ -PAD, -PAD });
    m_root->addChild(m_flash, 5);

    this->planCracks();
    // Cut the glass into pieces a few at a time while it roars
    this->schedule(schedule_selector(RageEvent::buildShards));
    this->schedule(schedule_selector(RageEvent::shake));

    this->setTouchEnabled(true);
    this->setKeypadEnabled(true);

    this->runAction(CCSequence::create(
        CCDelayTime::create(ROAR_AT), CallFuncExt::create([this] { this->roar(); }),
        CCDelayTime::create(LUNGE_AFTER), CallFuncExt::create([this] { this->lunge(); }),
        CCDelayTime::create(IMPACT_AFTER), CallFuncExt::create([this] { this->impact(); }),
        CCDelayTime::create(SHATTER_AFTER), CallFuncExt::create([this] { this->shatter(); }),
        CCDelayTime::create(FADE_AFTER), CallFuncExt::create([this] { this->fadeAway(); }),
        CCDelayTime::create(FADE_TIME + .05f), CallFuncExt::create([this] { this->finish(); }),
        nullptr
    ));
    return true;
}

void RageEvent::registerWithTouchDispatcher() {
    // Swallow every touch while it plays
    CCTouchDispatcher::get()->addTargetedDelegate(this, -10000, true);
}

void RageEvent::onExit() {
    // Leaving the level mid-scare: just let go, no reset callback
    if (s_current == this) s_current = nullptr;
    if (m_snapshot) {
        m_snapshot->release();
        m_snapshot = nullptr;
    }
    CCLayer::onExit();
}

// ---- Crack pattern ----

void RageEvent::planCracks() {
    m_impact = ccp(m_win.width / 2 + randomIn(-70.f, 70.f), m_win.height / 2 + randomIn(-35.f, 35.f));

    // Rays out of the impact; each crosses a ring of points at growing radii,
    // the last ring well outside the screen
    constexpr int RAYS = 11;
    const std::array<float, 6> RADII { 40.f, 100.f, 185.f, 300.f, 470.f, 1400.f };
    m_rays.clear();
    for (int i = 0; i < RAYS; i++) {
        float angle = 2 * PI * i / RAYS + randomIn(-.2f, .2f);
        std::vector<CCPoint> ray;
        for (float radius : RADII) {
            float a = angle + randomIn(-.06f, .06f);
            float r = radius * randomIn(.88f, 1.12f);
            ray.push_back(m_impact + CCPoint(std::cos(a), std::sin(a)) * r);
        }
        m_rays.push_back(ray);
    }

    // Pieces between neighbouring rays and rings
    m_pieces.clear();
    for (int i = 0; i < RAYS; i++) {
        auto const& a = m_rays[i];
        auto const& b = m_rays[(i + 1) % RAYS];
        m_pieces.push_back({ m_impact, a[0], b[0] });
        for (size_t k = 1; k < RADII.size(); k++) {
            m_pieces.push_back({ a[k - 1], a[k], b[k], b[k - 1] });
        }
    }
}

CCSprite* RageEvent::cutShard(std::vector<CCPoint> const& polygon) {
    float minX = m_win.width, minY = m_win.height, maxX = 0.f, maxY = 0.f;
    for (auto const& p : polygon) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }
    minX = std::max(0.f, std::floor(minX));
    minY = std::max(0.f, std::floor(minY));
    maxX = std::min(m_win.width, std::ceil(maxX));
    maxY = std::min(m_win.height, std::ceil(maxY));
    int width = static_cast<int>(maxX - minX);
    int height = static_cast<int>(maxY - minY);
    if (width < 2 || height < 2) return nullptr;

    CCPoint origin { minX, minY };
    std::vector<CCPoint> local;
    for (auto const& p : polygon) local.push_back(p - origin);

    // Paint the piece's shape as an opaque mask, then draw the snapshot
    // through it (keep the snapshot only where the mask is), so the texture
    // holds just this piece of the screen
    auto texture = CCRenderTexture::create(width, height);
    texture->beginWithClear(0.f, 0.f, 0.f, 0.f);

    auto mask = CCDrawNode::create();
    mask->setBlendFunc({ GL_ONE, GL_ZERO });
    for (size_t i = 1; i + 1 < local.size(); i++) {
        fillPolygon(mask, { local[0], local[i], local[i + 1] }, ccc4f(1.f, 1.f, 1.f, 1.f));
    }
    mask->visit();

    auto picture = CCSprite::createWithTexture(m_snapshot->getSprite()->getTexture());
    picture->setFlipY(true);
    picture->setAnchorPoint({ 0.f, 0.f });
    picture->setPosition(-origin);
    picture->setBlendFunc({ GL_DST_ALPHA, GL_ZERO });
    picture->visit();

    // A faint bright edge, like the side of broken glass
    auto edges = CCDrawNode::create();
    for (size_t i = 0; i < local.size(); i++) {
        edges->drawSegment(local[i], local[(i + 1) % local.size()], .55f, ccc4f(1.f, 1.f, 1.f, .32f));
    }
    edges->visit();
    texture->end();

    auto shard = CCSprite::createWithTexture(texture->getSprite()->getTexture());
    shard->setFlipY(true);
    shard->setPosition(origin + CCPoint(width / 2.f, height / 2.f));
    return shard;
}

void RageEvent::buildShards(float) {
    // A handful per frame keeps the roar smooth even on slow phones
    for (int i = 0; i < 8 && m_nextPiece < m_pieces.size(); i++, m_nextPiece++) {
        auto sprite = this->cutShard(m_pieces[m_nextPiece]);
        if (!sprite) continue;
        sprite->setVisible(false);
        m_shardLayer->addChild(sprite);

        auto center = sprite->getPosition();
        auto away = center - m_impact;
        float distance = std::max(1.f, ccpLength(away));
        auto direction = away / distance;
        Shard shard;
        shard.sprite = sprite;
        // Pieces near the hit fly out harder and go first
        float push = randomIn(40.f, 120.f) + 9000.f / (distance + 40.f);
        shard.velocity = direction * push + CCPoint(randomIn(-30.f, 30.f), randomIn(40.f, 160.f));
        shard.spin = randomIn(-220.f, 220.f) * (1.f + 60.f / (distance + 40.f));
        shard.delay = std::min(.25f, distance / 2200.f) + randomIn(0.f, .05f);
        m_shards.push_back(shard);
    }
    if (m_nextPiece >= m_pieces.size()) {
        this->unschedule(schedule_selector(RageEvent::buildShards));
    }
}

// ---- The sequence ----

void RageEvent::roar() {
    playSound("roar.wav"_spr);
    m_face->setVisible(true);
    float target = m_face->getScale();
    m_face->setScale(target * .45f);
    m_face->setOpacity(0);
    m_face->runAction(CCFadeIn::create(.25f));
    m_face->runAction(CCEaseBackOut::create(CCScaleTo::create(.4f, target)));
    // Breathing while it roars
    m_face->runAction(CCSequence::create(
        CCDelayTime::create(.4f),
        CCRepeat::create(CCSequence::create(
            CCScaleTo::create(.09f, target * 1.04f), CCScaleTo::create(.09f, target), nullptr
        ), 3),
        nullptr
    ));
    m_shakeTime = LUNGE_AFTER;
    m_shakeStrength = 5.f;
}

void RageEvent::lunge() {
    // It throws itself at the glass
    float target = m_face->getScale();
    m_face->stopAllActions();
    m_face->runAction(CCEaseIn::create(CCScaleTo::create(IMPACT_AFTER, target * 3.2f), 2.f));
    m_face->runAction(CCMoveTo::create(IMPACT_AFTER, m_impact));
}

void RageEvent::impact() {
    m_face->setVisible(false);
    // The dark stays underneath, so the jolt never shows the live level at the edges
    m_screen->setVisible(true);

    // One soft flash (not a strobe) and a hard jolt
    m_flash->setOpacity(110);
    m_flash->runAction(CCFadeTo::create(.18f, 0));
    m_shakeTime = .3f;
    m_shakeStrength = 11.f;
    playSound("crack.wav"_spr);

    m_crackTime = 0.f;
    this->schedule(schedule_selector(RageEvent::growCracks));
}

void RageEvent::growCracks(float dt) {
    m_crackTime += dt;
    float progress = std::min(1.f, m_crackTime / CRACK_TIME);
    // Fast at first, then slowing down as the cracks reach the edges
    float reach = (1.f - (1.f - progress) * (1.f - progress)) * 900.f;

    m_cracks->clear();
    auto shadow = ccc4f(0.f, 0.f, 0.f, .45f);
    auto line = ccc4f(1.f, 1.f, 1.f, .9f);

    // Crushed glass right where it hit
    m_cracks->drawDot(m_impact, 9.f * progress + 2.f, ccc4f(1.f, 1.f, 1.f, .35f));

    for (auto const& ray : m_rays) {
        CCPoint from = m_impact;
        float travelled = 0.f;
        for (auto const& to : ray) {
            float length = ccpDistance(from, to);
            if (travelled >= reach) break;
            CCPoint end = to;
            if (travelled + length > reach) {
                end = from + (to - from) * ((reach - travelled) / length);
            }
            m_cracks->drawSegment(from + CCPoint(.8f, -.8f), end + CCPoint(.8f, -.8f), 1.1f, shadow);
            m_cracks->drawSegment(from, end, .7f, line);
            travelled += length;
            from = to;
        }
    }
    // Rings of cracks between the rays, once the rays have passed them
    for (size_t i = 0; i < m_rays.size(); i++) {
        auto const& a = m_rays[i];
        auto const& b = m_rays[(i + 1) % m_rays.size()];
        for (size_t k = 0; k + 1 < a.size(); k++) {
            if (ccpDistance(m_impact, a[k]) > reach * .85f) break;
            m_cracks->drawSegment(a[k], b[k], .5f, ccc4f(1.f, 1.f, 1.f, .55f));
        }
    }
    if (progress >= 1.f) {
        this->unschedule(schedule_selector(RageEvent::growCracks));
    }
}

void RageEvent::shatter() {
    // Finish any pieces a slow device hasn't cut yet
    while (m_nextPiece < m_pieces.size()) this->buildShards(0.f);

    m_screen->setVisible(false);
    m_cracks->setVisible(false);
    for (auto& shard : m_shards) shard.sprite->setVisible(true);
    playSound("shatter.wav"_spr);
    m_shakeTime = .2f;
    m_shakeStrength = 6.f;

    // It's waiting behind the glass
    float scale = m_win.height * .75f / FACE_BASE;
    m_face->stopAllActions();
    m_face->setPosition(m_win / 2);
    m_face->setScale(scale);
    m_face->setOpacity(0);
    m_face->setVisible(true);
    m_face->runAction(CCSequence::create(CCDelayTime::create(.25f), CCFadeTo::create(.5f, 150), nullptr));

    m_fallTime = 0.f;
    this->schedule(schedule_selector(RageEvent::fall));
}

void RageEvent::fall(float dt) {
    m_fallTime += dt;
    constexpr float GRAVITY = 1500.f;
    for (auto& shard : m_shards) {
        if (!shard.sprite->isVisible() || m_fallTime < shard.delay) continue;
        shard.velocity.y -= GRAVITY * dt;
        shard.sprite->setPosition(shard.sprite->getPosition() + shard.velocity * dt);
        shard.sprite->setRotation(shard.sprite->getRotation() + shard.spin * dt);
        if (shard.sprite->getPositionY() < -m_win.height * .6f) shard.sprite->setVisible(false);
    }
}

void RageEvent::shake(float dt) {
    if (m_shakeTime <= 0.f) {
        m_root->setPosition({ 0.f, 0.f });
        return;
    }
    m_shakeTime -= dt;
    float strength = m_shakeStrength * std::clamp(m_shakeTime / .3f + .3f, 0.f, 1.f);
    m_root->setPosition({ randomIn(-strength, strength), randomIn(-strength, strength) });
}

void RageEvent::fadeAway() {
    // Back to the level
    m_black->runAction(CCFadeTo::create(FADE_TIME, 0));
    m_face->runAction(CCFadeOut::create(FADE_TIME * .6f));
    for (auto& shard : m_shards) shard.sprite->runAction(CCFadeOut::create(FADE_TIME));
}

void RageEvent::finish() {
    if (m_finished) return;
    m_finished = true;
    auto onDone = std::move(m_onDone);
    if (s_current == this) s_current = nullptr;
    // This runs inside one of our own actions: keep the object alive until
    // the end of the frame instead of deleting it mid-call
    this->retain();
    this->removeFromParent();
    this->autorelease();
    if (onDone) onDone();
}
