#include "Rage.hpp"
#include "Profile.hpp"

#include <algorithm>
#include <map>

using namespace geode::prelude;

namespace {
    float s_anger = 0.f;
    // Full of rage and waiting for the moment you least expect it
    bool s_armed = false;
    float s_difficulty = 1.f;
    int s_best = 0;
    bool s_platformer = false;
    // Deaths per 3% band this session, for "the same spot again"
    std::map<int, int> s_spots;

    float temper() {
        auto value = Mod::get()->getSettingValue<std::string>("rage-temper");
        if (value == "Patient") return .6f;
        if (value == "Short Fuse") return 1.6f;
        return 1.f;
    }
}

bool rage::active() {
    return profile::current().character == "grin";
}

float rage::difficultyWeight(GJGameLevel* level) {
    if (!level) return 1.f;
    if (level->m_autoLevel) return .3f;
    if (level->m_demon.value() > 0) {
        switch (level->m_demonDifficulty) {
            case 3: return 1.3f;  // easy demon
            case 4: return 1.45f; // medium
            case 5: return 1.8f;  // insane
            case 6: return 2.f;   // extreme
            default: return 1.6f; // hard
        }
    }
    int stars = level->m_stars.value();
    if (stars > 0) {
        if (stars <= 1) return .3f;
        if (stars == 2) return .6f;
        if (stars == 3) return .75f;
        if (stars <= 5) return .9f;
        if (stars <= 7) return 1.f;
        if (stars <= 9) return 1.15f;
        return 1.6f;
    }
    // Unrated: go by the players' votes, and assume "medium" without any
    switch (level->getAverageDifficulty()) {
        case 1: return .6f;
        case 2: return .75f;
        case 3: return .9f;
        case 4: return 1.f;
        case 5: return 1.15f;
        default: return 1.f;
    }
}

void rage::startLevel(GJGameLevel* level, bool platformer) {
    s_anger = 0.f;
    s_armed = false;
    s_difficulty = difficultyWeight(level);
    s_best = level ? level->m_normalPercent.value() : 0;
    s_platformer = platformer;
    s_spots.clear();
}

void rage::endLevel() {
    s_anger = 0.f;
    s_armed = false;
    s_spots.clear();
}

rage::DeathResult rage::onDeath(int percent, int attempt, bool practice, bool testMode) {
    DeathResult result;
    percent = std::clamp(percent, 0, 100);

    // Progress calms it a little instead of feeding it
    if (!practice && !testMode && !s_platformer && percent > s_best) {
        s_best = percent;
        s_anger = std::max(0.f, s_anger - 15.f);
        // Progress talks it down from striking
        s_armed = false;
        result.newBest = true;
        return result;
    }

    // How much this death hurts: a 1% death barely counts, 90%+ is brutal
    float p = percent / 100.f;
    float gain = s_platformer ? 6.f : 2.f + 28.f * p * p;

    // Heartbreak: dying just short of the best
    if (!s_platformer && s_best >= 30 && percent >= s_best - 3) gain *= 1.3f;

    gain *= s_difficulty;

    // The first few tries at the very start are forgiven
    if (attempt <= 3 && percent < 10) gain *= .3f;

    // The same spot again and again wears its patience thin
    if (!s_platformer) {
        int repeats = ++s_spots[percent / 3];
        gain *= std::min(2.f, 1.f + .15f * (repeats - 1));
    }

    // Practice and start positions matter less
    if (practice) gain *= .25f;
    else if (testMode) gain *= .5f;

    gain *= temper();
    s_anger = std::min(MAX, s_anger + gain);
    result.gained = gain;
    if (s_anger >= MAX && Mod::get()->getSettingValue<bool>("rage-jumpscare")) {
        if (Mod::get()->getSettingValue<bool>("rage-sneak")) {
            // It doesn't strike now, while you expect it
            result.armed = !s_armed;
            s_armed = true;
        }
        else {
            result.snapped = true;
        }
    }
    return result;
}

void rage::onComplete() {
    s_anger = 0.f;
    s_armed = false;
    s_spots.clear();
}

void rage::coolDown(float seconds) {
    // Waiting to strike: it doesn't calm down while it waits
    if (s_armed) return;
    // About one point every ten seconds of staying alive
    s_anger = std::max(0.f, s_anger - seconds * .1f);
}

void rage::afterSnap(float anger) {
    s_armed = false;
    s_anger = std::clamp(anger, 0.f, MAX - 1.f);
    s_spots.clear();
}

bool rage::armed() {
    return s_armed;
}

void rage::disarm() {
    s_armed = false;
}

float rage::anger() {
    return s_anger;
}

float rage::level() {
    return std::clamp(s_anger / MAX, 0.f, 1.f);
}

char const* rage::mood() {
    if (s_armed) return "watching you";
    if (s_anger < 20.f) return "calm";
    if (s_anger < 40.f) return "irritated";
    if (s_anger < 60.f) return "angry";
    if (s_anger < 80.f) return "furious";
    return "about to snap";
}

std::string rage::eventNote() {
    if (!active()) return "";
    if (s_armed) {
        return " Your anger is at 100/100 and you've decided to strike at a moment they won't see coming."
               " Keep it secret: sound calm, almost friendly, with only a hint that something is coming.";
    }
    return fmt::format(" Your anger is now {}/100 ({}).", static_cast<int>(s_anger), mood());
}
