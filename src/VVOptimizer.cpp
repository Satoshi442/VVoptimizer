
#include <android/asset_manager.h>

#include <pl/Mod.hpp>
#include <pl/memory/Hook.hpp>

#include <dlfcn.h>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

using OpenFn = AAsset* (*)(AAssetManager*, const char*, int);
using GetBufferFn = const void* (*)(AAsset*);
using ReadFn = int (*)(AAsset*, void*, size_t);
using CloseFn = void (*)(AAsset*);

constexpr std::string_view kTargetAsset =
    "renderer/platform_config/android/platform_configuration.android.json";

bool contains(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string_view::npos;
}

void replaceAll(std::string& s, std::string_view from, std::string_view to) {
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

bool patchPlatformConfig(void* data, size_t size) {
    if (!data || size == 0 || size > 4 * 1024 * 1024) {
        return false;
    }

    std::string text(reinterpret_cast<const char*>(data), size);

    if (!contains(text, "\"android\"") ||
        !contains(text, "\"tiers\"") ||
        !contains(text, "\"target_resolution\"")) {
        return false;
    }

    const std::string before = text;

    // Force the tier-5 profiles down to the low-cost settings that the
    // supplied Android renderer configuration already defines.
    replaceAll(text, "\"default_deferred_distance\": 6",
               "\"default_deferred_distance\": 4");
    replaceAll(text, "\"default_deferred_distance\": 8",
               "\"default_deferred_distance\": 4");

    replaceAll(text, "\"reflections\": \"medium\"",
               "\"reflections\": \"low\"");
    replaceAll(text, "\"reflections\": \"high\"",
               "\"reflections\": \"low\"");

    replaceAll(text, "\"shadows\": \"medium\"",
               "\"shadows\": \"low\"");
    replaceAll(text, "\"shadows\": \"high\"",
               "\"shadows\": \"low\"");

    replaceAll(text, "\"target_resolution\": \"720p\"",
               "\"target_resolution\": \"480p\"");
    replaceAll(text, "\"upscaling_mode\": \"taau\"",
               "\"upscaling_mode\": \"bilinear\"");

    replaceAll(text, "\"bloom\": true", "\"bloom\": false");

    if (text == before) {
        return false;
    }

    // The replacements above never increase the JSON length. Pad the tail
    // with spaces so AAsset_read/getBuffer callers still see the original
    // asset length.
    std::memset(data, ' ', size);
    std::memcpy(data, text.data(), text.size());
    return true;
}

class VVOptimizer {
public:
    static VVOptimizer& instance() {
        static VVOptimizer mod;
        return mod;
    }

    VVOptimizer() : self_(*ll::mod::NativeMod::current()) {}

    ll::mod::NativeMod& self() { return self_; }

    bool load() {
        self_.getLogger().info("VV Optimizer loading");
        return true;
    }

    bool enable() {
        if (!installHooks()) {
            self_.getLogger().error("VV Optimizer: hook installation failed");
            return false;
        }

        self_.getLogger().info(
            "VV Optimizer enabled: tier-5 Android VV configuration will use "
            "low-cost renderer settings");
        return true;
    }

    bool disable() {
        closeHook_.reset();
        readHook_.reset();
        getBufferHook_.reset();
        openHook_.reset();

        std::lock_guard lock(assetMutex_);
        targetAssets_.clear();
        patchedBuffers_.clear();

        self_.getLogger().info("VV Optimizer disabled");
        return true;
    }

    bool unload() {
        // Hooks are already removed by disable().
        return true;
    }

private:
    bool installHooks() {
        void* openTarget = dlsym(RTLD_DEFAULT, "AAssetManager_open");
        void* getBufferTarget = dlsym(RTLD_DEFAULT, "AAsset_getBuffer");
        void* readTarget = dlsym(RTLD_DEFAULT, "AAsset_read");
        void* closeTarget = dlsym(RTLD_DEFAULT, "AAsset_close");

        if (!openTarget || !getBufferTarget || !readTarget || !closeTarget) {
            self_.getLogger().error(
                "VV Optimizer: Android Asset API symbols missing "
                "(open={}, getBuffer={}, read={}, close={})",
                openTarget != nullptr, getBufferTarget != nullptr,
                readTarget != nullptr, closeTarget != nullptr);
            return false;
        }

        openHook_ = pl::memory::HookHandle(
            openTarget,
            reinterpret_cast<void*>(&onOpen),
            reinterpret_cast<void**>(&originalOpen_));

        if (!openHook_.installed()) {
            return false;
        }

        getBufferHook_ = pl::memory::HookHandle(
            getBufferTarget,
            reinterpret_cast<void*>(&onGetBuffer),
            reinterpret_cast<void**>(&originalGetBuffer_));

        if (!getBufferHook_.installed()) {
            openHook_.reset();
            return false;
        }

        readHook_ = pl::memory::HookHandle(
            readTarget,
            reinterpret_cast<void*>(&onRead),
            reinterpret_cast<void**>(&originalRead_));

        if (!readHook_.installed()) {
            getBufferHook_.reset();
            openHook_.reset();
            return false;
        }

        closeHook_ = pl::memory::HookHandle(
            closeTarget,
            reinterpret_cast<void*>(&onClose),
            reinterpret_cast<void**>(&originalClose_));

        if (!closeHook_.installed()) {
            readHook_.reset();
            getBufferHook_.reset();
            openHook_.reset();
            return false;
        }

        return true;
    }

    static AAsset* onOpen(AAssetManager* manager,
                          const char* filename,
                          int mode) {
        auto& mod = instance();
        AAsset* asset = mod.originalOpen_
            ? mod.originalOpen_(manager, filename, mode)
            : nullptr;

        if (asset && filename && filename == kTargetAsset) {
            std::lock_guard lock(mod.assetMutex_);
            mod.targetAssets_.insert(asset);
            mod.self_.getLogger().info("Watching {}", filename);
        }

        return asset;
    }

    static const void* onGetBuffer(AAsset* asset) {
        auto& mod = instance();

        if (!mod.originalGetBuffer_ || !mod.isTarget(asset)) {
            return mod.originalGetBuffer_
                ? mod.originalGetBuffer_(asset)
                : nullptr;
        }

        const void* original = mod.originalGetBuffer_(asset);
        if (!original) {
            return nullptr;
        }

        const off64_t length = AAsset_getLength64(asset);
        if (length <= 0 || length > 4 * 1024 * 1024) {
            return original;
        }

        std::vector<uint8_t> patched(static_cast<size_t>(length));
        std::memcpy(patched.data(), original, patched.size());

        if (!patchPlatformConfig(patched.data(), patched.size())) {
            return original;
        }

        std::lock_guard lock(mod.assetMutex_);
        auto& entry = mod.patchedBuffers_[asset];
        entry = std::move(patched);

        mod.self_.getLogger().info(
            "Patched Android VV platform config through AAsset_getBuffer");

        return entry.data();
    }

    static int onRead(AAsset* asset, void* buffer, size_t count) {
        auto& mod = instance();

        if (!mod.originalRead_) {
            return -1;
        }

        const int result = mod.originalRead_(asset, buffer, count);

        if (result > 0 && buffer && mod.isTarget(asset)) {
            // The Android platform configuration is only a few KB. When the
            // complete asset is read in one operation, patch it in-place.
            const off64_t length = AAsset_getLength64(asset);
            if (length > 0 && static_cast<off64_t>(result) == length) {
                if (patchPlatformConfig(buffer, static_cast<size_t>(result))) {
                    mod.self_.getLogger().info(
                        "Patched Android VV platform config through AAsset_read");
                }
            }
        }

        return result;
    }

    static void onClose(AAsset* asset) {
        auto& mod = instance();

        {
            std::lock_guard lock(mod.assetMutex_);
            mod.targetAssets_.erase(asset);
            mod.patchedBuffers_.erase(asset);
        }

        if (mod.originalClose_) {
            mod.originalClose_(asset);
        }
    }

    bool isTarget(AAsset* asset) {
        std::lock_guard lock(assetMutex_);
        return targetAssets_.contains(asset);
    }

    ll::mod::NativeMod& self_;

    pl::memory::HookHandle openHook_;
    pl::memory::HookHandle getBufferHook_;
    pl::memory::HookHandle readHook_;
    pl::memory::HookHandle closeHook_;

    OpenFn originalOpen_{};
    GetBufferFn originalGetBuffer_{};
    ReadFn originalRead_{};
    CloseFn originalClose_{};

    std::mutex assetMutex_;
    std::unordered_set<AAsset*> targetAssets_;
    std::unordered_map<AAsset*, std::vector<uint8_t>> patchedBuffers_;
};

} // namespace

PL_REGISTER_MOD(VVOptimizer, VVOptimizer::instance())
