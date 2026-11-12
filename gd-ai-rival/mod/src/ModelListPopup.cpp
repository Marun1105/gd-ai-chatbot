#include "ModelListPopup.hpp"
#include "Text.hpp"

#include <algorithm>

using namespace geode::prelude;

namespace {
    constexpr float WIDTH = 320.f;
    constexpr float HEIGHT = 270.f;
    const CCSize LIST_SIZE { 280.f, 180.f };
    constexpr float ROW_HEIGHT = 22.f;

    struct BadgeStyle {
        char const* text;
        ccColor3B color;
    };

    BadgeStyle badgeStyle(providers::ModelBadge badge) {
        using enum providers::ModelBadge;
        switch (badge) {
            case Recommended: return { "RECOMMENDED", { 70, 200, 95 } };
            case Free: return { "FREE", { 60, 170, 230 } };
            case Balanced: return { "BALANCED", { 225, 170, 50 } };
            case Smartest: return { "SMARTEST", { 175, 100, 235 } };
            case None: default: return { nullptr, { 0, 0, 0 } };
        }
    }

    // Recommended first, then free ones, then the rest as the provider lists them
    int sortRank(providers::ModelBadge badge) {
        using enum providers::ModelBadge;
        if (badge == Recommended) return 0;
        if (badge == Free) return 1;
        return 2;
    }

    // A small colored pill with the label, right-aligned in the row
    CCNode* createBadge(BadgeStyle const& style) {
        auto label = CCLabelBMFont::create(style.text, "bigFont.fnt");
        label->setScale(.28f);
        auto size = label->getScaledContentSize() + CCSize(10.f, 5.f);

        auto pill = NineSlice::create("square02b_001.png");
        pill->setColor(style.color);
        pill->setScale(.3f);
        pill->setContentSize(size / .3f);

        auto node = CCNode::create();
        node->setContentSize(size);
        node->setAnchorPoint({ 1.f, .5f });
        node->addChildAtPosition(pill, Anchor::Center);
        node->addChildAtPosition(label, Anchor::Center, ccp(0, .5f));
        return node;
    }
}

ModelListPopup* ModelListPopup::create(Connection connection, Callback onPick) {
    auto ret = new ModelListPopup();
    if (ret->init(std::move(connection), std::move(onPick))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool ModelListPopup::init(Connection connection, Callback onPick) {
    if (!Popup::init(WIDTH, HEIGHT)) return false;

    m_connection = std::move(connection);
    m_onPick = std::move(onPick);
    this->setTitle("Pick a Model");

    auto legend = CCLabelBMFont::create("Recommended: fast and cheap, plenty for in-game chat", "chatFont.fnt");
    legend->setScale(.5f);
    legend->setColor({ 170, 230, 180 });
    m_mainLayer->addChildAtPosition(legend, Anchor::Top, ccp(0, -40));

    auto listBG = NineSlice::create("square02b_001.png");
    listBG->setColor({ 0, 0, 0 });
    listBG->setOpacity(80);
    listBG->setScale(.4f);
    listBG->setContentSize((LIST_SIZE + CCSize(10.f, 10.f)) / .4f);
    m_mainLayer->addChildAtPosition(listBG, Anchor::Center, ccp(0, -12));

    m_spinner = LoadingSpinner::create(30.f);
    m_mainLayer->addChildAtPosition(m_spinner, Anchor::Center, ccp(0, -12));

    m_request.spawn(
        "AI Rival model list",
        providers::listModels(m_connection),
        [this](web::WebResponse response) {
            auto result = providers::parseModels(m_connection, response);
            if (result.isOk()) {
                this->showModels(result.unwrap());
            }
            else {
                this->showError(result.unwrapErr());
            }
        }
    );
    return true;
}

void ModelListPopup::showModels(std::vector<std::string> models) {
    m_spinner->setVisible(false);
    m_models = std::move(models);
    std::stable_sort(m_models.begin(), m_models.end(), [](std::string const& a, std::string const& b) {
        return sortRank(providers::modelBadge(a)) < sortRank(providers::modelBadge(b));
    });

    auto list = ScrollLayer::create(LIST_SIZE);
    list->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(0.f));

    for (size_t i = 0; i < m_models.size(); i++) {
        auto const& model = m_models[i];

        // Full-width hit area, so the whole row is tappable, not just the text
        auto rowContent = CCNode::create();
        rowContent->setContentSize({ LIST_SIZE.width, ROW_HEIGHT });

        auto label = CCLabelBMFont::create(text::toDisplayable(model).c_str(), "chatFont.fnt");
        label->setAnchorPoint({ 0.f, .5f });
        label->setPosition({ 8.f, ROW_HEIGHT / 2 });
        float labelSpace = LIST_SIZE.width - 16.f;
        auto style = badgeStyle(providers::modelBadge(model));
        if (style.text) {
            auto badge = createBadge(style);
            badge->setPosition({ LIST_SIZE.width - 8.f, ROW_HEIGHT / 2 });
            rowContent->addChild(badge);
            labelSpace -= badge->getContentWidth() + 8.f;
        }
        label->limitLabelWidth(labelSpace, .7f, .3f);
        if (model == m_connection.model) {
            label->setColor({ 120, 255, 140 });
        }
        rowContent->addChild(label);

        auto button = CCMenuItemSpriteExtra::create(rowContent, this, menu_selector(ModelListPopup::onPick));
        button->setTag(static_cast<int>(i));
        button->m_scaleMultiplier = 1.f;

        auto row = CCMenu::create();
        row->setContentSize(rowContent->getContentSize());
        row->ignoreAnchorPointForPosition(false);
        row->addChildAtPosition(button, Anchor::Center);
        list->m_contentLayer->addChild(row);
    }
    list->m_contentLayer->updateLayout();

    // A short list would otherwise sit at the bottom of the box
    if (list->m_contentLayer->getContentHeight() < LIST_SIZE.height) {
        auto diff = LIST_SIZE.height - list->m_contentLayer->getContentHeight();
        list->m_contentLayer->setContentHeight(LIST_SIZE.height);
        for (auto child : CCArrayExt<CCNode*>(list->m_contentLayer->getChildren())) {
            child->setPositionY(child->getPositionY() + diff);
        }
    }
    list->scrollToTop();

    m_mainLayer->addChildAtPosition(list, Anchor::Center, ccp(-LIST_SIZE.width / 2, -12 - LIST_SIZE.height / 2));
    m_mainLayer->addChildAtPosition(
        Scrollbar::create(list), Anchor::Center, ccp(LIST_SIZE.width / 2 + 12.f, -12)
    );
}

void ModelListPopup::showError(std::string const& message) {
    m_spinner->setVisible(false);
    auto label = Label::create(text::toDisplayable(message), "chatFont.fnt");
    label->setAlignment(Label::Alignment::Center);
    label->setScale(.6f);
    label->setMaxWidth((LIST_SIZE.width - 20.f) / .6f);
    label->setColor({ 255, 130, 130 });
    m_mainLayer->addChildAtPosition(label, Anchor::Center, ccp(0, -12));
}

void ModelListPopup::onPick(CCObject* sender) {
    auto index = static_cast<size_t>(sender->getTag());
    if (index < m_models.size() && m_onPick) {
        m_onPick(m_models[index]);
    }
    this->onClose(nullptr);
}
