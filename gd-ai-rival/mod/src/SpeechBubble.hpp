#pragma once

#include <Geode/Geode.hpp>

// A speech bubble shown over the level that fades out after a while.
// There is only ever one; showing a new one replaces the old.
class SpeechBubble : public cocos2d::CCNode {
public:
    static void show(std::string const& name, std::string const& text, bool isError = false);
    // "..." placeholder while waiting for a chat reply
    static void showTyping(std::string const& name);

protected:
    enum class Style { Reply, Error, Typing };

    static SpeechBubble* create(std::string const& name, std::string const& text, Style style);
    static void attach(SpeechBubble* bubble);

    bool init(std::string const& name, std::string const& text, Style style);
    void fadeOut();

    geode::NineSlice* m_bg = nullptr;
    geode::Label* m_nameLabel = nullptr;
    geode::Label* m_bodyLabel = nullptr;
};
