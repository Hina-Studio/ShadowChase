#include "shared/Sim.hpp"

#include <algorithm>
#include <random>

namespace sc {
InputCmd sanitizeInput(const InputCmd& in) {
    InputCmd out = in;
    out.moveX = std::max(-1.0, std::min(1.0, out.moveX));
    out.moveZ = std::max(-1.0, std::min(1.0, out.moveZ));
    if (out.moveX * out.moveX + out.moveZ * out.moveZ > 1.0) {
        Vec2 n = Vec2{out.moveX, out.moveZ}.normalized();
        out.moveX = n.x;
        out.moveZ = n.z;
    }
    while (out.yaw > 3.14159265358979323846) out.yaw -= 6.28318530717958647692;
    while (out.yaw < -3.14159265358979323846) out.yaw += 6.28318530717958647692;
    return out;
}

void Sim::generate(unsigned int seed, int size) {
    seed_ = seed;
    size_ = std::max(16, std::min(96, size));
    tick_ = 0;
    blocks_.clear();
    spawns_.clear();
    players_.clear();
    inputs_.clear();
    grid_.assign(static_cast<size_t>(size_) * static_cast<size_t>(size_), 0);

    auto setBlock = [&](int x, int z) {
        if (x < 0 || z < 0 || x >= size_ || z >= size_) return;
        grid_[static_cast<size_t>(z) * static_cast<size_t>(size_) + static_cast<size_t>(x)] = 1;
        blocks_.push_back(Block{x, z});
    };

    for (int i = 0; i < size_; ++i) {
        setBlock(i, 0);
        setBlock(i, size_ - 1);
        setBlock(0, i);
        setBlock(size_ - 1, i);
    }

    std::mt19937 rng(seed);
    int interior = size_ - 4;
    int blockCount = interior * interior / 22;
    std::uniform_int_distribution<int> dist(2, size_ - 3);
    for (int i = 0; i < blockCount; ++i) {
        int x = dist(rng);
        int z = dist(rng);
        if (grid_[static_cast<size_t>(z) * static_cast<size_t>(size_) + static_cast<size_t>(x)] == 0) {
            setBlock(x, z);
        }
    }

    double center = static_cast<double>(size_) / 2.0;
    double ring = center - 4.0;
    for (int i = 0; i < 16; ++i) {
        double a = (6.28318530717958647692 * static_cast<double>(i)) / 16.0;
        Vec2 p{center + std::cos(a) * ring, center + std::sin(a) * ring};
        spawns_.push_back(p);
        int gx = static_cast<int>(p.x);
        int gz = static_cast<int>(p.z);
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                int cx = gx + dx;
                int cz = gz + dz;
                if (cx > 0 && cz > 0 && cx < size_ - 1 && cz < size_ - 1) {
                    grid_[static_cast<size_t>(cz) * static_cast<size_t>(size_) + static_cast<size_t>(cx)] = 0;
                }
            }
        }
    }
    blocks_.clear();
    for (int z = 0; z < size_; ++z) {
        for (int x = 0; x < size_; ++x) {
            if (grid_[static_cast<size_t>(z) * static_cast<size_t>(size_) + static_cast<size_t>(x)] != 0) {
                blocks_.push_back(Block{x, z});
            }
        }
    }

    dynamicGrid_ = grid_;
    objects_.clear();
    auto freeSpot = [&](int& ox, int& oz) {
        std::uniform_int_distribution<int> d(3, size_ - 4);
        for (int tries = 0; tries < 64; ++tries) {
            int x = d(rng);
            int z = d(rng);
            if (grid_[static_cast<size_t>(z) * static_cast<size_t>(size_) + static_cast<size_t>(x)] == 0) {
                ox = x;
                oz = z;
                return true;
            }
        }
        return false;
    };

    int nextId = 1;
    for (int i = 0; i < 4; ++i) {
        int x = 0;
        int z = 0;
        if (!freeSpot(x, z)) break;
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Door;
        o.pos = Vec2{x + 0.5, z + 0.5};
        objects_.push_back(o);
    }
    for (int i = 0; i < 6; ++i) {
        int x = 0;
        int z = 0;
        if (!freeSpot(x, z)) break;
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Crate;
        o.pos = Vec2{x + 0.5, z + 0.5};
        objects_.push_back(o);
    }
    for (int i = 0; i < 6; ++i) {
        int x = 0;
        int z = 0;
        if (!freeSpot(x, z)) break;
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Pickup;
        o.pos = Vec2{x + 0.5, z + 0.5};
        objects_.push_back(o);
    }

    rng_.seed(seed ^ 0x5F3759DFu);
    monsters_.clear();
    {
        Vec2 spot = randomFreeSpot();
        for (int tries = 0; tries < 32; ++tries) {
            double dc = spot.distance(Vec2{size_ / 2.0, size_ / 2.0});
            if (dc > size_ / 3.0) break;
            spot = randomFreeSpot();
        }
        MonsterState m;
        m.id = 0;
        m.pos = spot;
        m.patrolTarget = randomFreeSpot();
        m.yaw = 0.0;
        monsters_.push_back(m);
    }
}

bool Sim::hasLineOfSight(const Vec2& a, const Vec2& b) const {
    const std::vector<unsigned char>& g = dynamicGrid_.empty() ? grid_ : dynamicGrid_;
    Vec2 d = b - a;
    double dist = d.length();
    int steps = static_cast<int>(dist / 0.4) + 1;
    for (int i = 1; i < steps; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(steps);
        Vec2 p = a + d * t;
        if (blockedCell(g, size_, p.x, p.z)) return false;
    }
    return true;
}

Vec2 Sim::randomFreeSpot() {
    std::uniform_int_distribution<int> dist(2, size_ - 3);
    for (int tries = 0; tries < 64; ++tries) {
        int x = dist(rng_);
        int z = dist(rng_);
        if (!blockedCell(grid_, size_, x + 0.5, z + 0.5)) {
            return Vec2{x + 0.5, z + 0.5};
        }
    }
    return Vec2{size_ / 2.0, size_ / 2.0};
}

void Sim::updateMonsters(double dt) {
    const std::vector<unsigned char>& g = dynamicGrid_.empty() ? grid_ : dynamicGrid_;
    for (auto& m : monsters_) {
        int seen = -1;
        Vec2 seenPos;
        for (const auto& p : players_) {
            if (!p.alive) continue;
            Vec2 d = p.pos - m.pos;
            double dist = d.length();
            if (dist > kMonsterSight) continue;
            bool moving = p.lastSpeed > kPlayerMovingThreshold;
            bool close = dist <= kMonsterCloseRange;
            if (!moving && !close) continue;
            Vec2 dir = d.normalized();
            Vec2 f{std::sin(m.yaw), std::cos(m.yaw)};
            double dot = dir.x * f.x + dir.z * f.z;
            bool inFov = close || dot > 0.819;
            if (inFov && hasLineOfSight(m.pos, p.pos)) {
                seen = p.id;
                seenPos = p.pos;
                break;
            }
        }

        if (seen >= 0) {
            m.state = 1;
            m.target = seen;
            m.lastSeen = seenPos;
            m.loseTimer = 0.0;
        } else if (m.state == 1) {
            m.loseTimer += dt;
            if (m.loseTimer > kMonsterLoseTime) {
                m.state = 0;
                m.target = -1;
                m.patrolTarget = randomFreeSpot();
                m.patrolTimer = 0.0;
            }
        }

        Vec2 goal = (m.state == 1) ? ((seen >= 0) ? seenPos : m.lastSeen) : m.patrolTarget;
        Vec2 to = goal - m.pos;
        double dist = to.length();
        double speed = (m.state == 1) ? kMonsterSpeedChase : kMonsterSpeedPatrol;
        if (dist > 0.08) {
            Vec2 dir = to.normalized();
            m.yaw = std::atan2(dir.x, dir.z);
            moveOnGrid(m.pos, dir, speed, dt, g, size_);
        }
        if (m.state == 0) {
            m.patrolTimer += dt;
            if (dist < 0.6 || m.patrolTimer > 8.0) {
                m.patrolTarget = randomFreeSpot();
                m.patrolTimer = 0.0;
            }
        }
    }
}

void Sim::refreshDynamicBlocks() {
    dynamicGrid_ = grid_;
    for (const auto& o : objects_) {
        if (o.type != ObjType::Door || o.open) continue;
        int gx = static_cast<int>(std::floor(o.pos.x));
        int gz = static_cast<int>(std::floor(o.pos.z));
        if (gx < 0 || gz < 0 || gx >= size_ || gz >= size_) continue;
        dynamicGrid_[static_cast<size_t>(gz) * static_cast<size_t>(size_) + static_cast<size_t>(gx)] = 1;
    }
}

bool Sim::blockedCell(const std::vector<unsigned char>& grid, int size, double x, double z) {
    int gx = static_cast<int>(std::floor(x));
    int gz = static_cast<int>(std::floor(z));
    if (gx < 0 || gz < 0 || gx >= size || gz >= size) return true;
    return grid[static_cast<size_t>(gz) * static_cast<size_t>(size) + static_cast<size_t>(gx)] != 0;
}

void Sim::moveOnGrid(Vec2& pos, const Vec2& dir, double speed, double dt,
                     const std::vector<unsigned char>& grid, int size) {
    double r = kPlayerRadius;
    auto freeAt = [&](double x, double z) {
        return !blockedCell(grid, size, x - r, z - r) && !blockedCell(grid, size, x + r, z - r) &&
               !blockedCell(grid, size, x - r, z + r) && !blockedCell(grid, size, x + r, z + r);
    };

    double nx = pos.x + dir.x * speed * dt;
    if (freeAt(nx, pos.z)) pos.x = nx;
    double nz = pos.z + dir.z * speed * dt;
    if (freeAt(pos.x, nz)) pos.z = nz;
}

bool Sim::blocked(int x, int z) const {
    if (x < 0 || z < 0 || x >= size_ || z >= size_) return true;
    return grid_[static_cast<size_t>(z) * static_cast<size_t>(size_) + static_cast<size_t>(x)] != 0;
}

bool Sim::blockedAt(const Vec2& p) const {
    return blocked(static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.z)));
}

bool Sim::blockedOnAxis(double x, double z) const {
    const std::vector<unsigned char>& g = dynamicGrid_.empty() ? grid_ : dynamicGrid_;
    double r = kPlayerRadius;
    auto hit = [&](double px, double pz) {
        return blockedCell(g, size_, px, pz);
    };
    return hit(x - r, z - r) || hit(x + r, z - r) || hit(x - r, z + r) || hit(x + r, z + r);
}

int Sim::addPlayer(const std::string& name) {
    PlayerState p;
    p.id = static_cast<int>(players_.size());
    p.name = name.empty() ? ("P" + std::to_string(p.id + 1)) : name;
    if (!spawns_.empty()) {
        p.pos = spawns_[static_cast<size_t>(p.id) % spawns_.size()];
    } else {
        p.pos = Vec2{size_ / 2.0, size_ / 2.0};
    }
    p.prevPos = p.pos;
    players_.push_back(p);
    inputs_.push_back(InputCmd{});
    prevInteract_.push_back(0);
    return p.id;
}

void Sim::removePlayer(int id) {
    if (id < 0 || id >= static_cast<int>(players_.size())) return;
    players_[static_cast<size_t>(id)].alive = false;
}

void Sim::setInput(int id, const InputCmd& cmd) {
    if (id < 0 || id >= static_cast<int>(inputs_.size())) return;
    inputs_[static_cast<size_t>(id)] = sanitizeInput(cmd);
}

void Sim::movePlayer(PlayerState& p, const InputCmd& cmd, double dt) {
    Vec2 forward{std::sin(cmd.yaw), std::cos(cmd.yaw)};
    p.yaw = cmd.yaw;
    p.sprinting = cmd.sprint && p.carrying < 0;

    Vec2 dir{cmd.moveX, cmd.moveZ};
    if (dir.length() > 1e-6) {
        dir = dir.normalized();
        double speed = p.sprinting ? kSprintSpeed : kWalkSpeed;
        if (p.carrying >= 0) speed *= 0.8;
        Vec2 before = p.pos;
        moveOnGrid(p.pos, dir, speed, dt, dynamicGrid_.empty() ? grid_ : dynamicGrid_, size_);
        double maxDist = speed * dt * 1.25 + 0.06;
        if (p.pos.distance(before) > maxDist) {
            p.pos = before;
            ++rejects_;
        }
    }

    if (p.carrying >= 0) {
        for (auto& o : objects_) {
            if (o.id == p.carrying) {
                o.pos = p.pos + forward * 0.9;
                o.holder = p.id;
            }
        }
    }
}

void Sim::handleInteract(PlayerState& p) {
    SimObject* best = nullptr;
    double bestD = 1.9;
    for (auto& o : objects_) {
        if (o.id == p.carrying) continue;
        if (o.type == ObjType::Pickup && o.taken) continue;
        if (o.type == ObjType::Crate && o.holder >= 0 && o.holder != p.id) continue;
        if (o.type == ObjType::Crate && p.carrying >= 0) continue;
        double d = p.pos.distance(o.pos);
        if (d < bestD) {
            bestD = d;
            best = &o;
        }
    }

    if (!best) {
        if (p.carrying >= 0) {
            for (auto& o : objects_) {
                if (o.id == p.carrying) o.holder = -1;
            }
            p.carrying = -1;
        }
        return;
    }

    if (best->type == ObjType::Door) {
        best->open = !best->open;
    } else if (best->type == ObjType::Pickup) {
        best->taken = true;
        p.hp = std::min(100.0, p.hp + 40.0);
        p.ammo += 12;
    } else if (best->type == ObjType::Crate) {
        if (p.carrying < 0) {
            best->holder = p.id;
            p.carrying = best->id;
        } else {
            for (auto& o : objects_) {
                if (o.id == p.carrying) o.holder = -1;
            }
            p.carrying = -1;
        }
    }
}

void Sim::step(double dt) {
    if (dt <= 0.0) return;
    ++tick_;
    refreshDynamicBlocks();
    for (auto& p : players_) {
        if (!p.alive) continue;
        InputCmd& cmd = inputs_[static_cast<size_t>(p.id)];
        p.interact = cmd.interact;

        bool edge = cmd.interact && prevInteract_[static_cast<size_t>(p.id)] == 0;
        prevInteract_[static_cast<size_t>(p.id)] = cmd.interact ? 1 : 0;
        if (edge) {
            handleInteract(p);
        }
        p.prevPos = p.pos;
        movePlayer(p, cmd, dt);
        p.lastSpeed = p.pos.distance(p.prevPos) / dt;
    }
    updateMonsters(dt);
}
}
