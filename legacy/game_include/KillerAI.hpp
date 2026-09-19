#pragma once
#include <algorithm>
#include <string>
#include <vector>

#include "Math.hpp"

namespace game {
class NavGrid;

class KillerAI {
public:
    enum class State { Patrol, Alert, Sneak, Chase, Search, Enrage };
    enum class Kind { Stalker, Whisper, Herd, Butcher, Warden };

    void reset(double x, double z, Kind k = Kind::Stalker);
    void update(double dt, const std::vector<Vec3>& survivorPositions, const NavGrid& nav);
    bool reportNoise(const Vec3& source);
    void applyNetState(double x, double z, int stateId, int angerValue);
    void smoothNet(double dt);

    State state() const { return st; }
    Kind kind() const { return kind_; }
    std::string stateName() const;
    std::string typeName() const;
    int anger() const { return angerVal; }
    bool seesSurvivor() const { return seesAny; }
    const Vec3& position() const { return pos; }
    double speed() const { return currentSpeed; }
    int attackDamage() const { return damage; }
    double attackInterval() const { return interval; }
    double detectRange() const;
    double silenceRadius() const { return kind_ == Kind::Whisper ? 6.0 : 0.0; }
    bool silencedAt(const Vec3& p) const {
        return silenceRadius() > 0.0 && pos.distance(p) <= silenceRadius();
    }
    bool isTemporaryKiller() const { return life > 0.0; }
    double lifeLeft() const { return life; }
    void setLife(double seconds) { life = seconds; }
    void tickLife(double dt) { if (life > 0.0) life -= dt; }
    bool hasSplit() const { return splitDone; }
    void markSplit() { splitDone = true; }
    void setAngerScale(double s) { angerScale = s; }
    double angerScaleValue() const { return angerScale; }
    void addAnger(int amount) {
        angerVal = std::max(0, std::min(100, angerVal + amount));
    }

private:
    State st = State::Patrol;
    Kind kind_ = Kind::Stalker;
    Vec3 pos;
    Vec3 wanderDir{1.0, 0.0, 0.0};
    Vec3 lastKnownTarget;
    double wanderTimer = 0.0;
    double stateTimer = 0.0;
    double sightTimer = 0.0;
    double currentSpeed = 0.0;
    int angerVal = 0;
    bool seesAny = false;

    double visionMul = 1.0;
    double speedMul = 1.0;
    double noiseMul = 1.0;
    double damage = 25;
    double interval = 4.0;

    std::vector<Vec3> path;
    int pathSx = -1;
    int pathSz = -1;
    int pathTx = -1;
    int pathTz = -1;
    Vec3 netPos;
    bool netActive = false;
    double life = -1.0;
    bool splitDone = false;
    double angerScale = 1.0;

    void changeState(State s);
    void stepTo(double dt, double speed, const Vec3& goal, const NavGrid& nav);
    void patrol(double dt, const NavGrid& nav);
    void alert(double dt, const NavGrid& nav);
    void chase(double dt, const NavGrid& nav);
    void search(double dt, const NavGrid& nav);
    void scan(const std::vector<Vec3>& pts);
};
}
