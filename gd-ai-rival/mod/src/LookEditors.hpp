#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/SliderNode.hpp>

#include <functional>
#include <string>
#include <vector>

#include "Look.hpp"
#include "Profile.hpp"

class SpeechBubble;
class BubbleDragArea;

// Shared bits of the two designers: a title + slider + value row
struct SliderRow {
    geode::SliderNode* slider = nullptr;
    cocos2d::CCLabelBMFont* value = nullptr;
};

// Speech bubble designer. Controls on the left in four tabs, and on the right
// the real bubble on a small copy of the game screen (drag it to move it) plus
// a close-up that can replay the reply letter by letter with the voice.
class BubbleDesigner : public geode::Popup {
public:
    using SaveCallback = std::function<void(BubbleLook const&)>;

    static BubbleDesigner* create(BubbleLook look, SaveCallback onSave);

    // Called by the drag area (world coordinates) while the bubble is dragged / dropped
    void dragBy(cocos2d::CCPoint location, cocos2d::CCPoint previous);
    void dragEnded();

protected:
    enum class Tab { Place, Text, Colors, Voice };

    bool init(BubbleLook look, SaveCallback onSave);
    void onClose(cocos2d::CCObject*) override;

    Profile previewProfile() const;
    void refreshPreview(bool animate);
    void placeScreenBubble();
    void rebuildControls();
    void buildPlace();
    void buildText();
    void buildColors();
    void buildVoice();
    void syncNudgeRows();

    SliderRow addRow(
        char const* title, float y, float min, float max, float step, float value,
        std::function<std::string(float)> format, std::function<void(float)> onChange
    );
    cocos2d::CCLabelBMFont* addHeader(char const* title, float y);
    cocos2d::CCMenu* addMenu(cocos2d::CCSize size, cocos2d::CCPoint offset);

    void onTab(cocos2d::CCObject* sender);
    void onAnchor(cocos2d::CCObject* sender);
    void onCenter(cocos2d::CCObject*);
    void onFont(cocos2d::CCObject* sender);
    void onTypewriter(cocos2d::CCObject*);
    void onTheme(cocos2d::CCObject* sender);
    void onPickColor(cocos2d::CCObject* sender);
    void onSound(cocos2d::CCObject* sender);
    void onReplay(cocos2d::CCObject*);
    void onReset(cocos2d::CCObject*);
    void onSave(cocos2d::CCObject*);

    BubbleLook m_look;
    BubbleLook m_original;
    SaveCallback m_onSave;
    Tab m_tab = Tab::Place;

    cocos2d::CCNode* m_controls = nullptr;
    std::vector<ButtonSprite*> m_tabSprites;
    SliderRow m_xRow;
    SliderRow m_yRow;

    // Preview
    cocos2d::CCNode* m_screen = nullptr;
    float m_screenScale = 1.f;
    SpeechBubble* m_screenBubble = nullptr;
    cocos2d::CCNode* m_closeUp = nullptr;
    BubbleDragArea* m_dragArea = nullptr;
};

// Chat window look: theme, color and size, with a live copy of the window
class ChatLookEditor : public geode::Popup {
public:
    using SaveCallback = std::function<void(ChatLook const&)>;

    static ChatLookEditor* create(ChatLook look, SaveCallback onSave);

protected:
    bool init(ChatLook look, SaveCallback onSave);
    void onClose(cocos2d::CCObject*) override;

    void rebuildControls();
    void refreshPreview();

    void onTheme(cocos2d::CCObject* sender);
    void onPreset(cocos2d::CCObject* sender);
    void onPickColor(cocos2d::CCObject*);
    void onSize(cocos2d::CCObject* sender);
    void onSave(cocos2d::CCObject*);

    ChatLook m_look;
    ChatLook m_original;
    SaveCallback m_onSave;

    cocos2d::CCNode* m_controls = nullptr;
    cocos2d::CCNode* m_preview = nullptr;
};

namespace look {
    // Small static picture of a bubble or chat window for a settings row
    cocos2d::CCNode* bubbleThumbnail(BubbleLook const& look, cocos2d::CCSize fit);
    cocos2d::CCNode* chatThumbnail(ChatLook const& look, cocos2d::CCSize fit);
}
