#pragma once
#include <string>
#include <vector>

namespace game {
struct ReplayFrame {
    float dt = 0.0f;
    float mx = 0.0f;
    float mz = 0.0f;
    bool interact = false;
    bool sprint = false;
};

struct ReplayData {
    unsigned long long seed = 0;
    int players = 4;
    std::string map = "Factory";
    std::string mode = "Classic";
    std::string killerTypes = "Stalker";
    double balMove = 0.0;
    double balSprint = 0.0;
    double balDrain = 0.0;
    double balRegen = 0.0;
    double balRepair = 0.0;
    double balDown = 0.0;
    double balDmg = 0.0;
    double balAnger = 0.0;
    int balFuel = 0;
    bool network = false;
    std::vector<ReplayFrame> frames;

    struct NetFrame {
        float dt = 0.0f;
        std::string data;
    };
    std::vector<NetFrame> netFrames;

    bool save(const std::string& path) const;
    bool load(const std::string& path);
};
}
