#pragma once

#include <Geode/Geode.hpp>

// What Grin looks like: the bundled 3D renders, or a picture the player
// picked in the settings ("Your Own Picture"), used everywhere Grin shows up.
namespace grin_look {
    enum class Pose {
        Face,   // the scare, mouth closed
        Roar,   // the scare, jaw open
        Icon,   // small head for the anger bar and chat bubble
        Body,   // full body for the fight
    };

    // Null only when a bundled file is missing
    cocos2d::CCTexture2D* texture(Pose pose);

    // A sprite of that pose whose longer side is `size` points
    cocos2d::CCSprite* sprite(Pose pose, float size);

    // True while the player's own picture is in use
    bool usingOwnPicture();
}
