#pragma once
#include <map>
#include <string>
#include <vector>

class Platform {
public:
    void init(bool enableSteam);
    void shutdown();
    void runCallbacks();

    void unlockAchievement(const std::string& id);
    void setRichPresence(const std::string& key, const std::string& value);

    std::string backendName() const;
    bool steamAvailable() const { return steam; }
    const std::vector<std::string>& unlockedAchievements() const { return achievements; }

private:
    bool enabled = false;
    bool steam = false;
    std::vector<std::string> achievements;
    std::map<std::string, std::string> richPresence;
};
