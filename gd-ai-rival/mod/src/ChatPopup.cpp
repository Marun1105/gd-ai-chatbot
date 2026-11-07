#include "ChatPopup.hpp"
#include "AIClient.hpp"
#include "ConnectionPopup.hpp"

using namespace geode::prelude;

namespace {
    constexpr float WIDTH = 380.f;
    constexpr float HEIGHT = 250.f;
    constexpr float HISTORY_WIDTH = 340.f;
    constexpr float HISTORY_HEIGHT = 120.f;
    constexpr float HISTORY_SCALE = .6f;

    ChatPopup* s_current = nullptr;

    // History uses rich text, so keep stray angle brackets from being read as color tags
    std::string escapeRich(std::string text) {
        for (auto& c : text) {
            if (c == '<') c = '(';
            else if (c == '>') c = ')';
        }
        return text;
    }
}

ChatPopup* ChatPopup::create() {
    auto ret = new ChatPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool ChatPopup::isOpen() {
    return s_current != nullptr;
}

ChatPopup::~ChatPopup() {
    if (s_current == this) {
        s_current = nullptr;
        AIClient::get().setUpdateCallback(nullptr);
    }
}

bool ChatPopup::init() {
    if (!Popup::init(WIDTH, HEIGHT)) return false;

    s_current = this;
    auto& client = AIClient::get();
    this->setTitle(fmt::format("Chat with {}", client.getName()));

    auto historyBG = NineSlice::create("square02b_001.png");
    historyBG->setColor({ 0, 0, 0 });
    historyBG->setOpacity(90);
    historyBG->setScale(.4f);
    historyBG->setContentSize(CCSize(HISTORY_WIDTH + 12.f, HISTORY_HEIGHT + 12.f) / .4f);
    m_mainLayer->addChildAtPosition(historyBG, Anchor::Center, ccp(0, 12));

    m_history = Label::createRich("", "chatFont.fnt");
    m_history->setAlignment(Label::Alignment::Left);
    m_history->setScale(HISTORY_SCALE);
    m_history->setMaxWidth(HISTORY_WIDTH / HISTORY_SCALE);
    m_history->setAnchorPoint({ 0.f, 1.f });
    m_mainLayer->addChildAtPosition(
        m_history, Anchor::Center,
        ccp(-HISTORY_WIDTH / 2, 12 + HISTORY_HEIGHT / 2)
    );

    m_status = CCLabelBMFont::create("", "chatFont.fnt");
    m_status->setScale(.55f);
    m_mainLayer->addChildAtPosition(m_status, Anchor::Bottom, ccp(0, 62));

    m_input = TextInput::create(260.f, "Say something...", "chatFont.fnt");
    m_input->setCommonFilter(CommonFilter::Any);
    m_input->setMaxCharCount(150);
    m_input->setDelegate(this);
    m_mainLayer->addChildAtPosition(m_input, Anchor::Bottom, ccp(-45, 32));

    auto sendSpr = ButtonSprite::create("Send", "goldFont.fnt", "GJ_button_01.png", .8f);
    sendSpr->setScale(.8f);
    m_sendBtn = CCMenuItemSpriteExtra::create(sendSpr, this, menu_selector(ChatPopup::onSend));
    m_buttonMenu->addChildAtPosition(m_sendBtn, Anchor::Bottom, ccp(135, 32));

    // Gear in the top right opens the AI Connection setup
    auto gearSpr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
    gearSpr->setScale(.55f);
    auto gearBtn = CCMenuItemSpriteExtra::create(gearSpr, this, menu_selector(ChatPopup::onConnection));
    m_buttonMenu->addChildAtPosition(gearBtn, Anchor::TopRight, ccp(-3, -3));

    client.setUpdateCallback([this] { this->refresh(); });
    this->refresh();

#ifdef GEODE_IS_DESKTOP
    // Delayed so the keybind's own key press doesn't get typed into the box
    this->scheduleOnce(schedule_selector(ChatPopup::focusInput), .1f);
#endif

    return true;
}

void ChatPopup::focusInput(float) {
    m_input->focus();
}

void ChatPopup::refresh() {
    auto& client = AIClient::get();
    bool waiting = client.isWaitingForChat();
    if (!waiting) {
        m_pendingMessage.clear();
    }

    std::vector<std::string> lines;
    for (auto const& turn : client.getMemory()) {
        auto text = escapeRich(turn.display);
        if (turn.fromAI) {
            lines.push_back(fmt::format("<cy>{}</c>", text));
        }
        else if (text.starts_with("* ")) {
            lines.push_back(fmt::format("<c-9a9a9a>{}</c>", text));
        }
        else {
            lines.push_back(text);
        }
    }
    if (!m_pendingMessage.empty()) {
        lines.push_back(fmt::format("You: {}", escapeRich(m_pendingMessage)));
    }

    if (lines.empty()) {
        auto missing = client.missingSetup();
        m_history->setRichText(missing.empty()
            ? fmt::format("<c-9a9a9a>{} is watching your run. Say something!</c>", client.getName())
            : fmt::format(
                "<c-9a9a9a>{} isn't connected to an AI yet.</c>\n"
                "Tap the <cy>gear</c> in the top right to pick one (some are free) and add {}.",
                client.getName(), missing
            )
        );
    }
    else {
        // Drop the oldest lines until the newest ones fit in the box
        size_t first = 0;
        while (true) {
            std::string text;
            for (size_t i = first; i < lines.size(); i++) {
                if (!text.empty()) text += "\n";
                text += lines[i];
            }
            m_history->setRichText(text);
            if (m_history->getScaledContentHeight() <= HISTORY_HEIGHT || first + 1 >= lines.size()) {
                break;
            }
            first++;
        }
    }

    if (waiting) {
        m_status->setString(fmt::format("{} is typing...", client.getName()).c_str());
        m_status->setColor({ 200, 200, 200 });
    }
    else if (auto error = client.getLastChatError(); !error.empty()) {
        m_status->setString(error.c_str());
        m_status->setColor({ 255, 140, 140 });
    }
    else {
        m_status->setString("");
    }
    m_status->limitLabelWidth(WIDTH - 30.f, .55f, .3f);

    m_sendBtn->setEnabled(!waiting);
    m_sendBtn->setOpacity(waiting ? 120 : 255);
}

void ChatPopup::onSend(CCObject*) {
    this->send();
}

void ChatPopup::onConnection(CCObject*) {
    ConnectionPopup::create()->show();
}

void ChatPopup::textInputReturn(CCTextInputNode*) {
    this->send();
}

void ChatPopup::enterPressed(CCTextInputNode*) {
    this->send();
}

void ChatPopup::send() {
    auto& client = AIClient::get();
    if (client.isWaitingForChat()) return;

    auto text = utils::string::trim(std::string(m_input->getString()));
    if (text.empty()) return;

    m_input->setString("");
    m_pendingMessage = text;
    client.sendChat(text);
    this->refresh();
}
