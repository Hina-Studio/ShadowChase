#include "core/Platform.hpp"

#include <algorithm>

#include "core/Logger.hpp"

#ifdef SLASHCO_ENABLE_STEAM
#include <steam/steam_api.h>
#endif

void Platform::init(bool enableSteam) {
    enabled = enableSteam;
    steam = false;

#ifdef SLASHCO_ENABLE_STEAM
    if (enabled) {
        steam = SteamAPI_Init();
        if (steam) {
            core::Logger::info("Platform backend: Steam");
            return;
        }
        core::Logger::warn("Steam init failed, falling back to local backend");
    }
#else
    (void)enableSteam;
#endif
    core::Logger::info("Platform backend: local (Steam SDK not linked)");
}

void Platform::shutdown() {
#ifdef SLASHCO_ENABLE_STEAM
    if (steam) {
        SteamAPI_Shutdown();
    }
#endif
    steam = false;
}

void Platform::runCallbacks() {
#ifdef SLASHCO_ENABLE_STEAM
    if (steam) {
        SteamAPI_RunCallbacks();
    }
#endif
}

void Platform::unlockAchievement(const std::string& id) {
    if (std::find(achievements.begin(), achievements.end(), id) != achievements.end()) {
        return;
    }
    achievements.push_back(id);

#ifdef SLASHCO_ENABLE_STEAM
    if (steam) {
        SteamUserStats()->SetAchievement(id.c_str());
        SteamUserStats()->StoreStats();
        core::Logger::info("Steam achievement unlocked: " + id);
        return;
    }
#endif
    core::Logger::info("Achievement unlocked (local): " + id);
}

void Platform::setRichPresence(const std::string& key, const std::string& value) {
    auto it = richPresence.find(key);
    if (it != richPresence.end() && it->second == value) return;
    richPresence[key] = value;

#ifdef SLASHCO_ENABLE_STEAM
    if (steam) {
        SteamFriends()->SetRichPresence(key.c_str(), value.c_str());
        return;
    }
#endif
}

std::string Platform::backendName() const {
    return steam ? "steam" : "local";
}
