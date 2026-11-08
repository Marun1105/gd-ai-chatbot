#pragma once

#include <Geode/Geode.hpp>

#include <functional>
#include <string>
#include <vector>

#include "Avatar.hpp"

// Drawing surface (8 to 32 squares per side): tap or drag to paint with the
// chosen palette slot
class PixelCanvas : public cocos2d::CCLayer {
public:
    enum class Tool { Pen, Fill };

    // extent: width and height of the canvas on screen
    static PixelCanvas* create(float extent, std::string const& pixels);

    std::string const& getPixels() const { return m_pixels; }
    void setPixels(std::string const& pixels, bool remember);
    void undo();
    void setMirror(bool mirror);
    bool canUndo() const { return !m_history.empty(); }
    int gridSize() const { return m_size; }

    // Palette slot to paint with; -1 erases
    int m_slot = 9;
    Tool m_tool = Tool::Pen;
    bool m_mirror = false;
    std::function<void(std::string const&)> m_onChange;

protected:
    bool init(float extent, std::string const& pixels);
    // Recompute square size and redraw everything after the size changed
    void applySize();
    void registerWithTouchDispatcher() override;
    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchCancelled(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;

    bool paintAt(cocos2d::CCPoint local);
    bool setPixel(int row, int col, char value);
    bool fill(int row, int col, char value);
    void redraw();
    void drawGuides();

    float m_extent = 160.f;
    int m_size = 16;
    float m_cell = 10.f;
    cocos2d::CCDrawNode* m_checker = nullptr;
    std::string m_pixels;
    std::string m_strokeStart;
    std::vector<std::string> m_history;
    cocos2d::CCSprite* m_sprite = nullptr;
    cocos2d::CCDrawNode* m_guides = nullptr;
};

// Avatar editor: pick a cube and its colors from GD's icon kit, draw your own
// 16x16 icon, mirror your own icon, or turn the avatar off. A preview on the
// left shows the result as it will look in the speech bubble.
class AvatarEditor : public geode::Popup {
public:
    using SaveCallback = std::function<void(AvatarLook const&)>;

    static AvatarEditor* create(AvatarLook look, SaveCallback onSave);

protected:
    enum class Tab { Cube, Color1, Color2, Glow };

    bool init(AvatarLook look, SaveCallback onSave);
    void onClose(cocos2d::CCObject*) override;

    void setMode(AvatarMode mode);
    void rebuildRight();
    void buildIconKit();
    void buildCubePage();
    void buildColorGrid();
    void buildDrawing();
    void buildInfo(char const* title, char const* text, bool withCopyButton);
    void refreshPreview();
    void refreshToolButtons();
    void moveCubeHighlight();
    void moveColorHighlight();
    int selectedColor() const;

    void onMode(cocos2d::CCObject* sender);
    void onTab(cocos2d::CCObject* sender);
    void onCube(cocos2d::CCObject* sender);
    void onPage(cocos2d::CCObject* sender);
    void onColor(cocos2d::CCObject* sender);
    void onSlot(cocos2d::CCObject* sender);
    void onGridSize(cocos2d::CCObject* sender);
    void onTool(cocos2d::CCObject* sender);
    void onMirror(cocos2d::CCObject*);
    void onUndo(cocos2d::CCObject*);
    void onClearCanvas(cocos2d::CCObject*);
    void onStarter(cocos2d::CCObject*);
    void onCopyCode(cocos2d::CCObject*);
    void onPasteCode(cocos2d::CCObject*);
    void onCopyMine(cocos2d::CCObject*);
    void onSave(cocos2d::CCObject*);

    AvatarLook m_look;
    AvatarLook m_original;
    SaveCallback m_onSave;
    Tab m_tab = Tab::Cube;
    int m_page = 0;
    std::vector<int> m_colorOrder;

    cocos2d::CCNode* m_previewHolder = nullptr;
    cocos2d::CCNode* m_bubbleHolder = nullptr;
    cocos2d::CCNode* m_right = nullptr;
    std::vector<ButtonSprite*> m_modeSprites;
    std::vector<ButtonSprite*> m_tabSprites;

    // Icon kit
    cocos2d::CCMenu* m_gridMenu = nullptr;
    cocos2d::CCNode* m_highlight = nullptr;
    cocos2d::CCLabelBMFont* m_pageLabel = nullptr;

    // Drawing
    PixelCanvas* m_canvas = nullptr;
    cocos2d::CCNode* m_slotHighlight = nullptr;
    cocos2d::CCMenu* m_paletteMenu = nullptr;
    ButtonSprite* m_penSprite = nullptr;
    ButtonSprite* m_fillSprite = nullptr;
    ButtonSprite* m_mirrorSprite = nullptr;
    std::vector<std::pair<int, ButtonSprite*>> m_sizeSprites;
};
