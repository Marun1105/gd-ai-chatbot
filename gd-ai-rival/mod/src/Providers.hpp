#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

// Every AI service the mod can talk to. Most speak the OpenAI-compatible
// chat format; Claude has its own; a creator proxy takes our own format and
// holds the key server-side. Adding a provider is one entry in the table.
enum class ApiFormat { Anthropic, OpenAI, Proxy };

struct Provider {
    std::string_view id;
    std::string_view name;
    ApiFormat format;
    // Base URL (ending in /v1 for OpenAI-style APIs), or the full /chat URL for a proxy
    std::string_view defaultUrl;
    std::string_view defaultModel;
    // Where players create a key (or download the app, for Ollama)
    std::string_view keyUrl;
    // One line about cost and setup, shown in the connection popup
    std::string_view note;
    // Request field that caps output length, empty to send none
    std::string_view tokenParam;
    bool needsKey;
    bool urlEditable;
    bool free;
};

// A provider plus the key, model and URL the player saved for it
struct Connection {
    Provider const* provider;
    std::string key;
    std::string model;
    std::string url;

    // Empty when ready to use, otherwise what's missing
    std::string missing() const;
};

struct ChatPayload {
    // Full system prompt, used by direct providers
    std::string system;
    // Character description only; a proxy adds its own rules around it
    std::string persona;
    // (fromAI, text) pairs, oldest first
    std::vector<std::pair<bool, std::string>> history;
    std::string message;
    // Facts about the current level, for a proxy to put in its own system prompt
    std::string levelAbout;
    std::string playerID;
    int levelID = 0;
};

namespace providers {
    std::span<Provider const> all();
    Provider const& byId(std::string_view id);

    Provider const& current();
    void setCurrent(Provider const& provider);

    Connection load(Provider const& provider);
    Connection loadCurrent();

    // Raw saved values, empty when the player hasn't set one
    std::string savedKey(Provider const& provider);
    std::string savedModel(Provider const& provider);
    std::string savedUrl(Provider const& provider);
    void saveKey(Provider const& provider, std::string const& key);
    void saveModel(Provider const& provider, std::string const& model);
    void saveUrl(Provider const& provider, std::string const& url);

    geode::utils::web::WebFuture sendChat(Connection const& connection, ChatPayload const& payload);
    // The cleaned reply, or a short message explaining what went wrong
    geode::Result<std::string, std::string> parseChat(
        Connection const& connection, geode::utils::web::WebResponse const& response, size_t maxChars = 220
    );

    // A label for the model list, guessed from the model's name
    enum class ModelBadge { None, Recommended, Free, Balanced, Smartest };
    ModelBadge modelBadge(std::string_view model);

    bool canListModels(Connection const& connection);
    geode::utils::web::WebFuture listModels(Connection const& connection);
    geode::Result<std::vector<std::string>, std::string> parseModels(
        Connection const& connection, geode::utils::web::WebResponse const& response
    );
}
