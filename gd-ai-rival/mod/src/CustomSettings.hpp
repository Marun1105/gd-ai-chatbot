#pragma once

#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>

#include "Avatar.hpp"
#include "Look.hpp"

// Two setting types of our own for the mod's settings page:
//
// custom:avatar     the whole avatar in one row, with a live preview and an
//                   "Edit" button that opens the avatar editor
// custom:long-text  text too long for a one-line box (the custom personality
//                   and the reaction directions); "Edit" opens a big editor
// custom:bubble     the speech bubble's look and voice; opens the designer
// custom:chat-look  the chat window's theme, color and size

class AvatarSetting : public geode::SettingBaseValueV3<AvatarLook> {
public:
    static geode::Result<std::shared_ptr<geode::SettingV3>> parse(
        std::string key, std::string modID, matjson::Value const& json
    );
    geode::SettingNodeV3* createNode(float width) override;
};

class LongTextSetting : public geode::SettingBaseValueV3<std::string> {
public:
    static geode::Result<std::shared_ptr<geode::SettingV3>> parse(
        std::string key, std::string modID, matjson::Value const& json
    );
    geode::SettingNodeV3* createNode(float width) override;
    geode::Result<> isValid(std::string value) const override;

    size_t getMaxLength() const { return m_maxLength; }
    std::string const& getPlaceholder() const { return m_placeholder; }
    std::string const& getHint() const { return m_hint; }
    std::vector<std::string> const& getExamples() const { return m_examples; }

protected:
    size_t m_maxLength = 200;
    std::string m_placeholder;
    // Plain-text line shown in the editor (the description has color tags)
    std::string m_hint;
    std::vector<std::string> m_examples;
};

class BubbleSetting : public geode::SettingBaseValueV3<BubbleLook> {
public:
    static geode::Result<std::shared_ptr<geode::SettingV3>> parse(
        std::string key, std::string modID, matjson::Value const& json
    );
    geode::SettingNodeV3* createNode(float width) override;
};

class ChatLookSetting : public geode::SettingBaseValueV3<ChatLook> {
public:
    static geode::Result<std::shared_ptr<geode::SettingV3>> parse(
        std::string key, std::string modID, matjson::Value const& json
    );
    geode::SettingNodeV3* createNode(float width) override;
};

template <>
struct geode::SettingTypeForValueType<AvatarLook> {
    using SettingType = AvatarSetting;
};

template <>
struct geode::SettingTypeForValueType<BubbleLook> {
    using SettingType = BubbleSetting;
};

template <>
struct geode::SettingTypeForValueType<ChatLook> {
    using SettingType = ChatLookSetting;
};

namespace settings {
    // Value of a custom:long-text setting ("" if it doesn't exist)
    std::string longText(std::string_view key);
    AvatarLook avatar();
    BubbleLook bubble();
    ChatLook chatLook();
}
