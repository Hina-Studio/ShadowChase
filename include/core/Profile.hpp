#pragma once
#include <map>
#include <string>
#include <vector>

struct ProfileData {
    int matches = 0;
    int survivorWins = 0;
    int killerWins = 0;
    int escapes = 0;
    int eliminated = 0;
    double bestSurvivorTime = 0.0;
    std::map<std::string, int> kills;
    std::vector<std::string> achievements;
};

class Profile {
public:
    void load(const std::string& path);
    bool save(const std::string& path) const;

    void addMatch(int escaped, int eliminated, bool survivorWin, double matchTime);
    void addKill(const std::string& killer, int count);
    void unlock(const std::string& id);
    bool has(const std::string& id) const;

    const ProfileData& data() const { return d; }

private:
    ProfileData d;
};
