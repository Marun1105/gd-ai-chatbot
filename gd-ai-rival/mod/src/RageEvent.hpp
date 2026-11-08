#pragma once

#include <Geode/Geode.hpp>

#include <functional>
#include <vector>

class RageFight;

struct RageOutcome {
    // The fight mini-game was played, and how it went
    bool fought = false;
    bool won = false;
    int hits = 0;
};

// Grin snaps: the screen goes dark, it roars and lunges at you, the glass
// cracks where it hits, then the whole screen (a snapshot of the level taken
// the moment you died) shatters into pieces that fall away. Plays over
// everything and holds the level reset until it's over.
class RageEvent : public cocos2d::CCLayer {
public:
    static void play(std::function<void(RageOutcome)> onDone);
    static bool isActive();

    // The creature's face, drawn once into a sprite (also used by the bar)
    static cocos2d::CCSprite* createFace(float size);

protected:
    struct Shard {
        cocos2d::CCSprite* sprite = nullptr;
        cocos2d::CCPoint velocity;
        float spin = 0.f;
        float delay = 0.f;
    };

    bool init(std::function<void(RageOutcome)> onDone);
    void onExit() override;
    void registerWithTouchDispatcher() override;
    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent*) override;
    void keyBackClicked() override {}

    void planCracks();
    void buildShards(float);
    cocos2d::CCSprite* cutShard(std::vector<cocos2d::CCPoint> const& polygon);

    void roar();
    void lunge();
    void impact();
    void growCracks(float dt);
    void shatter();
    void fall(float dt);
    void shake(float dt);
    void afterShatter();
    void roarFrame(float);
    void fadeAway();
    void finish();

    std::function<void(RageOutcome)> m_onDone;
    RageOutcome m_outcome;
    RageFight* m_fight = nullptr;
    // The 3D renders of the face, closed and open mouth (null if missing)
    cocos2d::CCTexture2D* m_faceTexture = nullptr;
    cocos2d::CCTexture2D* m_roarTexture = nullptr;
    bool m_mouthOpen = false;
    bool m_finished = false;
    cocos2d::CCSize m_win;

    cocos2d::CCNode* m_root = nullptr;
    cocos2d::CCRenderTexture* m_snapshot = nullptr;
    cocos2d::CCLayerColor* m_black = nullptr;
    cocos2d::CCSprite* m_screen = nullptr;
    cocos2d::CCDrawNode* m_cracks = nullptr;
    cocos2d::CCNode* m_shardLayer = nullptr;
    cocos2d::CCSprite* m_face = nullptr;
    cocos2d::CCLayerColor* m_flash = nullptr;

    // Crack pattern: rays from the impact, crossing rings of points
    cocos2d::CCPoint m_impact;
    std::vector<std::vector<cocos2d::CCPoint>> m_rays;
    std::vector<std::vector<cocos2d::CCPoint>> m_pieces;
    size_t m_nextPiece = 0;
    std::vector<Shard> m_shards;

    float m_crackTime = 0.f;
    float m_shakeTime = 0.f;
    float m_shakeStrength = 0.f;
    float m_fallTime = 0.f;
};
