#include "game/KillerAI.hpp"

#include <algorithm>
#include <cmath>

#include "core/Random.hpp"
#include "game/NavGrid.hpp"

namespace game {
namespace {
constexpr double kDetectRange = 18.0;
constexpr double kHearRange = 8.0;
constexpr double kAttackRange = 2.2;
constexpr double kAngerPerSpot = 8.0;
constexpr double kAngerGainChase = 5.0;
constexpr double kAngerGainEnrage = 8.0;
constexpr double kAngerDecay = 4.0;
constexpr double kMaxAnger = 100.0;
constexpr double kSearchTime = 5.0;
constexpr double kAlertTime = 3.0;
constexpr double kLoseSightTime = 3.0;
constexpr double kAngerVisionBonus = 0.15;
}

std::string KillerAI::stateName() const {
    switch (st) {
        case State::Patrol: return "Patrol";
        case State::Alert: return "Alert";
        case State::Sneak: return "Sneak";
        case State::Chase: return "Chase";
        case State::Search: return "Search";
        case State::Enrage: return "Enrage";
    }
    return "Unknown";
}

std::string KillerAI::typeName() const {
    switch (kind_) {
        case Kind::Stalker: return "Stalker";
        case Kind::Whisper: return "Whisper";
        case Kind::Herd: return "Herd";
        case Kind::Butcher: return "Butcher";
        case Kind::Warden: return "Warden";
    }
    return "Stalker";
}

double KillerAI::detectRange() const {
    return kDetectRange * visionMul + angerVal * kAngerVisionBonus;
}

void KillerAI::changeState(State s) {
    if (st == s) return;
    st = s;
    stateTimer = 0.0;
    sightTimer = 0.0;
    path.clear();
    pathSx = pathSz = pathTx = pathTz = -1;
}

void KillerAI::reset(double x, double z, Kind k) {
    kind_ = k;
    switch (k) {
        case Kind::Whisper:
            damage = 34;
            interval = 3.0;
            visionMul = 0.8;
            speedMul = 1.05;
            noiseMul = 2.0;
            break;
        case Kind::Herd:
            damage = 15;
            interval = 1.2;
            visionMul = 1.1;
            speedMul = 0.95;
            noiseMul = 0.6;
            break;
        case Kind::Butcher:
            damage = 40;
            interval = 2.5;
            visionMul = 1.2;
            speedMul = 1.0;
            noiseMul = 1.2;
            break;
        case Kind::Warden:
            damage = 30;
            interval = 3.5;
            visionMul = 1.4;
            speedMul = 0.85;
            noiseMul = 1.0;
            break;
        case Kind::Stalker:
        default:
            damage = 25;
            interval = 4.0;
            visionMul = 1.0;
            speedMul = 1.0;
            noiseMul = 1.0;
            break;
    }
    pos = Vec3{x, 0.0, z};
    angerVal = 0;
    currentSpeed = 2.0 * speedMul;
    st = State::Patrol;
    stateTimer = 0.0;
    sightTimer = 0.0;
    wanderTimer = 0.0;
    wanderDir = Vec3{1.0, 0.0, 0.0};
    seesAny = false;
    path.clear();
    pathSx = pathSz = pathTx = pathTz = -1;
    netActive = false;
}

bool KillerAI::reportNoise(const Vec3& source) {
    if (st == State::Chase || st == State::Enrage) return false;
    if (pos.distance(source) > kHearRange * noiseMul) return false;

    State old = st;
    lastKnownTarget = source;
    angerVal = static_cast<int>(std::min(kMaxAnger, angerVal + 4.0 * angerScale));
    if (st == State::Patrol || st == State::Sneak || st == State::Search) {
        changeState(State::Alert);
    }
    return old != st && st == State::Alert;
}

void KillerAI::applyNetState(double x, double z, int stateId, int angerValue) {
    if (!netActive) {
        pos = Vec3{x, 0.0, z};
        netActive = true;
    }
    netPos = Vec3{x, 0.0, z};
    if (stateId >= 0 && stateId <= 5) {
        st = static_cast<State>(stateId);
    }
    angerVal = std::max(0, std::min(100, angerValue));
    path.clear();
    pathSx = pathSz = pathTx = pathTz = -1;
}

void KillerAI::smoothNet(double dt) {
    if (!netActive) return;
    double k = 1.0 - std::exp(-12.0 * dt);
    pos = pos + (netPos - pos) * k;
}

void KillerAI::update(double dt, const std::vector<Vec3>& pts, const NavGrid& nav) {
    scan(pts);

    switch (st) {
        case State::Patrol: patrol(dt, nav); break;
        case State::Alert: alert(dt, nav); break;
        case State::Sneak: alert(dt, nav); break;
        case State::Chase: chase(dt, nav); break;
        case State::Search: search(dt, nav); break;
        case State::Enrage: chase(dt, nav); break;
    }
}

void KillerAI::scan(const std::vector<Vec3>& pts) {
    seesAny = false;
    double maxD = (st == State::Enrage) ? 1e18 : detectRange();
    Vec3 nearest;
    double best = 1e18;
    for (const auto& p : pts) {
        double d = pos.distance(p);
        if (d <= maxD && d < best) {
            best = d;
            nearest = p;
            seesAny = true;
        }
    }
    if (!seesAny) return;

    lastKnownTarget = nearest;
    if (st == State::Patrol || st == State::Alert || st == State::Sneak || st == State::Search) {
        angerVal = static_cast<int>(std::min(kMaxAnger, angerVal + kAngerPerSpot * angerScale));
        changeState(State::Chase);
    }
}

void KillerAI::stepTo(double dt, double speed, const Vec3& goal, const NavGrid& nav) {
    int sx, sz, tx, tz;
    nav.toTile(pos, sx, sz);
    nav.toTile(goal, tx, tz);
    if (sx != pathSx || sz != pathSz || tx != pathTx || tz != pathTz) {
        path = nav.findPath(pos, goal);
        pathSx = sx;
        pathSz = sz;
        pathTx = tx;
        pathTz = tz;
    }

    if (!path.empty()) {
        Vec3 next = path.front();
        if (pos.distance(next) <= 0.55) {
            path.erase(path.begin());
        } else {
            Vec3 delta = (next - pos).normalized() * (speed * dt);
            pos = nav.slideMove(pos, delta);
            return;
        }
    }

    Vec3 d = goal - pos;
    double len = d.length();
    if (len > 0.05) {
        Vec3 delta = d.normalized() * (speed * dt);
        pos = nav.slideMove(pos, delta);
    }
}

void KillerAI::patrol(double dt, const NavGrid& nav) {
    currentSpeed = 2.0 * speedMul;
    wanderTimer -= dt;
    if (wanderTimer <= 0.0) {
        wanderTimer = 3.0;
        auto& rng = core::Random::instance();
        wanderDir = Vec3{rng.range(-100, 101) / 100.0, 0.0, rng.range(-100, 101) / 100.0};
        wanderDir = wanderDir.normalized();
    }
    if (wanderDir.length() < 0.5) wanderDir = Vec3{1.0, 0.0, 0.0};
    pos = nav.slideMove(pos, wanderDir * (dt * currentSpeed));

    angerVal = std::max(0, angerVal - static_cast<int>(kAngerDecay * dt));
}

void KillerAI::alert(double dt, const NavGrid& nav) {
    currentSpeed = 2.6 * speedMul;
    stateTimer += dt;
    double dist = pos.distance(lastKnownTarget);
    if (dist > kAttackRange && dist < 20.0) {
        stepTo(dt, currentSpeed, lastKnownTarget, nav);
    }
    if (stateTimer >= kAlertTime) {
        changeState(State::Patrol);
    }
    angerVal = std::max(0, angerVal - static_cast<int>(kAngerDecay * dt));
}

void KillerAI::chase(double dt, const NavGrid& nav) {
    if (!seesAny) {
        sightTimer += dt;
        if (sightTimer >= kLoseSightTime) {
            changeState(State::Search);
        }
        return;
    }

    sightTimer = 0.0;
    bool enraged = (st == State::Enrage);
    currentSpeed = (enraged ? 5.6 : 4.2) * speedMul;
    angerVal = static_cast<int>(std::min(
        kMaxAnger, angerVal + (enraged ? kAngerGainEnrage : kAngerGainChase) * angerScale * dt));
    if (!enraged && angerVal >= static_cast<int>(kMaxAnger)) {
        changeState(State::Enrage);
        return;
    }

    double dist = pos.distance(lastKnownTarget);
    if (dist > kAttackRange) {
        stepTo(dt, currentSpeed, lastKnownTarget, nav);
    }
}

void KillerAI::search(double dt, const NavGrid& nav) {
    currentSpeed = 2.8 * speedMul;
    stateTimer += dt;
    double dist = pos.distance(lastKnownTarget);
    if (dist > 0.5) {
        stepTo(dt, currentSpeed, lastKnownTarget, nav);
    }
    if (stateTimer >= kSearchTime) {
        changeState(State::Patrol);
    }
    angerVal = std::max(0, angerVal - static_cast<int>(kAngerDecay * dt));
}
}
