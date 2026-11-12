#include "Haunt.hpp"
#include "GrinLook.hpp"
#include "Rage.hpp"
#include "RageBar.hpp"
#include "RageEvent.hpp"

#include <fmt/ranges.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

using namespace geode::prelude;

namespace {
    // Under the speech bubble (1000) and the screen break (2000)
    constexpr int LAYER_Z = 900;
    constexpr float PI2 = 6.2832f;
    constexpr size_t MAX_CRACKS = 14;
    constexpr int MAX_COMMANDS = 4;

    std::mt19937& rng() {
        static std::mt19937 gen(std::random_device{}());
        return gen;
    }

    float randomIn(float a, float b) {
        return std::uniform_real_distribution<float>(a, b)(rng());
    }

    GLubyte toByte(float value) {
        return static_cast<GLubyte>(std::clamp(value, 0.f, 255.f));
    }

    void playSound(char const* file, float loudness = 1.f, float speed = 1.f) {
        float volume = static_cast<float>(Mod::get()->getSettingValue<int64_t>("rage-volume")) / 100.f * loudness;
        if (volume > 0.f) FMODAudioEngine::get()->playEffect(file, speed, 0.f, volume);
    }

    bool mayMeddle() {
        return Mod::get()->getSettingValue<bool>("grin-meddle");
    }

    std::vector<std::string> s_log;
    std::vector<std::string> s_plan;

    void note(std::string what) {
        if (s_log.size() < 12) s_log.push_back(std::move(what));
    }

    // Everything Grin does to the screen while you play
    class HauntLayer : public CCLayer {
    public:
        static HauntLayer* create() {
            auto ret = new HauntLayer();
            if (ret->init()) {
                ret->autorelease();
                return ret;
            }
            delete ret;
            return nullptr;
        }

        bool init() override {
            if (!CCLayer::init()) return false;
            m_win = CCDirector::get()->getWinSize();
            this->setID("haunt-layer"_spr);

            // Darkness creeping in from the edges
            float bandX = m_win.width * .38f, bandY = m_win.height * .42f;
            ccColor4B black { 0, 0, 0, 255 }, clear { 0, 0, 0, 0 };
            struct Band { CCSize size; CCPoint position; CCPoint direction; };
            for (auto const& band : {
                Band { { bandX, m_win.height }, { 0.f, 0.f }, { 1.f, 0.f } },
                Band { { bandX, m_win.height }, { m_win.width - bandX, 0.f }, { -1.f, 0.f } },
                Band { { m_win.width, bandY }, { 0.f, 0.f }, { 0.f, 1.f } },
                Band { { m_win.width, bandY }, { 0.f, m_win.height - bandY }, { 0.f, -1.f } },
            }) {
                auto gradient = CCLayerGradient::create(black, clear, band.direction);
                gradient->setContentSize(band.size);
                gradient->setPosition(band.position);
                gradient->setOpacity(0);
                this->addChild(gradient, 1);
                m_edges.push_back(gradient);
            }

            m_faces = CCNode::create();
            this->addChild(m_faces, 2);

            m_cracks = CCDrawNode::create();
            this->addChild(m_cracks, 3);

            m_red = CCLayerColor::create({ 200, 0, 0, 0 }, m_win.width, m_win.height);
            this->addChild(m_red, 4);

            m_black = CCLayerColor::create({ 0, 0, 0, 0 }, m_win.width, m_win.height);
            this->addChild(m_black, 5);

            this->scheduleUpdate();
            return true;
        }

        void onExit() override {
            this->restoreLevel();
            CCLayer::onExit();
        }

        // ---- Effects ----

        void knock() {
            // Two knocks from inside the glass, each with a small jolt
            for (int i = 0; i < 2; i++) {
                this->runAction(CCSequence::create(
                    CCDelayTime::create(.02f + .26f * i),
                    CallFuncExt::create([this] {
                        playSound("hit.ogg"_spr, 1.f, .55f);
                        m_shake = std::max(m_shake, .14f);
                        m_shakeStrength = 3.5f;
                    }),
                    nullptr
                ));
            }
        }

        void crack(CCPoint center) {
            if (m_crackCount >= MAX_CRACKS) return;
            m_crackCount++;
            playSound("crack.ogg"_spr, .8f, randomIn(.9f, 1.15f));
            m_cracks->drawDot(center, 4.f, ccc4f(1.f, 1.f, 1.f, .3f));
            int rays = static_cast<int>(randomIn(6.f, 10.f));
            for (int i = 0; i < rays; i++) {
                float angle = PI2 * i / rays + randomIn(-.3f, .3f);
                CCPoint from = center;
                float length = randomIn(40.f, 120.f);
                for (int k = 0; k < 3; k++) {
                    angle += randomIn(-.35f, .35f);
                    CCPoint to = from + CCPoint(std::cos(angle), std::sin(angle)) * (length / 3);
                    m_cracks->drawSegment(from + CCPoint(.7f, -.7f), to + CCPoint(.7f, -.7f), 1.f, ccc4f(0.f, 0.f, 0.f, .4f));
                    m_cracks->drawSegment(from, to, .6f, ccc4f(1.f, 1.f, 1.f, .7f));
                    from = to;
                }
            }
            m_shake = std::max(m_shake, .12f);
            m_shakeStrength = 3.f;
        }

        void crackSomewhere(bool nearEdge) {
            CCPoint center = nearEdge
                ? CCPoint(
                    randomIn(0.f, 1.f) < .5f ? randomIn(0.f, m_win.width * .2f) : randomIn(m_win.width * .8f, m_win.width),
                    randomIn(m_win.height * .1f, m_win.height * .9f)
                )
                : CCPoint(randomIn(m_win.width * .15f, m_win.width * .85f), randomIn(m_win.height * .2f, m_win.height * .8f));
            this->crack(center);
        }

        void flicker() {
            // The lights stutter: uneven, a few dips, never a strobe
            m_black->stopAllActions();
            m_black->runAction(CCSequence::create(
                CCFadeTo::create(.05f, 170), CCFadeTo::create(.09f, 30),
                CCFadeTo::create(.04f, 210), CCFadeTo::create(.16f, 60),
                CCDelayTime::create(.12f),
                CCFadeTo::create(.05f, 190), CCFadeTo::create(.3f, 0),
                nullptr
            ));
        }

        void blackout() {
            m_black->stopAllActions();
            m_black->runAction(CCSequence::create(
                CCFadeTo::create(.06f, 255), CCDelayTime::create(.75f), CCFadeTo::create(.25f, 0), nullptr
            ));
            playSound("grin-snigger.ogg"_spr, .7f);
        }

        void dark() {
            m_dark = 9.f;
        }

        void red() {
            m_red->stopAllActions();
            m_red->setOpacity(120);
            m_red->runAction(CCFadeTo::create(.5f, 0));
        }

        void shake() {
            m_shake = std::max(m_shake, .45f);
            m_shakeStrength = 7.f;
        }

        void peek() {
            // Its face leans in from one side, then slides back out
            auto face = grin_look::sprite(grin_look::Pose::Face, m_win.height * 1.25f);
            if (!face) return;
            float side = randomIn(0.f, 1.f) < .5f ? -1.f : 1.f;
            float reach = face->getScaledContentSize().width;
            CCPoint hidden { m_win.width / 2 + side * (m_win.width / 2 + reach * .5f), m_win.height * randomIn(.4f, .6f) };
            CCPoint leaning { m_win.width / 2 + side * (m_win.width / 2 - reach * .08f), hidden.y };
            face->setPosition(hidden);
            face->setRotation(side * 14.f);
            face->setOpacity(215);
            face->runAction(CCSequence::create(
                CCEaseOut::create(CCMoveTo::create(.45f, leaning), 3.f),
                CCDelayTime::create(1.1f),
                CCEaseIn::create(CCMoveTo::create(.3f, hidden), 2.f),
                CCRemoveSelf::create(), nullptr
            ));
            m_faces->addChild(face);
            playSound("grin-talk-calm.ogg"_spr, .8f);
        }

        void watch() {
            // Huge and faint behind everything, just long enough to be sure you saw it
            auto face = grin_look::sprite(grin_look::Pose::Face, m_win.height * 1.5f);
            if (!face) return;
            face->setPosition({ m_win.width / 2, m_win.height * .48f });
            face->setOpacity(0);
            face->runAction(CCSequence::create(
                CCFadeTo::create(.5f, 85), CCDelayTime::create(1.4f), CCFadeTo::create(.7f, 0), CCRemoveSelf::create(), nullptr
            ));
            float scale = face->getScale();
            face->runAction(CCScaleTo::create(2.6f, scale * 1.12f));
            m_faces->addChild(face);
        }

        void mirror() {
            m_mirror = 5.f;
            this->flicker();
        }

        void tilt() {
            m_tilt = 6.f;
            m_tiltSide = randomIn(0.f, 1.f) < .5f ? -1.f : 1.f;
        }

        void zoom() {
            m_zoom = 5.f;
        }

        void clearDamage() {
            m_cracks->clear();
            m_crackCount = 0;
            m_dark = 0.f;
        }

        void restoreLevel() {
            m_mirror = m_tilt = m_zoom = m_shake = 0.f;
            if (m_touchedLevel) this->applyToLevel(0.f, 1.f, false, { 0.f, 0.f });
            m_touchedLevel = false;
        }

        bool meddling() const {
            return m_mirror > 0.f || m_tilt > 0.f || m_zoom > 0.f;
        }

    protected:
        void applyToLevel(float rotation, float zoom, bool mirrored, CCPoint offset) {
            auto playLayer = PlayLayer::get();
            if (!playLayer) return;
            playLayer->setRotation(rotation);
            playLayer->setScaleX(mirrored ? -zoom : zoom);
            playLayer->setScaleY(zoom);
            playLayer->setPosition(offset);
        }

        // 0..1: eases in over the first half second and out over the last
        static float envelope(float left, float total) {
            float in = std::clamp((total - left) / .5f, 0.f, 1.f);
            float out = std::clamp(left / .5f, 0.f, 1.f);
            float e = std::min(in, out);
            return e * e * (3.f - 2.f * e);
        }

        void update(float dt) override {
            auto playLayer = PlayLayer::get();
            if (!playLayer) return;
            // Grin has the whole screen, or the game is paused: hands off
            bool hold = RageEvent::isActive() || playLayer->m_isPaused;
            this->setVisible(!RageEvent::isActive());
            if (RageEvent::isActive()) {
                this->restoreLevel();
                return;
            }
            if (!hold) {
                m_mirror = std::max(0.f, m_mirror - dt);
                m_tilt = std::max(0.f, m_tilt - dt);
                m_zoom = std::max(0.f, m_zoom - dt);
                m_dark = std::max(0.f, m_dark - dt);
                m_shake = std::max(0.f, m_shake - dt);
                m_time += dt;
            }

            // The darker the mood, the less of the level you can see
            float anger = rage::level();
            float gloom = std::clamp((anger - .45f) / .55f, 0.f, 1.f) * .8f;
            if (rage::armed()) gloom = std::max(gloom, .7f + .15f * std::sin(m_time * 3.f));
            gloom = std::max(gloom, envelope(m_dark, 9.f) * .95f);
            m_gloom += (gloom - m_gloom) * std::min(1.f, dt * 3.f);
            for (auto edge : m_edges) edge->setOpacity(toByte(m_gloom * 255));

            // Tricks on the level itself
            float tilt = envelope(m_tilt, 6.f), zoom = envelope(m_zoom, 5.f);
            bool active = this->meddling() || m_shake > 0.f;
            if (active || m_touchedLevel) {
                float rotation = m_tiltSide * 7.f * tilt + (tilt > 0.f ? std::sin(m_time * 1.7f) * 1.5f * tilt : 0.f);
                // Zoomed a little whenever it's tilted, so the corners stay covered
                float scale = 1.f + .28f * zoom + .2f * tilt;
                CCPoint offset { 0.f, 0.f };
                if (m_shake > 0.f) offset = CCPoint(randomIn(-m_shakeStrength, m_shakeStrength), randomIn(-m_shakeStrength, m_shakeStrength));
                this->applyToLevel(rotation, scale, m_mirror > 0.f, offset);
                m_touchedLevel = active;
            }
        }

        CCSize m_win;
        std::vector<CCLayerGradient*> m_edges;
        CCNode* m_faces = nullptr;
        CCDrawNode* m_cracks = nullptr;
        CCLayerColor* m_red = nullptr;
        CCLayerColor* m_black = nullptr;
        size_t m_crackCount = 0;
        float m_time = 0.f;
        float m_gloom = 0.f;
        float m_dark = 0.f;
        float m_mirror = 0.f;
        float m_tilt = 0.f;
        float m_tiltSide = 1.f;
        float m_zoom = 0.f;
        float m_shake = 0.f;
        float m_shakeStrength = 0.f;
        bool m_touchedLevel = false;
    };

    HauntLayer* layer() {
        auto playLayer = PlayLayer::get();
        if (!playLayer || !playLayer->getParent()) return nullptr;
        return static_cast<HauntLayer*>(playLayer->getParent()->getChildByID("haunt-layer"_spr));
    }

    void refreshBar() {
        auto playLayer = PlayLayer::get();
        if (!playLayer || !playLayer->m_uiLayer) return;
        if (auto bar = static_cast<RageBar*>(playLayer->m_uiLayer->getChildByID("rage-bar"_spr))) bar->refresh(true);
    }

    std::string lower(std::string_view text) {
        return utils::string::toLower(std::string(utils::string::trim(std::string(text))));
    }
}

void haunt::attach() {
    auto playLayer = PlayLayer::get();
    if (!playLayer || !playLayer->getParent() || layer()) return;
    if (auto node = HauntLayer::create()) playLayer->getParent()->addChild(node, LAYER_Z);
}

void haunt::detach() {
    if (auto node = layer()) node->removeFromParent();
    s_log.clear();
    s_plan.clear();
}

std::string haunt::powers() {
    std::string tricks = mayMeddle()
        ? "- Tricks on the level itself, which make it harder to play; save them for when you're angry: mirror (the "
          "whole screen flips), tilt, zoom, blackout (a second of total darkness).\n"
        : "";
    return fmt::format(
        "\n\nYour powers:\n"
        "You don't only talk: you control the player's game. To act, end your reply with one command line in exactly "
        "this form: [DO: command, command]. The player never sees that line, only what it does.\n"
        "- Scares over the level: knock (you knock on the glass from inside), crack (a crack in their screen), flicker "
        "(the lights stutter), dark (darkness closes in for a while), peek (your face leans in from the edge), watch "
        "(your huge face, faint, behind the level), whisper, growl, laugh (your voice), red (a red flash), shake.\n"
        "{}"
        "- anger N: set your own anger bar, from 0 to 100. The bar is yours and the player watches it: raise it when "
        "they annoy or bore you, lower it when they impress you. Their screen decays as it fills.\n"
        "- strike: you've had enough. Moments later, in the middle of their attempt, you take the screen, smash it "
        "from the inside and the boss fight begins.\n"
        "- plan: the order of your attacks in the boss fight, as words separated by spaces, from: charge dash shards "
        "swarm dark feint. Example: plan dash charge swarm dark feint charge.\n"
        "Use at most three commands in a reply, and not in every reply. Match them to your words (say \"Tap, tap.\" "
        "and knock). Calm: small things. Angry: tricks. Game events tell you what you did since your last message; "
        "you did it on purpose, so own it, and never act surprised by your own effects.",
        tricks
    );
}

bool haunt::effect(std::string_view name) {
    auto node = layer();
    if (!node || RageEvent::isActive()) return false;
    auto key = lower(name);

    if (key == "knock") node->knock();
    else if (key == "crack") node->crackSomewhere(false);
    else if (key == "flicker") node->flicker();
    else if (key == "dark") node->dark();
    else if (key == "peek") node->peek();
    else if (key == "watch") node->watch();
    else if (key == "whisper") playSound("grin-snigger.ogg"_spr);
    else if (key == "growl") playSound("grin-snarl.ogg"_spr);
    else if (key == "laugh") playSound("grin-laugh.ogg"_spr, .9f);
    else if (key == "red") node->red();
    else if (key == "shake") node->shake();
    else if (key == "mirror" || key == "tilt" || key == "zoom" || key == "blackout") {
        // One trick at a time, and only if the player allows them
        if (!mayMeddle() || node->meddling()) return false;
        if (key == "mirror") node->mirror();
        else if (key == "tilt") node->tilt();
        else if (key == "zoom") node->zoom();
        else node->blackout();
    }
    else return false;
    return true;
}

void haunt::run(std::string_view commands) {
    int done = 0;
    for (auto const& raw : utils::string::split(std::string(commands), ",")) {
        auto command = lower(raw);
        if (command.empty()) continue;
        auto space = command.find_first_of(" :");
        auto word = command.substr(0, space);
        auto rest = space == std::string::npos ? std::string() : lower(command.substr(space + 1));
        if (!rest.empty() && rest.front() == ':') rest = lower(rest.substr(1));

        if (word == "anger") {
            int value = utils::numFromString<int>(rest).unwrapOr(-1);
            if (value < 0) continue;
            rage::set(static_cast<float>(value));
            refreshBar();
            onAngerChanged();
        }
        else if (word == "strike") {
            rage::set(rage::MAX);
            refreshBar();
            note("decided to strike");
        }
        else if (word == "plan") {
            s_plan.clear();
            for (auto const& move : utils::string::split(rest, " ")) {
                auto name = lower(move);
                if (name == "charge" || name == "dash" || name == "shards" || name == "swarm" || name == "dark" || name == "feint") {
                    s_plan.push_back(name);
                }
                if (s_plan.size() >= 12) break;
            }
        }
        else if (done < MAX_COMMANDS && effect(word)) {
            done++;
        }
    }
}

void haunt::onDeath() {
    auto node = layer();
    if (!node) return;
    // The screen decays by itself as the bar fills, so the bar is something you can see
    float anger = rage::level();
    float roll = randomIn(0.f, 1.f);
    if (anger >= .75f) {
        if (roll < .45f) { node->crackSomewhere(true); note("crack (as your anger grew)"); }
        else if (roll < .7f) { node->flicker(); note("flicker (as your anger grew)"); }
        else if (roll < .85f) { node->watch(); note("watch (as your anger grew)"); }
    }
    else if (anger >= .5f) {
        if (roll < .4f) { node->crackSomewhere(true); note("crack (as your anger grew)"); }
        else if (roll < .55f) { node->knock(); note("knock (as your anger grew)"); }
    }
    else if (anger >= .25f) {
        if (roll < .3f) { node->knock(); note("knock (as your anger grew)"); }
    }
}

void haunt::onAngerChanged() {
    // Calm again: the damage fades
    if (auto node = layer(); node && rage::level() < .2f) node->clearDamage();
}

void haunt::reset() {
    if (auto node = layer()) {
        node->clearDamage();
        node->restoreLevel();
    }
}

std::string haunt::takeLog() {
    if (s_log.empty()) return "";
    auto text = fmt::format(" Since your last message you did: {}.", fmt::join(s_log, ", "));
    s_log.clear();
    return text;
}

std::vector<std::string> const& haunt::fightPlan() {
    return s_plan;
}
