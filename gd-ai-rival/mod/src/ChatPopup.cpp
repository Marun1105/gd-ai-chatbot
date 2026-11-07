#include "ChatPopup.hpp"
#include "AIClient.hpp"
#include "ConnectionPopup.hpp"
#include "Profile.hpp"

using namespace geode::prelude;

namespace {
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
    auto look = profile::current();
    m_width = look.chatLarge ? 460.f : 380.f;
    m_height = look.chatLarge ? 310.f : 250.f;
    m_historyWidth = m_width - 40.f;
    m_historyHeight = m_height - 130.f;

    // A custom colour tints a plain panel; GD's brown panel can't be recoloured nicely
    if (!Popup::init(m_width, m_height, look.chatCustomColor ? "square02b_001.png" : "GJ_square01.png")) return false;
    if (look.chatCustomColor) {
        m_bgSprite->setColor(look.chatColor);
    }

    // AI lines in the history use the name colour, unless it's plain white
    auto const& name = look.bubble.nameColor;
    if (name.r != 255 || name.g != 255 || name.b != 255) {
        m_aiColorTag = fmt::format("<c-{:02x}{:02x}{:02x}>", name.r, name.g, name.b);
    }

    s_current = this;
    auto& client = AIClient::get();
    this->setTitle(fmt::format("Chat with {}", look.name));

    auto historyBG = NineSlice::create("square02b_001.png");
    historyBG->setColor({ 0, 0, 0 });
    historyBG->setOpacity(90);
    historyBG->setScale(.4f);
    historyBG->setContentSize(CCSize(m_historyWidth + 12.f, m_historyHeight + 12.f) / .4f);
    m_mainLayer->addChildAtPosition(historyBG, Anchor::Center, ccp(0, 12));

    m_history = Label::createRich("", "chatFont.fnt");
    m_history->setAlignment(Label::Alignment::Left);
    m_history->setScale(HISTORY_SCALE);
    m_history->setMaxWidth(m_historyWidth / HISTORY_SCALE);
    m_history->setAnchorPoint({ 0.f, 1.f });
    m_mainLayer->addChildAtPosition(
        m_history, Anchor::Center,
        ccp(-m_historyWidth / 2, 12 + m_historyHeight / 2)
    );

    m_status = CCLabelBMFont::create("", "chatFont.fnt");
    m_status->setScale(.55f);
    m_mainLayer->addChildAtPosition(m_status, Anchor::Bottom, ccp(0, 62));

    m_input = TextInput::create(m_width - 120.f, "Say something...", "chatFont.fnt");
    m_input->setCommonFilter(CommonFilter::Any);
    m_input->setMaxCharCount(150);
    m_enterKey.owner = this;
    m_input->setDelegate(&m_enterKey);
    m_mainLayer->addChildAtPosition(m_input, Anchor::Bottom, ccp(-45, 32));

    auto sendSpr = ButtonSprite::create("Send", "goldFont.fnt", "GJ_button_01.png", .8f);
    sendSpr->setScale(.8f);
    m_sendBtn = CCMenuItemSpriteExtra::create(sendSpr, this, menu_selector(ChatPopup::onSend));
    m_buttonMenu->addChildAtPosition(m_sendBtn, Anchor::Bottom, ccp(m_width / 2 - 55.f, 32));

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
            lines.push_back(fmt::format("{}{}</c>", m_aiColorTag, text));
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
            if (m_history->getScaledContentHeight() <= m_historyHeight || first + 1 >= lines.size()) {
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
    m_status->limitLabelWidth(m_width - 30.f, .55f, .3f);

    m_sendBtn->setEnabled(!waiting);
    m_sendBtn->setOpacity(waiting ? 120 : 255);
}

void ChatPopup::onSend(CCObject*) {
    this->send();
}

void ChatPopup::onConnection(CCObject*) {
    ConnectionPopup::create()->show();
}

void ChatPopup::EnterKeyDelegate::textInputReturn(CCTextInputNode*) {
    owner->send();
}

void ChatPopup::EnterKeyDelegate::enterPressed(CCTextInputNode*) {
    owner->send();
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
