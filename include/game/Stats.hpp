#pragma once
#include <map>
#include <string>

namespace game {
struct MatchStats {
    double matchTime = 0.0;
    int survivorsEscaped = 0;
    int survivorsEliminated = 0;
    int generatorsActivated = 0;
    int rescues = 0;
    int itemsUsed = 0;
    int charmsUsed = 0;
    int damageDealt = 0;
    int downs = 0;
    std::string topKiller = "none";
    std::map<std::string, int> killsByKiller;
};
}
