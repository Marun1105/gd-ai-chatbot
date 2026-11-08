#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "AIClient.hpp"
#include "ChatPopup.hpp"
#include "ConnectionPopup.hpp"
#include "GameInfo.hpp"
#include "Rage.hpp"
#include "RageBar.hpp"
#include "RageEvent.hpp"

using namespace geode::prelude;

namespace {
    bool isLevelAllowed(int levelID) {
        if (!Mod::get()->getSettingValue<bool>("level-lock")) return true;

        // Any non-digit separates IDs, so "1, 2 3" and "1,2,3" both work
        auto list = Mod::get()->getSettingValue<std::string>("level-ids");
        int64_t current = 0;
        bool inNumber = false;
        for (size_t i = 0; i <= list.size(); i++) {
            char c = i < list.size() ? list[i] : ',';
            if (c >= '0' && c <= '9') {
                if (current < 1'000'000'000'000) current = current * 10 + (c - '0');
                inNumber = true;
            }
            else {
                if (inNumber && current == levelID) return true;
                current = 0;
                inNumber = false;
            }
        }
        return false;
    }

    bool openChat() {
        auto playLayer = PlayLayer::get();
        if (!playLayer || !AIClient::get().isActive() || ChatPopup::isOpen()) return false;

        // Typing while the level keeps running would just get you killed
        if (!playLayer->m_isPaused && !playLayer->m_hasCompletedLevel) {
            playLayer->pauseGame(false);
        }
        ChatPopup::create()->show();
        return true;
    }
}

$on_mod(Loaded) {
    // "Set Up" button on the mod's settings page
    ButtonSettingPressedEventV3(Mod::get(), "connection").listen([](std::string_view) {
        ConnectionPopup::create()->show();
    }).leak();

#ifdef GEODE_IS_DESKTOP
    listenForKeybindSettingPresses("open-chat", [](Keybind const&, bool down, bool repeat, double) {
        if (!down || repeat) return false;
        return openChat();
    });
#endif
}

class $modify(AIPlayLayer, PlayLayer) {
    struct Fields {
        bool active = false;
        // destroyPlayer can fire more than once per death (dual mode)
        bool deathReported = false;
        RageBar* rageBar = nullptr;
        // GD tried to restart while Grin was smashing the screen
        bool resetPending = false;
    };

    bool init(GJGameLevel* level, bool dontCreateObjects) {
        if (!PlayLayer::init(level, dontCreateObjects)) return false;

        auto& client = AIClient::get();
        if (!isLevelAllowed(level->m_levelID.value())) {
            client.endLevel();
            return true;
        }

        m_fields->active = true;
        client.startLevel({
            .name = std::string(level->m_levelName),
            .id = level->m_levelID.value(),
            .bestPercent = level->m_normalPercent.value(),
            .totalAttempts = level->m_attempts.value(),
            .platformer = m_isPlatformer,
            .facts = gameinfo::levelFacts(level, m_isPlatformer),
            .about = gameinfo::levelAbout(level, m_isPlatformer),
        });
        client.onLevelStart();
        this->addChatButton();
        rage::startLevel(level, m_isPlatformer);
        this->addRageBar();
        return true;
    }

    void addRageBar() {
        // Grin's anger bar, top-left. Created even when hidden: it also lets
        // the anger cool down while you play.
        if (!rage::active() || !m_uiLayer) return;
        auto bar = RageBar::create();
        auto winSize = CCDirector::get()->getWinSize();
        bar->setPosition({ 8.f, winSize.height - 6.f });
        bar->setVisible(Mod::get()->getSettingValue<bool>("rage-bar"));
        m_uiLayer->addChild(bar, 10);
        m_fields->rageBar = bar;
    }

    void rageFinished() {
        rage::afterSnap();
        if (m_fields->rageBar) m_fields->rageBar->refresh();
        if (m_fields->resetPending) {
            m_fields->resetPending = false;
            this->resetLevel();
        }
        AIClient::get().onRage();
    }

    void addChatButton() {
        if (!Mod::get()->getSettingValue<bool>("show-ingame-button") || !m_uiLayer) return;

        auto sprite = CCSprite::createWithSpriteFrameName("GJ_chatBtn_001.png");
        sprite->setScale(.5f);
        sprite->setOpacity(150);
        auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(AIPlayLayer::onChatButton));
        button->setID("chat-button"_spr);

        auto menu = CCMenu::create();
        menu->setID("chat-button-menu"_spr);
        menu->addChild(button);
        button->setPosition({ 0.f, 0.f });

        // Sit just left of the pause button
        auto winSize = CCDirector::get()->getWinSize();
        CCPoint position { winSize.width - 60.f, winSize.height - 20.f };
        if (auto pauseButton = m_uiLayer->m_pauseBtn) {
            position = m_uiLayer->convertToNodeSpace(pauseButton->convertToWorldSpaceAR({ 0.f, 0.f }));
            position.x -= 34.f;
        }
        menu->setPosition(position);
        m_uiLayer->addChild(menu, 10);
    }

    void onChatButton(CCObject*) {
        openChat();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);

        // GD calls this with a dummy spike when the level starts; also skip noclip
        if (!m_fields->active || object == m_anticheatSpike) return;
        if (!player || !player->m_isDead || m_fields->deathReported) return;

        m_fields->deathReported = true;
        int percent = m_isPlatformer ? 0 : this->getCurrentPercentInt();
        auto situation = gameinfo::situation(player, m_gameState.m_isDualMode);

        bool snapped = false;
        if (rage::active()) {
            auto result = rage::onDeath(percent, m_attempts, m_isPracticeMode, m_isTestMode);
            if (m_fields->rageBar) m_fields->rageBar->refresh(result.gained > 0.f);
            snapped = result.snapped;
        }

        // The screen break gets its own comment once it's over
        AIClient::get().onDeath(percent, m_attempts, m_isPracticeMode || m_isTestMode, situation, snapped);
        if (snapped) {
            RageEvent::play([] {
                if (auto playLayer = static_cast<AIPlayLayer*>(PlayLayer::get())) playLayer->rageFinished();
            });
        }
    }

    void resetLevel() {
        // Hold the restart while Grin smashes the screen, or the next attempt
        // would start (and probably end) behind the dark overlay
        if (RageEvent::isActive()) {
            m_fields->resetPending = true;
            return;
        }
        PlayLayer::resetLevel();
        m_fields->deathReported = false;
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        if (m_fields->active && rage::active()) {
            rage::onComplete();
            if (m_fields->rageBar) m_fields->rageBar->refresh();
        }
        if (m_fields->active) {
            AIClient::get().onComplete(m_attempts, m_isPracticeMode || m_isTestMode);
        }
    }

    void onQuit() {
        if (m_fields->active) {
            AIClient::get().endLevel();
            rage::endLevel();
        }
        PlayLayer::onQuit();
    }
};

class $modify(AIPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        if (!AIClient::get().isActive()) return;

        auto sprite = CCSprite::createWithSpriteFrameName("GJ_chatBtn_001.png");
        sprite->setScale(.65f);
        auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(AIPauseLayer::onAIChat));
        button->setID("chat-button"_spr);

        // If the Node IDs mod is installed, share its empty column on the right
        // with other mods; otherwise put our own button in that same free spot
        if (auto menu = this->getChildByIDRecursive("right-button-menu")) {
            menu->addChild(button);
            menu->updateLayout();
            return;
        }

        auto winSize = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();
        menu->setID("chat-menu"_spr);
        menu->setPosition({ winSize.width - 36.f, winSize.height / 2 });
        menu->addChild(button);
        button->setPosition({ 0.f, 0.f });
        this->addChild(menu, 10);
    }

    void onAIChat(CCObject*) {
        if (!ChatPopup::isOpen()) {
            ChatPopup::create()->show();
        }
    }
};
