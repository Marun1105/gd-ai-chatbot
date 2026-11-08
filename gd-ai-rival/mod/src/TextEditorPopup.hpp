#pragma once

#include <Geode/Geode.hpp>

#include <functional>
#include <string>
#include <vector>

struct TextEditorOptions {
    std::string title;
    // One plain line under the title explaining what to write
    std::string hint;
    std::string placeholder;
    // Cycled through by the "Example" button (hidden when empty)
    std::vector<std::string> examples;
    size_t maxLength = 200;
};

// A big text box for writing a paragraph: the custom personality, or a
// direction for one of the reactions. GD's own text inputs are one line that
// shrinks as you type, so this keeps a hidden one for the keyboard and draws
// the text itself, wrapped over several lines, with a live character count.
class TextEditorPopup : public geode::Popup {
public:
    using SaveCallback = std::function<void(std::string const&)>;

    static TextEditorPopup* create(TextEditorOptions options, std::string text, SaveCallback onSave);

protected:
    bool init(TextEditorOptions options, std::string text, SaveCallback onSave);
    void update(float dt) override;
    void onClose(cocos2d::CCObject*) override;

    void setText(std::string text);
    void refresh();
    void placeCursor();
    void hideNativeText();

    void onFocus(cocos2d::CCObject*);
    void onPaste(cocos2d::CCObject*);
    void onClear(cocos2d::CCObject*);
    void onExample(cocos2d::CCObject*);
    void onSave(cocos2d::CCObject*);

    TextEditorOptions m_options;
    std::string m_original;
    std::string m_text;
    SaveCallback m_onSave;
    size_t m_nextExample = 0;

    geode::TextInput* m_input = nullptr;
    geode::NineSlice* m_box = nullptr;
    geode::Label* m_textLabel = nullptr;
    geode::Label* m_placeholderLabel = nullptr;
    cocos2d::CCLabelBMFont* m_cursor = nullptr;
    cocos2d::CCLabelBMFont* m_tapHint = nullptr;
    cocos2d::CCLabelBMFont* m_counter = nullptr;
    float m_blink = 0.f;
    bool m_wasFocused = false;
};
