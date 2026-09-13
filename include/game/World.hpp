#pragma once
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "Balance.hpp"
#include "Food.hpp"
#include "Health.hpp"
#include "KillerAI.hpp"
#include "Math.hpp"
#include "NavGrid.hpp"
#include "Stats.hpp"

namespace game {
struct Control {
    Vec3 move;
    bool interact = false;
    bool sprint = false;
};

struct Survivor {
    int id = 0;
    Vec3 position;
    Health health;
    double hurtCooldown = 4.0;
    int downs = 0;
    bool eliminated = false;
    bool escaped = false;

    bool downed = false;
    double downTimer = 0.0;
    double rescueCharge = 0.0;
    double escapeCharge = 0.0;

    int coins = 200;
    bool charmBought = false;

    Vec3 netPos;
    bool netActive = false;
    bool inSilence = false;

    double stamina = 100.0;

    std::vector<FoodItem> items;
    std::string effect;
    double effectTimer = 0.0;

    Survivor() = default;
    Survivor(int sid, const Vec3& p) : id(sid), position(p) {}

    bool hasEffect(const std::string& tag) const {
        return effect == tag && effectTimer > 0.0;
    }

    void grantEffect(const std::string& tag, double seconds) {
        effect = tag;
        effectTimer = seconds;
    }

    void tickEffects(double dt) {
        if (effect.empty()) return;
        effectTimer -= dt;
        if (effectTimer <= 0.0) {
            effect.clear();
            effectTimer = 0.0;
        }
    }

    void applyFood(const FoodItem& f) {
        if (f.heal > 0) health.heal(f.heal);
        if (f.stamina > 0) stamina = std::min(100.0, stamina + f.stamina);
        if (!f.effect.empty()) grantEffect(f.effect, f.duration);
    }
};

struct Generator {
    int id = 0;
    Vec3 position;
    int fuelMax = 4;
    double work = 0.0;
    bool activated = false;

    Generator() = default;
    Generator(int gid, const Vec3& p, int maxFuel) : id(gid), position(p), fuelMax(maxFuel) {}
};

struct Exit {
    int id = 0;
    Vec3 position;
    bool open = false;

    Exit() = default;
    Exit(int eid, const Vec3& p) : id(eid), position(p) {}
};

class World {
public:
    void configure(int playerCount, const std::vector<KillerAI::Kind>& kinds,
                   const std::string& mapName = "Factory", const std::string& mode = "Classic");
    void configureTutorial();
    void update(double dt, const Control& ctrl);
    void bindLocalPlayer(int survivorId);

    const std::vector<Survivor>& survivors() const { return survivorList; }
    const std::vector<Generator>& generators() const { return generatorList; }
    const std::vector<Exit>& exits() const { return exitList; }
    bool doorsOpen() const { return escapeOpen; }
    const std::vector<KillerAI>& killers() const { return killerList; }
    const Survivor* localSurvivor() const;
    void useLocalItem(int slot);
    bool buyCharm(int survivorId, bool enhanced);
    void setRemoteControl(int survivorId, const Control& c);
    void smoothNetwork(double dt);
    void setPrediction(bool on) { predictionEnabled = on; }
    void predictLocal(double dt, const Control& ctrl);
    const NavGrid& nav() const { return navGrid; }
    double clock() const { return matchClock; }
    bool over() const { return matchOver; }
    std::string result() const { return resultText; }
    const std::string& mapName() const { return mapName_; }
    const MatchStats& stats() const { return stats_; }
    const Vec3& vaultPos() const { return vaultPos_; }
    bool vaultOpened() const { return vaultOpened_; }
    int vaultVariant() const { return vaultVariant_; }
    const std::string& modeName() const { return mode_; }
    const Vec3& powerSwitchPos() const { return powerSwitchPos_; }
    bool powerSwitchActivated() const { return powerSwitchOn_; }
    Balance& balance() { return balance_; }
    void syncBalance();
    std::vector<std::string> takeEvents();
    std::string serializeSnapshot() const;
    void applySnapshot(const std::string& data);

private:
    std::vector<Survivor> survivorList;
    std::vector<Generator> generatorList;
    std::vector<Exit> exitList;
    std::vector<KillerAI> killerList;
    std::vector<std::string> pendingEvents;
    double matchClock = 0.0;
    bool matchOver = false;
    std::string resultText = "None";
    int localPlayer = -1;
    bool escapeOpen = false;
    bool mapCharmUsed = false;
    bool enhancedBought = false;
    NavGrid navGrid;
    std::unordered_map<int, Control> remoteCmds;
    std::string mapName_ = "Factory";
    bool predictionEnabled = false;
    MatchStats stats_;
    Vec3 vaultPos_;
    bool vaultOpened_ = false;
    int vaultVariant_ = 0;
    Balance balance_;
    std::string mode_ = "Classic";
    Vec3 powerSwitchPos_;
    bool powerSwitchOn_ = false;
    double powerCharge_ = 0.0;

    int generatorCountFor(int players) const;
    bool allGeneratorsDone() const;
    bool canOpenDoors() const;
    bool wardenNear(const Vec3& p) const;
};
}
