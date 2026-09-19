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
    rng_.seed(seed ^ 0x5F3759DFu);

    bool valid = false;
    std::string reason;
    for (int attempt = 0; attempt < 8; ++attempt) {
        placeQuestObjects();
        if (validateLayout(reason)) {
            valid = true;
            break;
        }
    }
    if (!valid) {
        applyFallback();
        layoutReason_ = "fallback: " + reason;
    } else {
        layoutReason_ = "ok";
    }

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

void Sim::placeQuestObjects() {
    objects_.clear();
    vehicleId_ = -1;
    status_ = 0;
    int nextId = 1;

    for (int i = 0; i < 4; ++i) {
        Vec2 p = randomFreeSpot();
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Door;
        o.pos = p;
        objects_.push_back(o);
    }
    for (int i = 0; i < 6; ++i) {
        Vec2 p = randomFreeSpot();
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Crate;
        o.pos = p;
        objects_.push_back(o);
    }
    for (int i = 0; i < 6; ++i) {
        Vec2 p = randomFreeSpot();
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Pickup;
        o.pos = p;
        objects_.push_back(o);
    }
    for (int i = 0; i < kFuelCanTotal; ++i) {
        Vec2 p = randomFreeSpot();
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::FuelCan;
        o.pos = p;
        objects_.push_back(o);
    }
    for (int i = 0; i < 2; ++i) {
        Vec2 p = randomFreeSpot();
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Generator;
        o.pos = p;
        objects_.push_back(o);
    }
    {
        Vec2 p = randomFreeSpot();
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Vehicle;
        o.pos = p;
        objects_.push_back(o);
        vehicleId_ = o.id;
    }
}

bool Sim::reachableFrom(const Vec2& start, const Vec2& goal,
                        const std::vector<unsigned char>& grid) const {
    int sx = static_cast<int>(std::floor(start.x));
    int sz = static_cast<int>(std::floor(start.z));
    int gx = static_cast<int>(std::floor(goal.x));
    int gz = static_cast<int>(std::floor(goal.z));
    if (sx < 0 || sz < 0 || sx >= size_ || sz >= size_) return false;
    if (gx < 0 || gz < 0 || gx >= size_ || gz >= size_) return false;

    std::vector<unsigned char> seen(static_cast<size_t>(size_) * static_cast<size_t>(size_), 0);
    std::vector<int> queue;
    queue.push_back(sz * size_ + sx);
    seen[static_cast<size_t>(sz) * static_cast<size_t>(size_) + static_cast<size_t>(sx)] = 1;
    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    size_t head = 0;
    while (head < queue.size()) {
        int cur = queue[head++];
        int cx = cur % size_;
        int cz = cur / size_;
        if (cx == gx && cz == gz) return true;
        for (const auto& d : dirs) {
            int nx = cx + d[0];
            int nz = cz + d[1];
            if (nx < 0 || nz < 0 || nx >= size_ || nz >= size_) continue;
            if (grid[static_cast<size_t>(nz) * static_cast<size_t>(size_) + static_cast<size_t>(nx)] != 0) {
                continue;
            }
            size_t idx = static_cast<size_t>(nz) * static_cast<size_t>(size_) + static_cast<size_t>(nx);
            if (seen[idx]) continue;
            seen[idx] = 1;
            queue.push_back(nz * size_ + nx);
        }
    }
    return false;
}

bool Sim::validateLayout(std::string& reason) const {
    int required = kGeneratorFuelNeed * 2 + kVehicleFuelNeed;
    if (fuelCansInWorld() < (required * 13) / 10 + 1) {
        reason = "fuel redundancy below 1.3x";
        return false;
    }

    std::vector<unsigned char> passable = grid_;
    for (const auto& o : objects_) {
        if (o.type != ObjType::Door) continue;
        int gx = static_cast<int>(std::floor(o.pos.x));
        int gz = static_cast<int>(std::floor(o.pos.z));
        if (gx < 0 || gz < 0 || gx >= size_ || gz >= size_) continue;
        passable[static_cast<size_t>(gz) * static_cast<size_t>(size_) + static_cast<size_t>(gx)] = 0;
    }

    const SimObject* vehicle = nullptr;
    for (const auto& o : objects_) {
        if (o.type == ObjType::Vehicle) vehicle = &o;
    }
    if (!vehicle) {
        reason = "no vehicle";
        return false;
    }
    for (const auto& s : spawns_) {
        if (!reachableFrom(s, vehicle->pos, passable)) {
            reason = "spawn cannot reach vehicle";
            return false;
        }
    }
    for (const auto& o : objects_) {
        if (o.type != ObjType::Generator && o.type != ObjType::FuelCan) continue;
        bool anySpawn = false;
        for (const auto& s : spawns_) {
            if (reachableFrom(s, o.pos, passable)) {
                anySpawn = true;
                break;
            }
        }
        if (!anySpawn) {
            reason = "objective unreachable";
            return false;
        }
    }
    return true;
}

void Sim::applyFallback() {
    objects_.clear();
    vehicleId_ = -1;
    status_ = 0;
    int nextId = 1;
    for (int i = 0; i < 6; ++i) {
        Vec2 p = randomFreeSpot();
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Crate;
        o.pos = p;
        objects_.push_back(o);
    }
    for (int i = 0; i < kFuelCanTotal; ++i) {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::FuelCan;
        o.pos = spawns_.empty() ? randomFreeSpot()
                                : spawns_[static_cast<size_t>(i) % spawns_.size()];
        objects_.push_back(o);
    }
    for (int i = 0; i < 2; ++i) {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Generator;
        o.pos = spawns_.empty() ? randomFreeSpot()
                                : spawns_[static_cast<size_t>(i + 2) % spawns_.size()];
        objects_.push_back(o);
    }
    {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Vehicle;
        o.pos = spawns_.empty() ? Vec2{size_ / 2.0, size_ / 2.0} : spawns_[0];
        objects_.push_back(o);
        vehicleId_ = o.id;
    }
}

bool Sim::generatorsPowered() const {
    int gens = 0;
    for (const auto& o : objects_) {
        if (o.type != ObjType::Generator) continue;
        ++gens;
        if (o.charge < kGeneratorFuelNeed) return false;
    }
    return gens > 0;
}

int Sim::fuelCansInWorld() const {
    int count = 0;
    for (const auto& o : objects_) {
        if (o.type == ObjType::FuelCan && !o.taken) ++count;
    }
    return count;
}

void Sim::debugTeleportPlayer(int id, double x, double z) {
    if (id < 0 || id >= static_cast<int>(players_.size())) return;
    players_[static_cast<size_t>(id)].pos = Vec2{x, z};
}

void Sim::updateObjectives(double dt) {
    const SimObject* vehicle = nullptr;
    for (const auto& o : objects_) {
        if (o.type == ObjType::Vehicle) vehicle = &o;
    }

    for (auto& p : players_) {
        if (!p.alive || p.extracted) continue;
        InputCmd& cmd = inputs_[static_cast<size_t>(p.id)];
        int carriedCan = -1;
        for (const auto& o : objects_) {
            if (o.id == p.carrying && o.type == ObjType::FuelCan && !o.taken) carriedCan = o.id;
        }

        if (cmd.interact && carriedCan >= 0) {
            for (auto& g : objects_) {
                if (g.type != ObjType::Generator || g.charge >= kGeneratorFuelNeed) continue;
                if (p.pos.distance(g.pos) > 1.9) continue;
                g.progress += static_cast<float>(dt);
                if (g.progress >= kChannelTime) {
                    g.progress = 0.0f;
                    g.charge += 1;
                    for (auto& o : objects_) {
                        if (o.id == carriedCan) {
                            o.taken = true;
                            o.holder = -1;
                        }
                    }
                    p.carrying = -1;
                }
                break;
            }
            if (generatorsPowered() && vehicle && vehicle->charge < kVehicleFuelNeed &&
                p.pos.distance(vehicle->pos) <= 2.2) {
                SimObject* v = nullptr;
                for (auto& o : objects_) {
                    if (o.type == ObjType::Vehicle) v = &o;
                }
                if (v) {
                    v->progress += static_cast<float>(dt);
                    if (v->progress >= kChannelTime) {
                        v->progress = 0.0f;
                        v->charge += 1;
                        for (auto& o : objects_) {
                            if (o.id == carriedCan) {
                                o.taken = true;
                                o.holder = -1;
                            }
                        }
                        p.carrying = -1;
                    }
                }
            }
        }

        if (vehicle && vehicle->charge >= kVehicleFuelNeed &&
            p.pos.distance(vehicle->pos) <= kExtractRange) {
            p.extractTimer += dt;
            if (p.extractTimer >= kExtractTime) {
                p.extracted = true;
            }
        } else {
            p.extractTimer = 0.0;
        }
    }

    bool anyActive = false;
    for (const auto& p : players_) {
        if (p.alive && !p.extracted) anyActive = true;
    }
    if (!anyActive && !players_.empty() && status_ == 0) {
        bool anyExtracted = false;
        for (const auto& p : players_) {
            if (p.extracted) anyExtracted = true;
        }
        if (anyExtracted) status_ = 1;
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
        if (o.type == ObjType::FuelCan && o.taken) continue;
        if ((o.type == ObjType::Crate || o.type == ObjType::FuelCan) && o.holder >= 0 &&
            o.holder != p.id) {
            continue;
        }
        if ((o.type == ObjType::Crate || o.type == ObjType::FuelCan) && p.carrying >= 0) continue;
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
    } else if (best->type == ObjType::Crate || best->type == ObjType::FuelCan) {
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
    updateObjectives(dt);
    updateMonsters(dt);
}
}
