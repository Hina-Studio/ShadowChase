#include "game/Replay.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace game {
bool ReplayData::save(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "R|" << seed << "|" << players << "|" << map << "|" << killerTypes << "|"
         << (network ? 1 : 0) << "|" << mode << "\n";
    file << "B|" << balMove << "|" << balSprint << "|" << balDrain << "|" << balRegen << "|"
         << balRepair << "|" << balDown << "|" << balDmg << "|" << balAnger << "|" << balFuel
         << "\n";

    char buf[128];
    for (const auto& f : frames) {
        std::snprintf(buf, sizeof(buf), "F|%.5f|%.3f|%.3f|%d|%d", f.dt, f.mx, f.mz,
                      f.interact ? 1 : 0, f.sprint ? 1 : 0);
        file << buf << "\n";
    }
    for (const auto& nf : netFrames) {
        std::snprintf(buf, sizeof(buf), "N|%.5f|", nf.dt);
        file << buf << nf.data << "\n";
    }
    return true;
}

bool ReplayData::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    frames.clear();
    netFrames.clear();
    network = false;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string tag;
        std::getline(ss, tag, '|');
        if (tag == "R") {
            std::string tok;
            std::getline(ss, tok, '|');
            seed = std::stoull(tok);
            std::getline(ss, tok, '|');
            players = std::atoi(tok.c_str());
            std::getline(ss, map, '|');
            std::getline(ss, killerTypes, '|');
            if (std::getline(ss, tok)) {
                network = std::atoi(tok.c_str()) != 0;
            }
            if (std::getline(ss, tok) && !tok.empty()) {
                mode = tok;
            }
        } else if (tag == "B") {
            std::string tok;
            std::getline(ss, tok, '|'); balMove = std::stod(tok);
            std::getline(ss, tok, '|'); balSprint = std::stod(tok);
            std::getline(ss, tok, '|'); balDrain = std::stod(tok);
            std::getline(ss, tok, '|'); balRegen = std::stod(tok);
            std::getline(ss, tok, '|'); balRepair = std::stod(tok);
            std::getline(ss, tok, '|'); balDown = std::stod(tok);
            std::getline(ss, tok, '|'); balDmg = std::stod(tok);
            std::getline(ss, tok, '|'); balAnger = std::stod(tok);
            std::getline(ss, tok, '|'); balFuel = std::atoi(tok.c_str());
        } else if (tag == "F") {
            ReplayFrame f;
            std::string tok;
            std::getline(ss, tok, '|'); f.dt = std::stof(tok);
            std::getline(ss, tok, '|'); f.mx = std::stof(tok);
            std::getline(ss, tok, '|'); f.mz = std::stof(tok);
            std::getline(ss, tok, '|'); f.interact = std::atoi(tok.c_str()) != 0;
            std::getline(ss, tok); f.sprint = std::atoi(tok.c_str()) != 0;
            frames.push_back(f);
        } else if (tag == "N") {
            NetFrame nf;
            std::string tok;
            std::getline(ss, tok, '|');
            nf.dt = std::stof(tok);
            std::getline(ss, nf.data);
            netFrames.push_back(nf);
        }
    }
    return !frames.empty() || !netFrames.empty();
}
}
