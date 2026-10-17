#include "ClientApp.hpp"

#if defined(__ANDROID__)
#include <sys/types.h>
#include <unistd.h>

#include <android/asset_manager.h>
#include <android_native_app_glue.h>

#include <algorithm>
#include <cstdio>
#include <string>

// raylib (PLATFORM_ANDROID) exports the glue handle once android_main ran.
extern "C" struct android_app* GetAndroidApp(void);

static void androidSetup(const char* /*appName*/) {
    struct android_app* app = GetAndroidApp();
    if (!app || !app->activity) return;

    const char* dataPath = app->activity->internalDataPath;
    std::string dir = dataPath ? dataPath : ".";
    chdir(dir.c_str());

    AAssetManager* mgr = app->activity->assetManager;
    if (!mgr) return;
    AAsset* cfg = AAssetManager_open(mgr, "config.ini", AASSET_MODE_BUFFER);
    if (!cfg) return;
    std::string out = dir + "/config.ini";
    FILE* f = std::fopen(out.c_str(), "wb");
    if (f) {
        const off_t len = AAsset_getLength(cfg);
        std::string buf(static_cast<size_t>(len), '\0');
        AAsset_read(cfg, buf.data(), static_cast<size_t>(len));
        std::fwrite(buf.data(), 1, buf.size(), f);
        std::fclose(f);
    }
    AAsset_close(cfg);
}
#endif

int main(int argc, char** argv) {
#if defined(__ANDROID__)
    androidSetup("SlashCo");
#endif
    ClientApp app;
    if (!app.init(argc, argv)) {
        return 1;
    }
    app.run();
    app.shutdown();
    return 0;
}
