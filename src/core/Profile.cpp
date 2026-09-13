#include "core/Profile.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

void Profile::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    d = ProfileData{};
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        if (key.rfind("kill.", 0) == 0) {
            d.kills[key.substr(5)] = std::atoi(value.c_str());
        } else if (key.rfind("ach.", 0) == 0) {
            if (std::atoi(value.c_str()) != 0) d.achievements.push_back(key.substr(4));
        } else if (key == "matches") {
            d.matches = std::atoi(value.c_str());
        } else if (key == "survivorWins") {
            d.survivorWins = std::atoi(value.c_str());
        } else if (key == "killerWins") {
            d.killerWins = std::atoi(value.c_str());
        } else if (key == "escapes") {
            d.escapes = std::atoi(value.c_str());
        } else if (key == "eliminated") {
            d.eliminated = std::atoi(value.c_str());
        } else if (key == "bestSurvivorTime") {
            d.bestSurvivorTime = std::stod(value);
        }
    }
}

bool Profile::save(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "matches=" << d.matches << "\n";
    file << "survivorWins=" << d.survivorWins << "\n";
    file << "killerWins=" << d.killerWins << "\n";
    file << "escapes=" << d.escapes << "\n";
    file << "eliminated=" << d.eliminated << "\n";
    file << "bestSurvivorTime=" << d.bestSurvivorTime << "\n";
    for (const auto& kv : d.kills) {
        file << "kill." << kv.first << "=" << kv.second << "\n";
    }
    for (const auto& a : d.achievements) {
        file << "ach." << a << "=1\n";
    }
    return true;
}

void Profile::addMatch(int escaped, int eliminated, bool survivorWin, double matchTime) {
    ++d.matches;
    d.escapes += escaped;
    d.eliminated += eliminated;
    if (survivorWin) {
        ++d.survivorWins;
        if (d.bestSurvivorTime <= 0.0 || (matchTime > 0.0 && matchTime < d.bestSurvivorTime)) {
            d.bestSurvivorTime = matchTime;
        }
    } else {
        ++d.killerWins;
    }
}

void Profile::addKill(const std::string& killer, int count) {
    d.kills[killer] += count;
}

void Profile::unlock(const std::string& id) {
    for (const auto& a : d.achievements) {
        if (a == id) return;
    }
    d.achievements.push_back(id);
}

bool Profile::has(const std::string& id) const {
    for (const auto& a : d.achievements) {
        if (a == id) return true;
    }
    return false;
}
