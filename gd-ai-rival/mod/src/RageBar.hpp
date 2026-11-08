#pragma once

#include <Geode/Geode.hpp>

// Grin's anger as a bar in the top-left corner of the level. Fills from
// yellow to red, trembles when Grin is close to snapping, and lets the anger
// cool down slowly while the player stays alive.
class RageBar : public cocos2d::CCNode {
public:
    static RageBar* create();
    // Show the current anger; bump plays a little jolt when it just went up
    void refresh(bool bump = false);

protected:
    bool init() override;
    void update(float dt) override;

    cocos2d::CCLayerColor* m_fill = nullptr;
    cocos2d::CCLabelBMFont* m_mood = nullptr;
    cocos2d::CCSprite* m_face = nullptr;
    cocos2d::CCNode* m_body = nullptr;
    float m_shown = 0.f;
    float m_tremble = 0.f;
};
