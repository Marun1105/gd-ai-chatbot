#include "Providers.hpp"
#include "Text.hpp"

#include <algorithm>
#include <array>

using namespace geode::prelude;

namespace {
    constexpr auto ANTHROPIC_VERSION = "2026-10-21";
    // If the model declines, the API retries on Anthropic's recommended fallback model
    constexpr auto ANTHROPIC_FALLBACK_BETA = "server-side-fallback-2026-07-01";
    constexpr auto REFUSAL_LINE = "Let's keep our eyes on the level.";
    // Room for reasoning tokens on models that think before answering
    constexpr int MAX_OUTPUT_TOKENS = 2048;

    constexpr auto PROVIDERS = std::to_array<Provider>({
        {
            .id = "anthropic", .name = "Claude (Anthropic)", .format = ApiFormat::Anthropic,
            .defaultUrl = "https://api.anthropic.com", .defaultModel = "claude-haiku-5-5",
            .keyUrl = "https://console.anthropic.com/settings/keys",
            .note = "Paid, pay as you go: a few cents per thousand messages on Haiku 5.5. Create a key in the Claude Console.",
            .tokenParam = "", .needsKey = true, .urlEditable = false, .free = false,
        },
        {
            .id = "openai", .name = "ChatGPT (OpenAI)", .format = ApiFormat::OpenAI,
            .defaultUrl = "https://api.openai.com/v1", .defaultModel = "gpt-5.6",
            .keyUrl = "https://platform.openai.com/api-keys",
            .note = "Paid, pay as you go. Add billing on the OpenAI platform, then create a key.",
            .tokenParam = "max_completion_tokens", .needsKey = true, .urlEditable = false, .free = false,
        },
        {
            .id = "gemini", .name = "Gemini (Google)", .format = ApiFormat::OpenAI,
            .defaultUrl = "https://generativelanguage.googleapis.com/v1beta/openai", .defaultModel = "gemini-flash-latest",
            .keyUrl = "https://aistudio.google.com/apikey",
            .note = "FREE tier with a Google account, no card needed (rate limited). Create a key in Google AI Studio.",
            .tokenParam = "max_tokens", .needsKey = true, .urlEditable = false, .free = true,
        },
        {
            .id = "xai", .name = "Grok (xAI)", .format = ApiFormat::OpenAI,
            .defaultUrl = "https://api.x.ai/v1", .defaultModel = "grok-4.5",
            .keyUrl = "https://console.x.ai",
            .note = "Paid. New accounts sometimes get trial credits. Create a key in the xAI console.",
            .tokenParam = "max_tokens", .needsKey = true, .urlEditable = false, .free = false,
        },
        {
            .id = "groq", .name = "Groq", .format = ApiFormat::OpenAI,
            .defaultUrl = "https://api.groq.com/openai/v1", .defaultModel = "llama-3.3-70b-versatile",
            .keyUrl = "https://console.groq.com/keys",
            .note = "FREE tier, no card needed. Very fast open models like Llama (not the same as Grok).",
            .tokenParam = "max_completion_tokens", .needsKey = true, .urlEditable = false, .free = true,
        },
        {
            .id = "openrouter", .name = "OpenRouter", .format = ApiFormat::OpenAI,
            .defaultUrl = "https://openrouter.ai/api/v1", .defaultModel = "openrouter/free",
            .keyUrl = "https://openrouter.ai/keys",
            .note = "One key for hundreds of models. The default model \"openrouter/free\" only uses free ones.",
            .tokenParam = "max_tokens", .needsKey = true, .urlEditable = false, .free = true,
        },
        {
            .id = "ollama", .name = "Ollama (on this PC)", .format = ApiFormat::OpenAI,
            .defaultUrl = "http://localhost:11434/v1", .defaultModel = "llama3.2",
            .keyUrl = "https://ollama.com/download",
            .note = "FREE and offline, runs on your own PC (not phones). Install Ollama, then run: ollama pull llama3.2",
            .tokenParam = "max_tokens", .needsKey = false, .urlEditable = true, .free = true,
        },
        {
            .id = "custom", .name = "Custom (OpenAI-compatible)", .format = ApiFormat::OpenAI,
            .defaultUrl = "", .defaultModel = "",
            .keyUrl = "",
            .note = "Any OpenAI-compatible API, like DeepSeek or LM Studio. Enter its base URL, usually ending in /v1.",
            .tokenParam = "max_tokens", .needsKey = false, .urlEditable = true, .free = false,
        },
        {
            .id = "proxy", .name = "Creator proxy", .format = ApiFormat::Proxy,
            .defaultUrl = "", .defaultModel = "",
            .keyUrl = "",
            .note = "A server run by a level creator, who pays for the AI. Paste the URL they shared (it ends in /chat).",
            .tokenParam = "", .needsKey = false, .urlEditable = true, .free = true,
        },
    });

    std::string saved(std::string_view kind, Provider const& provider) {
        return Mod::get()->getSavedValue<std::string>(fmt::format("{}.{}", kind, provider.id));
    }

    void save(std::string_view kind, Provider const& provider, std::string const& value) {
        Mod::get()->setSavedValue(fmt::format("{}.{}", kind, provider.id), utils::string::trim(value));
    }

    web::WebRequest baseRequest() {
        web::WebRequest req;
        req.timeout(std::chrono::seconds(30));
        return req;
    }

    web::WebRequest anthropicRequest(Connection const& connection) {
        auto req = baseRequest();
        req.header("x-api-key", connection.key);
        req.header("anthropic-version", ANTHROPIC_VERSION);
        req.header("anthropic-beta", ANTHROPIC_FALLBACK_BETA);
        return req;
    }

    web::WebRequest openAIRequest(Connection const& connection) {
        auto req = baseRequest();
        if (!connection.key.empty()) {
            req.header("Authorization", "Bearer " + connection.key);
        }
        return req;
    }

    matjson::Value conversation(ChatPayload const& payload, bool withSystem) {
        auto messages = matjson::Value::array();
        if (withSystem) {
            messages.push(matjson::makeObject({ { "role", "system" }, { "content", payload.system } }));
        }
        for (auto const& [fromAI, text] : payload.history) {
            messages.push(matjson::makeObject({ { "role", fromAI ? "assistant" : "user" }, { "content", text } }));
        }
        messages.push(matjson::makeObject({ { "role", "user" }, { "content", payload.message } }));
        return messages;
    }

    // Error text from any of the formats: {"error": {"message"}}, {"message"},
    // {"error": "..."}; Gemini sometimes wraps the whole thing in a list
    std::string providerMessage(web::WebResponse const& response) {
        auto json = response.json();
        if (!json) return "";
        auto const value = json.unwrap();
        auto const& root = value.isArray() ? value[0] : value;
        if (root["error"].isObject()) {
            return root["error"]["message"].asString().unwrapOr("");
        }
        if (root["message"].isString()) {
            return root["message"].asString().unwrapOr("");
        }
        return root["error"].asString().unwrapOr("");
    }

    std::string describeError(Connection const& connection, web::WebResponse const& response, bool listing) {
        auto const& provider = *connection.provider;
        auto message = text::toDisplayable(providerMessage(response));
        if (message.size() > 140) {
            message = message.substr(0, 137) + "...";
        }
        int code = response.code();

        if (response.error() || code == 0) {
            if (provider.id == "ollama") {
                return "Couldn't reach Ollama. Is it installed and running on this PC?";
            }
            return fmt::format("Couldn't reach {}. Check the URL and your internet connection.", provider.name);
        }
        if (provider.format == ApiFormat::Proxy) {
            if (code == 404) return "Proxy not found. Check the URL; it should end in /chat.";
            return message.empty() ? fmt::format("The proxy returned an error (HTTP {}).", code) : message;
        }
        if (code == 401 || code == 403) {
            return fmt::format("{} rejected the API key. Check it in AI Connection.", provider.name);
        }
        if (code == 402) {
            return fmt::format("You're out of credits on {}.", provider.name);
        }
        if (code == 404) {
            return listing
                ? fmt::format("{} can't list models. Type the model name instead.", provider.name)
                : fmt::format("{} doesn't know the model \"{}\". Pick another in AI Connection.", provider.name, connection.model);
        }
        if (code == 429 && message.empty()) {
            return fmt::format("{}: too many requests, or you're out of credits.", provider.name);
        }
        return message.empty()
            ? fmt::format("{} returned an error (HTTP {}).", provider.name, code)
            : fmt::format("{}: {}", provider.name, message);
    }

    bool isChatModel(std::string_view id) {
        // Model lists also contain embedding, speech, image and moderation models
        constexpr std::array<std::string_view, 14> NOT_CHAT {
            "embed", "tts", "whisper", "dall-e", "image", "audio", "moderation",
            "transcribe", "realtime", "imagen", "veo", "aqa", "guard", "sora",
        };
        return std::none_of(NOT_CHAT.begin(), NOT_CHAT.end(), [&](auto word) {
            return id.find(word) != std::string_view::npos;
        });
    }
}

std::string Connection::missing() const {
    if (provider->needsKey && key.empty()) return "an API key";
    if (url.empty()) return provider->format == ApiFormat::Proxy ? "the proxy URL" : "the API URL";
    if (provider->format != ApiFormat::Proxy && model.empty()) return "a model";
    return "";
}

std::span<Provider const> providers::all() {
    return PROVIDERS;
}

Provider const& providers::byId(std::string_view id) {
    for (auto const& provider : PROVIDERS) {
        if (provider.id == id) return provider;
    }
    return PROVIDERS.front();
}

Provider const& providers::current() {
    return byId(Mod::get()->getSavedValue<std::string>("provider"));
}

void providers::setCurrent(Provider const& provider) {
    Mod::get()->setSavedValue("provider", std::string(provider.id));
}

std::string providers::savedKey(Provider const& provider) { return saved("key", provider); }
std::string providers::savedModel(Provider const& provider) { return saved("model", provider); }
std::string providers::savedUrl(Provider const& provider) { return saved("url", provider); }
void providers::saveKey(Provider const& provider, std::string const& key) { save("key", provider, key); }
void providers::saveModel(Provider const& provider, std::string const& model) { save("model", provider, model); }
void providers::saveUrl(Provider const& provider, std::string const& url) { save("url", provider, url); }

Connection providers::load(Provider const& provider) {
    auto model = savedModel(provider);
    auto url = savedUrl(provider);
    if (model.empty()) model = provider.defaultModel;
    if (url.empty()) url = provider.defaultUrl;
    while (!url.empty() && url.back() == '/') url.pop_back();
    return { &provider, savedKey(provider), std::move(model), std::move(url) };
}

Connection providers::loadCurrent() {
    return load(current());
}

web::WebFuture providers::sendChat(Connection const& connection, ChatPayload const& payload) {
    switch (connection.provider->format) {
        case ApiFormat::Anthropic: {
            auto body = matjson::makeObject({
                { "model", connection.model },
                { "max_tokens", 1024 },
                { "system", payload.system },
                { "messages", conversation(payload, false) },
                // Short banter: keep thinking brief so replies land while the moment lasts
                { "output_config", matjson::makeObject({ { "effort", "low" } }) },
                { "fallbacks", "default" },
            });
            return anthropicRequest(connection).bodyJSON(body).post(connection.url + "/v1/messages");
        }
        case ApiFormat::OpenAI: {
            auto body = matjson::makeObject({
                { "model", connection.model },
                { "messages", conversation(payload, true) },
            });
            if (!connection.provider->tokenParam.empty()) {
                body[connection.provider->tokenParam] = MAX_OUTPUT_TOKENS;
            }
            return openAIRequest(connection).bodyJSON(body).post(connection.url + "/chat/completions");
        }
        case ApiFormat::Proxy:
        default: {
            auto history = matjson::Value::array();
            for (auto const& [fromAI, text] : payload.history) {
                history.push(matjson::makeObject({ { "role", fromAI ? "assistant" : "user" }, { "content", text } }));
            }
            auto body = matjson::makeObject({
                { "player_id", payload.playerID },
                { "level_id", payload.levelID },
                { "persona", payload.persona },
                { "level", payload.levelAbout },
                { "powers", payload.powers },
                { "history", std::move(history) },
                { "message", payload.message },
            });
            return baseRequest().bodyJSON(body).post(connection.url);
        }
    }
}

Result<std::string, std::string> providers::parseChat(Connection const& connection, web::WebResponse const& response, size_t maxChars, std::string* commands) {
    if (!response.ok()) {
        return Err(describeError(connection, response, false));
    }
    auto json = response.json();
    if (!json) {
        return Err(fmt::format("{} sent a reply the mod couldn't read.", connection.provider->name));
    }
    auto const value = json.unwrap();

    std::string raw;
    switch (connection.provider->format) {
        case ApiFormat::Anthropic: {
            if (value["stop_reason"].asString().unwrapOr("") == "refusal") {
                return Ok(std::string(REFUSAL_LINE));
            }
            if (value["content"].isArray()) {
                for (auto const& block : value["content"]) {
                    if (block["type"].asString().unwrapOr("") == "text") {
                        raw += block["text"].asString().unwrapOr("") + " ";
                    }
                }
            }
        } break;

        case ApiFormat::OpenAI: {
            auto const& message = value["choices"][0]["message"];
            raw = message["content"].asString().unwrapOr("");
            if (raw.empty() && message["refusal"].isString()) {
                return Ok(std::string(REFUSAL_LINE));
            }
        } break;

        case ApiFormat::Proxy: {
            raw = value["reply"].asString().unwrapOr("");
        } break;
    }

    // Commands come off first, so trimming the spoken line can't cut them
    auto [spoken, found] = text::splitCommands(raw);
    if (commands) *commands = text::toDisplayable(found);
    auto reply = text::cleanReply(spoken, maxChars);
    if (reply.empty() && !found.empty()) reply = "...";
    if (reply.empty()) {
        return Err(std::string("The AI had nothing to say. Try again."));
    }
    return Ok(reply);
}

bool providers::canListModels(Connection const& connection) {
    return connection.provider->format != ApiFormat::Proxy && !connection.url.empty() &&
        (!connection.provider->needsKey || !connection.key.empty());
}

web::WebFuture providers::listModels(Connection const& connection) {
    if (connection.provider->format == ApiFormat::Anthropic) {
        auto req = anthropicRequest(connection);
        req.param("limit", "100");
        return req.get(connection.url + "/v1/models");
    }
    return openAIRequest(connection).get(connection.url + "/models");
}

Result<std::vector<std::string>, std::string> providers::parseModels(Connection const& connection, web::WebResponse const& response) {
    if (!response.ok()) {
        return Err(describeError(connection, response, true));
    }
    auto json = response.json();
    if (!json || !json.unwrap()["data"].isArray()) {
        return Err(fmt::format("{} sent a model list the mod couldn't read.", connection.provider->name));
    }

    std::vector<std::string> models;
    for (auto const& entry : json.unwrap()["data"]) {
        auto id = entry["id"].asString().unwrapOr("");
        // Gemini lists "models/gemini-..." but chat expects the bare name
        if (id.starts_with("models/")) id = id.substr(7);
        if (!id.empty() && isChatModel(id)) models.push_back(std::move(id));
    }
    std::sort(models.begin(), models.end());
    models.erase(std::unique(models.begin(), models.end()), models.end());

    if (models.empty()) {
        return Err(fmt::format("{} didn't list any chat models.", connection.provider->name));
    }
    return Ok(std::move(models));
}

providers::ModelBadge providers::modelBadge(std::string_view model) {
    auto name = utils::string::toLower(std::string(model));
    auto has = [&](std::string_view part) { return name.find(part) != std::string::npos; };

    if (has(":free") || name == "openrouter/free") return ModelBadge::Free;
    // Fast and cheap: plenty for short in-game replies
    if (has("haiku") || has("flash") || has("-mini") || has("-nano") || has("instant")
        || (has("grok") && has("fast"))) {
        return ModelBadge::Recommended;
    }
    if (has("sonnet")) return ModelBadge::Balanced;
    if (has("opus") || has("fable") || has("mythos") || has("-pro")) return ModelBadge::Smartest;
    return ModelBadge::None;
}
