#include "ConnectionPopup.hpp"
#include "AIClient.hpp"
#include "ModelListPopup.hpp"
#include "Prompt.hpp"
#include "Text.hpp"

using namespace geode::prelude;

namespace {
    constexpr float WIDTH = 400.f;
    constexpr float HEIGHT = 300.f;
    constexpr float ROW_WIDTH = 370.f;
    constexpr float ROW_HEIGHT = 30.f;
    constexpr float ROW_SPACING = 34.f;
    // Middle of the free space between the provider note and the status line
    constexpr float ROWS_CENTER_Y = -19.f;

    constexpr ccColor3B GRAY { 190, 190, 190 };
    constexpr ccColor3B GREEN { 120, 255, 140 };
    constexpr ccColor3B RED { 255, 130, 130 };
    constexpr ccColor3B ORANGE { 255, 190, 80 };

    CCMenuItemSpriteExtra* smallButton(char const* text, char const* bg, CCObject* target, SEL_MenuHandler selector) {
        auto sprite = ButtonSprite::create(text, "goldFont.fnt", bg, .8f);
        sprite->setScale(.5f);
        return CCMenuItemSpriteExtra::create(sprite, target, selector);
    }
}

ConnectionPopup* ConnectionPopup::create() {
    auto ret = new ConnectionPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

ConnectionPopup::Row ConnectionPopup::createRow(char const* label, float inputWidth, float inputX, size_t maxChars) {
    Row row;
    row.node = CCNode::create();
    row.node->setContentSize({ ROW_WIDTH, ROW_HEIGHT });
    row.node->setAnchorPoint({ .5f, .5f });

    row.label = CCLabelBMFont::create(label, "goldFont.fnt");
    row.label->setAnchorPoint({ 0.f, .5f });
    row.label->setPosition({ 10.f, ROW_HEIGHT / 2 });
    row.node->addChild(row.label);

    row.input = TextInput::create(inputWidth, "", "chatFont.fnt");
    row.input->setCommonFilter(CommonFilter::Any);
    row.input->setMaxCharCount(maxChars);
    row.input->setPosition({ inputX, ROW_HEIGHT / 2 });
    row.node->addChild(row.input);

    row.buttons = CCMenu::create();
    row.buttons->setContentSize(row.node->getContentSize());
    row.buttons->ignoreAnchorPointForPosition(false);
    row.buttons->setAnchorPoint({ 0.f, 0.f });
    row.buttons->setPosition({ 0.f, 0.f });
    row.node->addChild(row.buttons);

    m_mainLayer->addChildAtPosition(row.node, Anchor::Center);
    return row;
}

bool ConnectionPopup::init() {
    if (!Popup::init(WIDTH, HEIGHT)) return false;

    m_provider = &providers::current();
    this->setTitle("AI Connection");

    // Provider picker: < Name >
    auto arrowMenu = CCMenu::create();
    arrowMenu->setContentSize({ WIDTH - 40.f, 40.f });
    arrowMenu->ignoreAnchorPointForPosition(false);
    m_mainLayer->addChildAtPosition(arrowMenu, Anchor::Center, ccp(0, 98));

    auto prevSprite = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    prevSprite->setScale(.55f);
    auto prevBtn = CCMenuItemSpriteExtra::create(prevSprite, this, menu_selector(ConnectionPopup::onPrevious));
    arrowMenu->addChildAtPosition(prevBtn, Anchor::Left, ccp(20, 0));

    auto nextSprite = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    nextSprite->setFlipX(true);
    nextSprite->setScale(.55f);
    auto nextBtn = CCMenuItemSpriteExtra::create(nextSprite, this, menu_selector(ConnectionPopup::onNext));
    arrowMenu->addChildAtPosition(nextBtn, Anchor::Right, ccp(-20, 0));

    m_nameLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_mainLayer->addChildAtPosition(m_nameLabel, Anchor::Center, ccp(0, 98));

    // FREE / PAID tag and the "Get Key" link share a line under the name
    m_tagLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_tagLabel->setScale(.38f);
    m_mainLayer->addChildAtPosition(m_tagLabel, Anchor::Center, ccp(-45, 76));

    auto linkMenu = CCMenu::create();
    linkMenu->setContentSize({ 120.f, 24.f });
    linkMenu->ignoreAnchorPointForPosition(false);
    m_mainLayer->addChildAtPosition(linkMenu, Anchor::Center, ccp(45, 76));
    m_getKeyBtn = smallButton("Get Key", "GJ_button_02.png", this, menu_selector(ConnectionPopup::onGetKey));
    linkMenu->addChildAtPosition(m_getKeyBtn, Anchor::Center);

    m_noteLabel = Label::create("", "chatFont.fnt");
    m_noteLabel->setAlignment(Label::Alignment::Center);
    m_noteLabel->setScale(.55f);
    m_noteLabel->setMaxWidth((WIDTH - 50.f) / .55f);
    m_noteLabel->setColor(GRAY);
    m_mainLayer->addChildAtPosition(m_noteLabel, Anchor::Center, ccp(0, 52));

    // Settings panel behind the input rows
    m_panel = NineSlice::create("square02b_001.png");
    m_panel->setColor({ 0, 0, 0 });
    m_panel->setOpacity(80);
    m_panel->setScale(.4f);
    m_mainLayer->addChildAtPosition(m_panel, Anchor::Center, ccp(0, ROWS_CENTER_Y));

    m_keyRow = this->createRow("API Key", 200.f, 185.f, 300);
    m_keyRow.input->setPasswordMode(true);
    m_keyRow.input->setCallback([this](std::string const& value) {
        providers::saveKey(*m_provider, value);
    });
    m_keyRow.buttons->addChildAtPosition(
        smallButton("Paste", "GJ_button_04.png", this, menu_selector(ConnectionPopup::onPaste)),
        Anchor::Left, ccp(330, 0)
    );

    m_urlRow = this->createRow("API URL", 270.f, 220.f, 200);
    m_urlRow.input->setCallback([this](std::string const& value) {
        providers::saveUrl(*m_provider, value);
    });

    m_modelRow = this->createRow("Model", 210.f, 190.f, 100);
    m_modelRow.input->setCallback([this](std::string const& value) {
        providers::saveModel(*m_provider, value);
    });
    m_modelRow.buttons->addChildAtPosition(
        smallButton("List", "GJ_button_04.png", this, menu_selector(ConnectionPopup::onListModels)),
        Anchor::Left, ccp(335, 0)
    );

    m_statusLabel = Label::create("", "chatFont.fnt");
    m_statusLabel->setAlignment(Label::Alignment::Center);
    m_statusLabel->setScale(.55f);
    m_statusLabel->setMaxWidth((WIDTH - 40.f) / .55f);
    m_mainLayer->addChildAtPosition(m_statusLabel, Anchor::Center, ccp(0, -92));

    m_testBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Test", "goldFont.fnt", "GJ_button_02.png", .8f),
        this, menu_selector(ConnectionPopup::onTest)
    );
    m_buttonMenu->addChildAtPosition(m_testBtn, Anchor::Bottom, ccp(-55, 24));

    auto doneBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Done", "goldFont.fnt", "GJ_button_01.png", .8f),
        this, menu_selector(ConnectionPopup::onDone)
    );
    m_buttonMenu->addChildAtPosition(doneBtn, Anchor::Bottom, ccp(55, 24));

    this->refresh();
    return true;
}

void ConnectionPopup::refresh() {
    auto const& provider = *m_provider;
    bool isProxy = provider.format == ApiFormat::Proxy;

    m_nameLabel->setString(std::string(provider.name).c_str());
    m_nameLabel->limitLabelWidth(WIDTH - 130.f, .6f, .3f);

    if (provider.id == "custom") {
        m_tagLabel->setString("");
    }
    else {
        m_tagLabel->setString(provider.free ? "FREE" : "PAID");
        m_tagLabel->setColor(provider.free ? GREEN : ORANGE);
    }
    m_getKeyBtn->setVisible(!provider.keyUrl.empty());
    if (auto sprite = typeinfo_cast<ButtonSprite*>(m_getKeyBtn->getNormalImage())) {
        sprite->setString(provider.id == "ollama" ? "Download" : "Get Key");
    }
    // With no link button, the tag sits alone in the middle
    m_tagLabel->updateAnchoredPosition(Anchor::Center, ccp(m_getKeyBtn->isVisible() ? -45.f : 0.f, 76.f));

    m_noteLabel->setText(std::string(provider.note));

    m_keyRow.node->setVisible(provider.needsKey || provider.id == "custom");
    m_keyRow.input->setPlaceholder(provider.needsKey ? "Paste your API key" : "Optional");
    m_keyRow.input->setString(providers::savedKey(provider));

    m_urlRow.node->setVisible(provider.urlEditable);
    m_urlRow.label->setString(isProxy ? "Proxy URL" : "API URL");
    m_urlRow.input->setPlaceholder(
        provider.defaultUrl.empty()
            ? (isProxy ? "https://your-proxy.workers.dev/chat" : "https://api.example.com/v1")
            : std::string(provider.defaultUrl)
    );
    m_urlRow.input->setString(providers::savedUrl(provider));

    m_modelRow.node->setVisible(!isProxy);
    // An empty model field means "use the default", shown as the placeholder
    m_modelRow.input->setPlaceholder(provider.defaultModel.empty() ? "Model name" : std::string(provider.defaultModel));
    m_modelRow.input->setString(providers::savedModel(provider));

    for (auto row : { &m_keyRow, &m_urlRow, &m_modelRow }) {
        row->label->limitLabelWidth(72.f, .45f, .3f);
    }
    this->layoutRows();

    auto missing = providers::load(provider).missing();
    if (missing.empty()) {
        this->setStatus("Ready. Press Test to make sure it works.", GRAY);
    }
    else {
        this->setStatus(fmt::format("Add {} to use {}.", missing, provider.name), GRAY);
    }
}

void ConnectionPopup::layoutRows() {
    std::vector<Row*> visible;
    for (auto row : { &m_keyRow, &m_urlRow, &m_modelRow }) {
        if (row->node->isVisible()) visible.push_back(row);
    }

    m_panel->setVisible(!visible.empty());
    auto count = static_cast<float>(visible.size());
    m_panel->setContentSize(CCSize(ROW_WIDTH + 10.f, count * ROW_SPACING + 10.f) / m_panel->getScale());

    float y = ROWS_CENTER_Y + (count - 1) * ROW_SPACING / 2;
    for (auto row : visible) {
        row->node->updateAnchoredPosition(Anchor::Center, ccp(0.f, y));
        y -= ROW_SPACING;
    }
}

void ConnectionPopup::setStatus(std::string const& text, ccColor3B color) {
    m_statusLabel->setText(text::toDisplayable(text));
    m_statusLabel->setColor(color);
}

void ConnectionPopup::switchProvider(int step) {
    auto all = providers::all();
    auto count = static_cast<int>(all.size());
    int index = static_cast<int>(m_provider - all.data());
    index = ((index + step) % count + count) % count;

    m_test.cancel();
    m_testBtn->setEnabled(true);
    m_testBtn->setOpacity(255);

    m_provider = &all[index];
    providers::setCurrent(*m_provider);
    this->refresh();
}

void ConnectionPopup::onPrevious(CCObject*) {
    this->switchProvider(-1);
}

void ConnectionPopup::onNext(CCObject*) {
    this->switchProvider(1);
}

void ConnectionPopup::onGetKey(CCObject*) {
    if (!m_provider->keyUrl.empty()) {
        web::openLinkInBrowser(std::string(m_provider->keyUrl));
    }
}

void ConnectionPopup::onPaste(CCObject*) {
    std::string key;
    for (char c : utils::clipboard::read()) {
        if (!std::isspace(static_cast<unsigned char>(c))) key += c;
    }
    if (key.empty()) {
        this->setStatus("Your clipboard is empty. Copy your API key first.", RED);
        return;
    }
    m_keyRow.input->setString(key, true);
    this->setStatus(fmt::format("Key pasted ({}). Press Test to check it.", text::maskKey(key)), GRAY);
}

void ConnectionPopup::onListModels(CCObject*) {
    auto connection = providers::load(*m_provider);
    if (!providers::canListModels(connection)) {
        this->setStatus("Add your API key first, then load the model list.", RED);
        return;
    }
    ModelListPopup::create(connection, [this](std::string const& model) {
        m_modelRow.input->setString(model, true);
        this->setStatus(fmt::format("Model set to {}. Press Test to try it.", model), GRAY);
    })->show();
}

void ConnectionPopup::onTest(CCObject*) {
    auto connection = providers::load(*m_provider);
    if (auto missing = connection.missing(); !missing.empty()) {
        this->setStatus(fmt::format("Add {} first.", missing), RED);
        return;
    }

    auto& client = AIClient::get();
    ChatPayload payload;
    payload.persona = prompt::persona(client.getName());
    payload.system = prompt::system(payload.persona);
    payload.message = "[PLAYER] Quick connection test: say hi to me in one short line. (attempt 1, best 0%)";
    payload.playerID = client.getPlayerID();
    if (auto playLayer = PlayLayer::get(); playLayer && playLayer->m_level) {
        payload.levelID = playLayer->m_level->m_levelID.value();
    }

    this->setStatus(fmt::format("Talking to {}...", connection.provider->name), GRAY);
    m_testBtn->setEnabled(false);
    m_testBtn->setOpacity(120);

    m_test.spawn(
        "AI Rival connection test",
        providers::sendChat(connection, payload),
        [this, connection](web::WebResponse response) {
            m_testBtn->setEnabled(true);
            m_testBtn->setOpacity(255);
            auto result = providers::parseChat(connection, response);
            if (result.isOk()) {
                this->setStatus(fmt::format("It works! {}: {}", AIClient::get().getName(), result.unwrap()), GREEN);
            }
            else {
                this->setStatus(result.unwrapErr(), RED);
            }
        }
    );
}

void ConnectionPopup::onDone(CCObject* sender) {
    this->onClose(sender);
}

void ConnectionPopup::onClose(CCObject* sender) {
    AIClient::get().connectionChanged();
    Popup::onClose(sender);
}
