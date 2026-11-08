#pragma once

#include <Geode/Geode.hpp>

#include "Profile.hpp"

// A speech bubble shown over the level that fades out after a while.
// There is only ever one; showing a new one replaces the old. Its look,
// avatar and sound come from the current profile.
class SpeechBubble : public cocos2d::CCNode {
public:
    static void show(Profile const& profile, std::string const& text, bool isError = false);
    // Animated "..." while waiting for a chat reply
    static void showTyping(Profile const& profile);
    // For the bubble designer: not attached to the level and never fades.
    // With animate, the text types itself out with the talking sound.
    static SpeechBubble* createPreview(Profile const& profile, std::string const& text, bool animate);

    // Where the bubble goes on a screen-sized parent
    void place(BubbleStyle const& style);
    // Center of a bubble of this size at an anchor, before any nudge
    static cocos2d::CCPoint anchorPosition(BubbleAnchor anchor, cocos2d::CCSize size, cocos2d::CCSize screen);

protected:
    enum class Style { Reply, Error, Typing };

    static SpeechBubble* create(Profile const& profile, std::string const& text, Style style);
    static void attach(SpeechBubble* bubble);

    bool initBubble(Profile const& profile, std::string const& text, Style style);
    cocos2d::CCNode* createAvatar(BubbleStyle const& style);
    void revealStep(float);
    void typingStep(float);
    void startTimer(float seconds);
    void playBlip();
    void fadeOut();

    geode::NineSlice* m_bg = nullptr;
    geode::Label* m_nameLabel = nullptr;
    geode::Label* m_bodyLabel = nullptr;
    cocos2d::CCNode* m_avatar = nullptr;

    // Letter-by-letter reveal
    cocos2d::ccColor3B m_textColor { 255, 255, 255 };
    size_t m_charCount = 0;
    size_t m_revealed = 0;
    float m_holdTime = 0.f;

    std::string m_soundFile;
    float m_volume = 0.f;
    int m_typingDots = 0;
    bool m_preview = false;
    bool m_animate = false;
};
