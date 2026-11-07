#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>

#include "Providers.hpp"

// "AI Connection": pick a provider, paste a key, choose a model, and test it.
// Everything is saved as the player types, per provider, so switching back
// and forth keeps each provider's key.
class ConnectionPopup : public geode::Popup {
public:
    static ConnectionPopup* create();

protected:
    // One labelled input line in the settings panel
    struct Row {
        cocos2d::CCNode* node = nullptr;
        cocos2d::CCLabelBMFont* label = nullptr;
        geode::TextInput* input = nullptr;
        cocos2d::CCMenu* buttons = nullptr;
    };

    bool init() override;
    Row createRow(char const* label, float inputWidth, float inputX, size_t maxChars);
    void refresh();
    void layoutRows();
    void setStatus(std::string const& text, cocos2d::ccColor3B color);

    void onPrevious(cocos2d::CCObject*);
    void onNext(cocos2d::CCObject*);
    void onGetKey(cocos2d::CCObject*);
    void onPaste(cocos2d::CCObject*);
    void onListModels(cocos2d::CCObject*);
    void onTest(cocos2d::CCObject*);
    void onDone(cocos2d::CCObject*);
    void onClose(cocos2d::CCObject*) override;
    void switchProvider(int step);

    Provider const* m_provider = nullptr;

    cocos2d::CCLabelBMFont* m_nameLabel = nullptr;
    cocos2d::CCLabelBMFont* m_tagLabel = nullptr;
    geode::Label* m_noteLabel = nullptr;
    geode::NineSlice* m_panel = nullptr;
    Row m_keyRow;
    Row m_urlRow;
    Row m_modelRow;
    CCMenuItemSpriteExtra* m_getKeyBtn = nullptr;
    geode::Label* m_statusLabel = nullptr;
    CCMenuItemSpriteExtra* m_testBtn = nullptr;

    geode::async::TaskHolder<geode::utils::web::WebResponse> m_test;
};
