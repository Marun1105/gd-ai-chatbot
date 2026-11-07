#pragma once

#include <Geode/Geode.hpp>

// Chat box opened from the pause menu, the in-game button or the keybind.
// Shows the recent conversation and stays open so you can keep talking.
class ChatPopup : public geode::Popup {
public:
    static ChatPopup* create();
    static bool isOpen();

protected:
    ~ChatPopup() override;

    bool init() override;
    void refresh();
    void focusInput(float);
    void onSend(cocos2d::CCObject*);
    void onConnection(cocos2d::CCObject*);
    void send();

    // Sends on Enter, on platforms where GD reports it. A separate object,
    // because in GD 2.2091 FLAlertLayer is already a TextInputDelegate.
    struct EnterKeyDelegate : TextInputDelegate {
        ChatPopup* owner = nullptr;
        void textInputReturn(CCTextInputNode*) override;
        void enterPressed(CCTextInputNode*) override;
    };

    EnterKeyDelegate m_enterKey;
    geode::TextInput* m_input = nullptr;
    geode::Label* m_history = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    CCMenuItemSpriteExtra* m_sendBtn = nullptr;
    // Shown at the bottom of the history until the reply arrives
    std::string m_pendingMessage;
};
