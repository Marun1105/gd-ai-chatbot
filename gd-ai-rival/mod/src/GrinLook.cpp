#include "GrinLook.hpp"

#include <Geode/ui/Notification.hpp>
#include <Geode/utils/file.hpp>
#include <asp/fs.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace geode::prelude;

namespace {
    constexpr auto SETTING = "grin-picture";
    // The setting's default, shown as its label until a file is picked
    constexpr auto NO_PICTURE = "Default 3D Grin";
    // The picked file's path when we last copied it
    constexpr auto SOURCE_KEY = "grin-picture-source";
    // Big photos are shrunk to this on their longer side: plenty for the
    // screen, and a 12 MP photo would otherwise take ~50 MB of video memory
    constexpr int MAX_SIDE = 1024;

    CCTexture2D* s_own = nullptr;
    bool s_loaded = false;

    asp::fs::Path copyPath() {
        return Mod::get()->getSaveDir() / "grin-picture.img";
    }

    // PNG or JPG bytes into a texture, averaged down when it's large
    CCTexture2D* decode(std::vector<uint8_t>& bytes) {
        if (bytes.size() < 8) return nullptr;
        auto image = new CCImage();
        bool ok = image->initWithImageData(bytes.data(), static_cast<int>(bytes.size()));
        int w = ok ? image->getWidth() : 0;
        int h = ok ? image->getHeight() : 0;
        if (!ok || !image->getData() || w <= 0 || h <= 0 || image->getBitsPerComponent() != 8) {
            image->release();
            return nullptr;
        }

        int channels = image->hasAlpha() ? 4 : 3;
        float scale = std::min(1.f, static_cast<float>(MAX_SIDE) / static_cast<float>(std::max(w, h)));
        int ow = std::max(1, static_cast<int>(static_cast<float>(w) * scale + .5f));
        int oh = std::max(1, static_cast<int>(static_cast<float>(h) * scale + .5f));
        std::vector<uint8_t> out(static_cast<size_t>(ow) * static_cast<size_t>(oh) * 4);
        auto data = image->getData();
        for (int y = 0; y < oh; y++) {
            int y0 = y * h / oh;
            int y1 = std::max(y0 + 1, (y + 1) * h / oh);
            for (int x = 0; x < ow; x++) {
                int x0 = x * w / ow;
                int x1 = std::max(x0 + 1, (x + 1) * w / ow);
                uint32_t sum[4] {};
                for (int sy = y0; sy < y1; sy++) {
                    auto pixel = data + (static_cast<size_t>(sy) * static_cast<size_t>(w) + static_cast<size_t>(x0)) * channels;
                    for (int sx = x0; sx < x1; sx++, pixel += channels) {
                        sum[0] += pixel[0];
                        sum[1] += pixel[1];
                        sum[2] += pixel[2];
                        sum[3] += channels == 4 ? pixel[3] : 255u;
                    }
                }
                auto count = static_cast<uint32_t>((y1 - y0) * (x1 - x0));
                auto target = &out[(static_cast<size_t>(y) * static_cast<size_t>(ow) + static_cast<size_t>(x)) * 4];
                for (int c = 0; c < 4; c++) target[c] = static_cast<uint8_t>(sum[c] / count);
            }
        }
        bool premultiplied = channels == 4 && image->isPremultipliedAlpha();
        image->release();

        auto texture = new CCTexture2D();
        if (!texture->initWithData(out.data(), kCCTexture2DPixelFormat_RGBA8888, ow, oh, CCSize(ow, oh))) {
            texture->release();
            return nullptr;
        }
        texture->m_bHasPremultipliedAlpha = premultiplied;
        texture->setAntiAliasTexParameters();
        return texture;
    }

    void complain(std::string const& why) {
        log::warn("Grin picture: {}", why);
        Notification::create("Grin can't use that picture. Pick a PNG or JPG.", NotificationIcon::Error)->show();
    }

    // Picks up the player's picture. We keep our own copy in the mod's save
    // folder, so it still works if the original is moved, and on phones where
    // the picker hands over a temporary file.
    void load(bool changed) {
        s_loaded = true;
        CC_SAFE_RELEASE_NULL(s_own);

        auto mod = Mod::get();
        auto picked = mod->getSettingValue<asp::fs::Path>(SETTING);
        auto copy = copyPath();
        if (picked.empty() || picked.string() == NO_PICTURE) {
            if (asp::fs::exists(copy)) (void)asp::fs::removeFile(copy);
            mod->setSavedValue<std::string>(SOURCE_KEY, "");
            return;
        }

        std::vector<uint8_t> bytes;
        if (!changed && mod->getSavedValue<std::string>(SOURCE_KEY) == picked.string() && asp::fs::exists(copy)) {
            if (auto read = file::readBinary(copy)) bytes = std::move(read).unwrap();
        }
        bool fresh = bytes.empty();
        if (fresh) {
            auto read = file::readBinary(picked);
            if (!read) {
                if (changed) complain("can't read " + picked.string());
                return;
            }
            bytes = std::move(read).unwrap();
        }

        s_own = decode(bytes);
        if (!s_own) {
            if (changed) complain("not a picture we can decode");
            return;
        }
        if (fresh) {
            (void)file::createDirectoryAll(mod->getSaveDir());
            if (file::writeBinary(copy, bytes)) mod->setSavedValue<std::string>(SOURCE_KEY, picked.string());
        }
        if (changed) Notification::create("Grin has a new face.", NotificationIcon::Success)->show();
    }
}

CCTexture2D* grin_look::texture(Pose pose) {
    if (!s_loaded) load(false);
    if (s_own) return s_own;
    auto cache = CCTextureCache::sharedTextureCache();
    switch (pose) {
        case Pose::Face: return cache->addImage("grin-face.png"_spr, false);
        case Pose::Roar: return cache->addImage("grin-roar.png"_spr, false);
        case Pose::Icon: return cache->addImage("grin-icon.png"_spr, false);
        case Pose::Body: return cache->addImage("grin-full.png"_spr, false);
    }
    return nullptr;
}

CCSprite* grin_look::sprite(Pose pose, float size) {
    auto texture = grin_look::texture(pose);
    if (!texture) return nullptr;
    auto sprite = CCSprite::createWithTexture(texture);
    if (!sprite) return nullptr;
    auto content = sprite->getContentSize();
    float longer = std::max(content.width, content.height);
    if (longer > 0.f) sprite->setScale(size / longer);
    return sprite;
}

bool grin_look::usingOwnPicture() {
    if (!s_loaded) load(false);
    return s_own != nullptr;
}

$execute {
    listenForSettingChanges<asp::fs::Path>(SETTING, [](asp::fs::Path) {
        load(true);
    });
}
