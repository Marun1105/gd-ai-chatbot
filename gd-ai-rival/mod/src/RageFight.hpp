#pragma once

#include <Geode/Geode.hpp>

#include <functional>

// The short fight after Grin breaks your screen. You're inside the broken
// screen with it: tap Grin to shoot it (the head hurts more). Every few
// seconds it charges at you; land enough hits during the charge to knock it
// back, or it reaches you and you lose a heart. Empty its health to win, lose
// all three hearts (or run out of time) and it wins.
class RageFight : public cocos2d::CCNode {
public:
    struct Result {
        bool won = false;
        int hits = 0;
    };

    static RageFight* create(std::function<void(Result)> onDone);
    // A tap anywhere on screen, in world coordinates
    void shoot(cocos2d::CCPoint location);

protected:
    enum class State { Intro, Fight, Over };

    bool init(std::function<void(Result)> onDone);
    void update(float dt) override;

    void begin();
    void startCharge();
    void stagger();
    void hurtPlayer();
    void win();
    void lose();
    void done();

    void damageGrin(float amount, cocos2d::CCPoint where);
    void sparks(cocos2d::CCPoint where, cocos2d::ccColor4F color, int count);
    void refreshHud();
    cocos2d::CCNode* heart(bool full);
    void shakeScreen(float strength, float time);

    std::function<void(Result)> m_onDone;
    State m_state = State::Intro;
    cocos2d::CCSize m_win;

    cocos2d::CCNode* m_world = nullptr;
    cocos2d::CCSprite* m_grin = nullptr;
    cocos2d::CCSprite* m_grinFlash = nullptr;
    cocos2d::CCNode* m_effects = nullptr;
    cocos2d::CCLayerColor* m_hurtFlash = nullptr;
    cocos2d::CCDrawNode* m_screenCracks = nullptr;
    cocos2d::CCLayerColor* m_hpFill = nullptr;
    cocos2d::CCNode* m_hearts = nullptr;
    cocos2d::CCLabelBMFont* m_timer = nullptr;

    float m_grinHealth = 100.f;
    int m_playerHearts = 3;
    int m_hits = 0;
    bool m_won = false;
    float m_time = 0.f;
    float m_timeLeft = 25.f;
    float m_baseScale = 1.f;
    float m_nextCharge = 2.6f;
    bool m_charging = false;
    float m_chargeTime = 0.f;
    int m_chargeHits = 0;
    float m_tint = 0.f;
    float m_lastGrunt = -10.f;
    float m_shakeTime = 0.f;
    float m_shakeStrength = 0.f;
};
