#pragma once

#include <Geode/Geode.hpp>

#include "Avatar.hpp"
#include "Look.hpp"

#include <map>
#include <string>

// Everything about how the AI looks, sounds and behaves, built from the
// player's settings and then overridden by the current level's creator
// config (hidden text objects starting with "ai:"), unless the player
// turned that off. Cost-related settings (provider, key, daily limit,
// cooldown, chance) are deliberately not part of this: levels can't touch them.

enum class ReplyLength { VeryShort, Short, Medium };

struct BubbleStyle {
    BubbleAnchor anchor = BubbleAnchor::Top;
    float offsetX = 0.f;
    float offsetY = 0.f;
    float width = 360.f;
    float textScale = .7f;
    std::string font = "chatFont.fnt";
    cocos2d::ccColor4B background { 0, 0, 0, 170 };
    cocos2d::ccColor3B textColor { 255, 255, 255 };
    cocos2d::ccColor3B nameColor { 255, 255, 255 };
    float duration = 4.f;
    bool typewriter = true;
    AvatarLook avatar;
};

struct EventRules {
    bool start = true;
    bool death = true;
    bool closeCall = true;
    bool newBest = true;
    bool complete = true;
    // How close to the best (in %) a death counts as a close call
    int closeCallRange = 3;
    // Extra direction for the AI, appended to that event's report
    std::string onStart;
    std::string onDeath;
    std::string onNewBest;
    std::string onComplete;
};

struct Profile {
    std::string name;
    // Final character description: preset, custom text, or the level's
    std::string persona;
    ReplyLength length = ReplyLength::Short;
    EventRules events;
    BubbleStyle bubble;
    // Empty means no talking sound
    std::string soundFile;
    float soundVolume = .6f;
    bool chatCustomColor = false;
    cocos2d::ccColor3B chatColor { 40, 40, 90 };
    bool chatLarge = false;
    // True when the current level changed anything
    bool fromLevel = false;
};

namespace profile {
    // Settings + current level config, resolved fresh each call so settings
    // changed from the pause menu apply right away
    Profile current();

    // Forget the previous level's config (it's read lazily from the new one)
    void resetLevel();

    // Exposed for tests and the docs: "ai: name = Rocky | sound = chirp"
    std::map<std::string, std::string> parseConfigText(std::string_view text);

    // Max characters kept from a reply of this length
    size_t maxReplyChars(ReplyLength length);
}
