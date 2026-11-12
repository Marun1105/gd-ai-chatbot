#pragma once

#include <Geode/Geode.hpp>

#include <functional>
#include <vector>

// The boss fight after Grin breaks your screen. You're inside the broken
// screen with it: tap Grin to shoot it (the head hurts more). It dashes from
// side to side, and every few seconds it charges at you: land enough hits
// during the charge to knock it back, or it reaches you and you lose a heart.
// At half health it's enraged: faster, glowing, and throwing shards of your
// broken screen that you have to shoot down. Empty its health to win; lose
// all your hearts (or run out of time) and it wins.
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

    // A piece of glass flying at you
    struct Shard {
        cocos2d::CCNode* node = nullptr;
        cocos2d::CCPoint from;
        cocos2d::CCPoint to;
        float time = 0.f;
        float duration = 1.f;
        float spin = 0.f;
    };

    bool init(std::function<void(Result)> onDone);
    void update(float dt) override;

    void begin();
    void enrage();
    void startCharge();
    void chargeLanded();
    void stagger();
    void dash();
    void throwShard();
    void darken();
    // The next attack from the AI's plan
    void planMove();
    void updateShards(float dt);
    bool shootShard(cocos2d::CCPoint local);
    void hurtPlayer(cocos2d::CCPoint where);
    void win();
    void lose();
    void clearShards();
    void done();

    void damageGrin(float amount, bool head, cocos2d::CCPoint where);
    void sparks(cocos2d::CCPoint where, cocos2d::ccColor4F color, int count, float reach = 75.f, float time = .35f);
    void damageNumber(int amount, bool head, cocos2d::CCPoint where);
    void spawnEmber();
    void afterimage();
    void crackScreen(cocos2d::CCPoint center);
    void setVignette(float strength);
    void refreshHud();
    cocos2d::CCNode* heart(bool full);
    void shakeScreen(float strength, float time);

    float chargeDuration() const;
    int hitsToStagger() const;

    std::function<void(Result)> m_onDone;
    State m_state = State::Intro;
    cocos2d::CCSize m_win;

    cocos2d::CCNode* m_world = nullptr;
    cocos2d::CCSprite* m_arena = nullptr;
    cocos2d::CCNode* m_embers = nullptr;
    cocos2d::CCSprite* m_grin = nullptr;
    cocos2d::CCSprite* m_grinFlash = nullptr;
    cocos2d::CCSprite* m_aura = nullptr;
    cocos2d::CCNode* m_shardLayer = nullptr;
    cocos2d::CCNode* m_effects = nullptr;
    cocos2d::CCDrawNode* m_screenCracks = nullptr;
    cocos2d::CCLayerColor* m_hurtFlash = nullptr;
    std::vector<cocos2d::CCLayerGradient*> m_vignette;
    cocos2d::CCLayerColor* m_hpFill = nullptr;
    cocos2d::CCLabelBMFont* m_rageLabel = nullptr;
    cocos2d::CCNode* m_hearts = nullptr;
    cocos2d::CCLabelBMFont* m_timer = nullptr;

    float m_grinHealth = 0.f;
    int m_playerHearts = 0;
    int m_hits = 0;
    bool m_won = false;
    bool m_enraged = false;
    bool m_explainedCharge = false;
    float m_time = 0.f;
    // Starts at zero when the fight begins, so its movement picks up exactly
    // where it rose into view
    float m_moveTime = 0.f;
    float m_timeLeft = 0.f;
    float m_baseScale = 1.f;

    // Where it hovers, and its dash from one spot to another
    float m_homeX = 0.f;
    float m_dashFrom = 0.f;
    float m_dashTo = 0.f;
    float m_dashTime = -1.f;
    float m_nextDash = 2.4f;
    float m_afterimageClock = 0.f;

    float m_nextCharge = 2.6f;
    bool m_charging = false;
    float m_chargeTime = 0.f;
    // How big the charge looks; it eases back instead of snapping
    float m_chargeLook = 0.f;
    int m_chargeHits = 0;

    // Following the AI's plan: which attack is next, and when
    size_t m_planIndex = 0;
    float m_nextMove = 1.2f;
    bool m_feint = false;
    float m_darkTime = 0.f;
    cocos2d::CCLayerColor* m_darkness = nullptr;

    float m_nextShard = 1.2f;
    std::vector<Shard> m_shards;

    float m_lastShot = -1.f;
    float m_lastGrunt = -10.f;
    float m_emberClock = 0.f;
    float m_shakeTime = 0.f;
    float m_shakeStrength = 0.f;
};
