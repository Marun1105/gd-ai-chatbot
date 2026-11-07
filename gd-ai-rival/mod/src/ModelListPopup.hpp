#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>

#include "Providers.hpp"

#include <functional>

// Fetches the provider's live model list and lets the player tap one, so
// model names that change over time never leave the mod stuck.
class ModelListPopup : public geode::Popup {
public:
    using Callback = std::function<void(std::string const&)>;
    static ModelListPopup* create(Connection connection, Callback onPick);

protected:
    bool init(Connection connection, Callback onPick);
    void showModels(std::vector<std::string> models);
    void showError(std::string const& message);
    void onPick(cocos2d::CCObject* sender);

    Connection m_connection;
    Callback m_onPick;
    std::vector<std::string> m_models;
    geode::LoadingSpinner* m_spinner = nullptr;
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_request;
};
