#include "GameInfo.hpp"
#include "Text.hpp"

#include <cmath>
#include <fmt/ranges.h>

using namespace geode::prelude;

namespace {
    std::string clean(std::string_view text, size_t max) {
        auto out = text::toDisplayable(text);
        if (out.size() > max) out = out.substr(0, max - 3) + "...";
        return out;
    }

    std::string difficulty(GJGameLevel* level) {
        if (level->m_autoLevel) return "Auto (it plays itself, no input needed)";
        if (level->m_demon.value() > 0) {
            switch (level->m_demonDifficulty) {
                case 3: return "Easy Demon";
                case 4: return "Medium Demon";
                case 5: return "Insane Demon";
                case 6: return "Extreme Demon";
                default: return "Hard Demon";
            }
        }
        if (int stars = level->m_stars.value(); stars > 0) {
            if (stars == 1) return "Auto";
            if (stars == 2) return "Easy";
            if (stars == 3) return "Normal";
            if (stars <= 5) return "Hard";
            if (stars <= 7) return "Harder";
            if (stars <= 9) return "Insane";
            return "Demon";
        }
        // Unrated: GD shows the community's average difficulty vote
        switch (level->getAverageDifficulty()) {
            case 1: return "Easy (by player votes)";
            case 2: return "Normal (by player votes)";
            case 3: return "Hard (by player votes)";
            case 4: return "Harder (by player votes)";
            case 5: return "Insane (by player votes)";
            default: return "no difficulty yet";
        }
    }

    std::string rating(GJGameLevel* level) {
        int stars = level->m_stars.value();
        if (stars <= 0) return "unrated";
        std::string out = fmt::format("rated {} star{}", stars, stars == 1 ? "" : "s");
        switch (level->m_isEpic) {
            case 1: out += ", Epic"; break;
            case 2: out += ", Legendary"; break;
            case 3: out += ", Mythic"; break;
            default: if (level->m_featured > 0) out += ", Featured"; break;
        }
        return out;
    }

    std::string creator(GJGameLevel* level) {
        switch (level->m_levelType) {
            case GJLevelType::Main: return "RobTop (an official level)";
            case GJLevelType::Editor: return "the player themselves (their own unpublished level)";
            default: break;
        }
        auto name = clean(level->m_creatorName, 30);
        return name.empty() ? "an unknown creator" : name;
    }

    std::string length(GJGameLevel* level, bool platformer) {
        if (platformer) return "platformer";
        switch (level->m_levelLength) {
            case 0: return "Tiny";
            case 1: return "Short";
            case 2: return "Medium";
            case 3: return "Long";
            case 4: return "XL";
            default: return "";
        }
    }

    std::string song(GJGameLevel* level) {
        if (level->m_songID > 0) {
            if (auto info = MusicDownloadManager::sharedState()->getSongInfoObject(level->m_songID)) {
                auto name = clean(info->m_songName, 40);
                auto artist = clean(info->m_artistName, 30);
                if (!name.empty()) {
                    return artist.empty() ? fmt::format("\"{}\"", name) : fmt::format("\"{}\" by {}", name, artist);
                }
            }
            return fmt::format("custom song #{}", level->m_songID);
        }
        auto title = clean(LevelTools::getAudioTitle(level->m_audioTrack), 40);
        return title.empty() ? "" : fmt::format("\"{}\" (official GD song)", title);
    }
}

std::string gameinfo::levelFacts(GJGameLevel* level, bool platformer) {
    if (!level) return "";
    auto out = fmt::format(
        "\"{}\" by {} | {}, {}",
        clean(level->m_levelName, 40), creator(level), difficulty(level), rating(level)
    );
    if (auto len = length(level, platformer); !len.empty()) out += " | " + len;
    if (auto music = song(level); !music.empty()) out += " | song: " + music;
    if (int objects = level->m_objectCount.value(); objects > 0) out += fmt::format(" | {} objects", objects);
    if (int original = level->m_originalLevel.value(); original > 0) {
        out += fmt::format(" | a copy of level {}", original);
    }
    return out;
}

std::string gameinfo::levelAbout(GJGameLevel* level, bool platformer) {
    if (!level) return "";
    std::vector<std::string> lines;
    lines.push_back(fmt::format("Name: \"{}\"", clean(level->m_levelName, 40)));
    lines.push_back("Creator: " + creator(level));
    lines.push_back(fmt::format("Difficulty: {} ({})", difficulty(level), rating(level)));
    if (auto len = length(level, platformer); !len.empty()) lines.push_back("Length: " + len);
    if (auto music = song(level); !music.empty()) lines.push_back("Song: " + music);
    if (int objects = level->m_objectCount.value(); objects > 0) lines.push_back(fmt::format("Objects: {}", objects));
    if (level->m_levelType != GJLevelType::Editor && level->m_levelType != GJLevelType::Main) {
        lines.push_back(fmt::format("Online: {} downloads, {} likes", level->m_downloads, level->m_likes));
    }
    if (int original = level->m_originalLevel.value(); original > 0) {
        lines.push_back(fmt::format("Copied from level ID {} (so it may be a copy or remake of another level)", original));
    }
    if (level->m_twoPlayerMode) lines.push_back("Two-player level");
    auto description = clean(LevelTools::base64DecodeString(level->m_levelDesc), 180);
    if (!description.empty()) lines.push_back(fmt::format("Creator's description: \"{}\"", description));
    return fmt::format("{}", fmt::join(lines, "\n"));
}

std::string gameinfo::situation(PlayerObject* player, bool dual) {
    if (!player) return "";

    std::string mode = "cube";
    if (player->m_isShip) mode = "ship";
    else if (player->m_isBird) mode = "UFO";
    else if (player->m_isBall) mode = "ball";
    else if (player->m_isDart) mode = "wave";
    else if (player->m_isRobot) mode = "robot";
    else if (player->m_isSpider) mode = "spider";
    else if (player->m_isSwing) mode = "swing";

    std::string out = "in the ";
    if (player->m_vehicleSize < .9f) out += "mini ";
    out += mode;

    // GD's internal speed values for the 0.5x / 1x / 2x / 3x / 4x portals
    constexpr std::pair<float, char const*> SPEEDS[] = {
        { .7f, "0.5x speed" }, { .9f, "normal speed" }, { 1.1f, "2x speed" }, { 1.3f, "3x speed" }, { 1.6f, "4x speed" },
    };
    auto nearest = SPEEDS[1];
    for (auto const& speed : SPEEDS) {
        if (std::abs(speed.first - player->m_playerSpeed) < std::abs(nearest.first - player->m_playerSpeed)) {
            nearest = speed;
        }
    }
    out += fmt::format(", {}", nearest.second);
    if (player->m_isUpsideDown) out += ", upside down";
    if (dual) out += ", dual mode";
    return out;
}
