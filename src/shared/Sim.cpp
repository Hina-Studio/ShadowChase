#include "shared/Sim.hpp"

#include <algorithm>
#include <cmath>
#include <queue>
#include <random>

#include "shared/MapData.hpp"

namespace sc {
namespace {
constexpr int kDir4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

int cellOf(double v) {
    return static_cast<int>(std::floor(v / kCellSize));
}

Vec2 centerOf(int c, int r) {
    return Vec2{static_cast<double>(c) * kCellSize + kCellSize * 0.5,
                static_cast<double>(r) * kCellSize + kCellSize * 0.5};
}
}

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

int Sim::roomIdAt(const Vec2& p) const {
    int c = cellOf(p.x);
    int r = cellOf(p.z);
    if (c < 0 || r < 0 || c >= cols_ || r >= rows_) return -1;
    return room_[static_cast<size_t>(r) * static_cast<size_t>(cols_) + static_cast<size_t>(c)];
}

bool Sim::blockedCell(const std::vector<unsigned char>& grid, int cols, int rows, double x,
                      double z) {
    int c = cellOf(x);
    int r = cellOf(z);
    if (c < 0 || r < 0 || c >= cols || r >= rows) return true;
    return grid[static_cast<size_t>(r) * static_cast<size_t>(cols) + static_cast<size_t>(c)] != 0;
}

void Sim::moveOnGrid(Vec2& pos, const Vec2& dir, double speed, double dt,
                     const std::vector<unsigned char>& grid, int cols, int rows) {
    double r = kPlayerRadius;
    auto freeAt = [&](double x, double z) {
        return !blockedCell(grid, cols, rows, x - r, z - r) &&
               !blockedCell(grid, cols, rows, x + r, z - r) &&
               !blockedCell(grid, cols, rows, x - r, z + r) &&
               !blockedCell(grid, cols, rows, x + r, z + r);
    };
    double nx = pos.x + dir.x * speed * dt;
    if (freeAt(nx, pos.z)) pos.x = nx;
    double nz = pos.z + dir.z * speed * dt;
    if (freeAt(pos.x, nz)) pos.z = nz;
}

void Sim::generate(unsigned int seed) {
    seed_ = seed;
    tick_ = 0;
    rejects_ = 0;
    blocks_.clear();
    spawns_.clear();
    players_.clear();
    inputs_.clear();
    prevInteract_.clear();
    prevHeldInteract_.clear();
    prevButtons_.clear();
    objects_.clear();
    monsters_.clear();
    noises_.clear();
    matchTime_ = 0.0;
    rageActive_ = false;
    filesRequired_ = 0;
    helicopterSpawned_ = false;
    status_ = 0;
    vehicleId_ = -1;
    activeExit_ = -1;

    const auto& rows = maloneFarmMap();
    rows_ = static_cast<int>(rows.size());
    cols_ = 0;
    for (const auto& line : rows) {
        cols_ = std::max(cols_, static_cast<int>(line.size()));
    }
    grid_.assign(static_cast<size_t>(cols_) * static_cast<size_t>(rows_), 1);
    grass_.assign(grid_.size(), 0);
    room_.assign(grid_.size(), -1);
    genCandidates_.clear();
    lootCandidates_.clear();
    trapCandidates_.clear();
    exitCandidates_.clear();
    spawnCandidates_.clear();

    for (int r = 0; r < rows_; ++r) {
        const std::string& line = rows[static_cast<size_t>(r)];
        for (int c = 0; c < cols_; ++c) {
            char ch = c < static_cast<int>(line.size()) ? line[static_cast<size_t>(c)] : '#';
            size_t idx = static_cast<size_t>(r) * static_cast<size_t>(cols_) + static_cast<size_t>(c);
            Vec2 center = centerOf(c, r);
            switch (ch) {
                case '#':
                    grid_[idx] = 1;
                    blocks_.push_back(Block{c, r});
                    break;
                case '~':
                    grid_[idx] = 0;
                    grass_[idx] = 1;
                    break;
                case '.':
                    grid_[idx] = 0;
                    break;
                case 'D': {
                    grid_[idx] = 0;
                    SimObject d;
                    d.id = 0;
                    d.type = ObjType::Door;
                    d.pos = center;
                    d.open = true;
                    objects_.push_back(d);
                    break;
                }
                case 'G':
                    grid_[idx] = 0;
                    genCandidates_.push_back(center);
                    break;
                case 'L':
                    grid_[idx] = 0;
                    lootCandidates_.push_back(center);
                    break;
                case 'T':
                    grid_[idx] = 0;
                    trapCandidates_.push_back(center);
                    break;
                case 'E':
                    grid_[idx] = 0;
                    exitCandidates_.push_back(center);
                    break;
                case 'S':
                    grid_[idx] = 0;
                    spawnCandidates_.push_back(center);
                    break;
                case 'W':
                    grid_[idx] = 0;
                    break;
                default:
                    grid_[idx] = 0;
                    break;
            }
        }
    }

    for (const auto& s : spawnCandidates_) {
        spawns_.push_back(s);
    }

    buildRooms();
    rng_.seed(seed ^ 0x5F3759DFu);
    actionRng_.seed(seed ^ 0x00C0FFEEu);

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

    Vec2 farCell = spawns_.empty() ? centerOf(cols_ / 2, rows_ / 2) : spawns_[0];
    double bestD = -1.0;
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            size_t idx = static_cast<size_t>(r) * static_cast<size_t>(cols_) + static_cast<size_t>(c);
            if (grid_[idx] != 0) continue;
            Vec2 p = centerOf(c, r);
            double d = p.distance(spawns_.empty() ? p : spawns_[0]);
            if (d > bestD) {
                bestD = d;
                farCell = p;
            }
        }
    }
    MonsterState m;
    m.id = 0;
    m.pos = farCell;
    m.patrolTarget = farCell;
    monsters_.push_back(m);
}

void Sim::buildRooms() {
    room_.assign(static_cast<size_t>(cols_) * static_cast<size_t>(rows_), -1);
    int nextRoom = 0;
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            size_t idx = static_cast<size_t>(r) * static_cast<size_t>(cols_) + static_cast<size_t>(c);
            if (grid_[idx] != 0 || room_[idx] != -1) continue;
            bool doorCell = false;
            for (const auto& o : objects_) {
                if (o.type != ObjType::Door) continue;
                if (cellOf(o.pos.x) == c && cellOf(o.pos.z) == r) doorCell = true;
            }
            if (doorCell) {
                room_[idx] = -2;
                continue;
            }
            std::vector<int> queue;
            queue.push_back(r * cols_ + c);
            room_[idx] = nextRoom;
            size_t head = 0;
            while (head < queue.size()) {
                int cur = queue[head++];
                int cx = cur % cols_;
                int cz = cur / cols_;
                for (const auto& d : kDir4) {
                    int nx = cx + d[0];
                    int nz = cz + d[1];
                    if (nx < 0 || nz < 0 || nx >= cols_ || nz >= rows_) continue;
                    size_t nidx = static_cast<size_t>(nz) * static_cast<size_t>(cols_) + static_cast<size_t>(nx);
                    if (grid_[nidx] != 0 || room_[nidx] != -1) continue;
                    bool ndoor = false;
                    for (const auto& o : objects_) {
                        if (o.type != ObjType::Door) continue;
                        if (cellOf(o.pos.x) == nx && cellOf(o.pos.z) == nz) ndoor = true;
                    }
                    if (ndoor) {
                        room_[nidx] = -2;
                        continue;
                    }
                    room_[nidx] = nextRoom;
                    queue.push_back(nz * cols_ + nx);
                }
            }
            ++nextRoom;
        }
    }
}

bool Sim::passableCell(int c, int r) const {
    if (c < 0 || r < 0 || c >= cols_ || r >= rows_) return false;
    size_t idx = static_cast<size_t>(r) * static_cast<size_t>(cols_) + static_cast<size_t>(c);
    const std::vector<unsigned char>& g = dynamicGrid_.empty() ? grid_ : dynamicGrid_;
    return g[idx] == 0;
}

std::vector<Vec2> Sim::findPath(const Vec2& from, const Vec2& to) const {
    std::vector<Vec2> out;
    int sc = cellOf(from.x);
    int sr = cellOf(from.z);
    int tc = cellOf(to.x);
    int tr = cellOf(to.z);
    if (sc == tc && sr == tr) return out;
    if (!passableCell(tc, tr)) return out;

    const int total = cols_ * rows_;
    std::vector<int> came(static_cast<size_t>(total), -1);
    std::vector<double> gScore(static_cast<size_t>(total), 1e18);
    auto heur = [&](int c, int r) {
        return static_cast<double>(std::abs(c - tc) + std::abs(r - tr));
    };
    struct Node {
        double f;
        int id;
        bool operator<(const Node& o) const { return f > o.f; }
    };
    std::priority_queue<Node> open;
    int startId = sr * cols_ + sc;
    int goalId = tr * cols_ + tc;
    gScore[static_cast<size_t>(startId)] = 0.0;
    came[static_cast<size_t>(startId)] = startId;
    open.push(Node{heur(sc, sr), startId});

    while (!open.empty()) {
        Node cur = open.top();
        open.pop();
        if (cur.id == goalId) break;
        int cx = cur.id % cols_;
        int cz = cur.id / cols_;
        for (const auto& d : kDir4) {
            int nx = cx + d[0];
            int nz = cz + d[1];
            if (!passableCell(nx, nz)) continue;
            int nid = nz * cols_ + nx;
            double ng = gScore[static_cast<size_t>(cur.id)] + 1.0;
            if (ng >= gScore[static_cast<size_t>(nid)]) continue;
            gScore[static_cast<size_t>(nid)] = ng;
            came[static_cast<size_t>(nid)] = cur.id;
            open.push(Node{ng + heur(nx, nz), nid});
        }
    }
    if (came[static_cast<size_t>(goalId)] == -1) return out;

    std::vector<int> rev;
    for (int id = goalId; id != startId; id = came[static_cast<size_t>(id)]) {
        rev.push_back(id);
    }
    std::reverse(rev.begin(), rev.end());
    for (int id : rev) {
        out.push_back(centerOf(id % cols_, id / cols_));
    }
    return out;
}

bool Sim::reachableFrom(const Vec2& start, const Vec2& goal,
                        const std::vector<unsigned char>& grid) const {
    int sc = cellOf(start.x);
    int sr = cellOf(start.z);
    int tc = cellOf(goal.x);
    int tr = cellOf(goal.z);
    if (sc < 0 || sr < 0 || sc >= cols_ || sr >= rows_) return false;
    if (tc < 0 || tr < 0 || tc >= cols_ || tr >= rows_) return false;
    if (grid[static_cast<size_t>(sr) * static_cast<size_t>(cols_) + static_cast<size_t>(sc)] != 0) {
        return false;
    }

    std::vector<unsigned char> seen(static_cast<size_t>(cols_) * static_cast<size_t>(rows_), 0);
    std::vector<int> queue;
    queue.push_back(sr * cols_ + sc);
    seen[static_cast<size_t>(sr) * static_cast<size_t>(cols_) + static_cast<size_t>(sc)] = 1;
    size_t head = 0;
    while (head < queue.size()) {
        int cur = queue[head++];
        int cx = cur % cols_;
        int cz = cur / cols_;
        if (cx == tc && cz == tr) return true;
        for (const auto& d : kDir4) {
            int nx = cx + d[0];
            int nz = cz + d[1];
            if (nx < 0 || nz < 0 || nx >= cols_ || nz >= rows_) continue;
            size_t nidx = static_cast<size_t>(nz) * static_cast<size_t>(cols_) + static_cast<size_t>(nx);
            if (seen[nidx] || grid[nidx] != 0) continue;
            seen[nidx] = 1;
            queue.push_back(nz * cols_ + nx);
        }
    }
    return false;
}

void Sim::placeQuestObjects() {
    std::vector<SimObject> doors;
    std::vector<SimObject> towers;
    for (const auto& o : objects_) {
        if (o.type == ObjType::Door) doors.push_back(o);
        if (o.type == ObjType::WaterTower) towers.push_back(o);
    }
    objects_.clear();
    for (auto& d : doors) {
        d.open = true;
        d.phase = ((actionRng_() % 100) < 25) ? 1 : 0;
        objects_.push_back(d);
    }
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            const std::string& line = maloneFarmMap()[static_cast<size_t>(r)];
            if (c < static_cast<int>(line.size()) && line[static_cast<size_t>(c)] == 'W') {
                SimObject w;
                w.id = 0;
                w.type = ObjType::WaterTower;
                w.pos = centerOf(c, r);
                objects_.push_back(w);
            }
        }
    }
    (void)towers;

    std::vector<Vec2> gens = genCandidates_;
    std::shuffle(gens.begin(), gens.end(), rng_);
    if (static_cast<int>(gens.size()) > kActiveGenerators) gens.resize(kActiveGenerators);

    std::vector<Vec2> loot = lootCandidates_;
    std::shuffle(loot.begin(), loot.end(), rng_);
    size_t lootIdx = 0;
    auto nextLoot = [&]() -> Vec2 {
        if (loot.empty()) return centerOf(cols_ / 2, rows_ / 2);
        Vec2 p = loot[lootIdx % loot.size()];
        ++lootIdx;
        return p;
    };

    if (!exitCandidates_.empty()) {
        activeExit_ = static_cast<int>(actionRng_() % exitCandidates_.size());
    }

    int nextId = 1;
    for (const auto& g : gens) {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Generator;
        o.pos = g;
        objects_.push_back(o);
    }
    for (int i = 0; i < kFuelCanTotal; ++i) {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::FuelCan;
        o.pos = nextLoot();
        objects_.push_back(o);
    }
    for (int i = 0; i < kBatteryTotal; ++i) {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Battery;
        o.pos = nextLoot();
        objects_.push_back(o);
    }
    for (int i = 0; i < kFileTotal; ++i) {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::File;
        o.pos = nextLoot();
        objects_.push_back(o);
    }
    for (int i = 0; i < 4; ++i) {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Crate;
        o.pos = nextLoot();
        objects_.push_back(o);
    }
    for (int i = 0; i < 4; ++i) {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Pickup;
        o.pos = nextLoot();
        objects_.push_back(o);
    }
    for (int i = 0; i < kMasterLockTotal; ++i) {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::MasterLock;
        o.pos = nextLoot();
        objects_.push_back(o);
    }
    for (const auto& t : trapCandidates_) {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Trap;
        o.pos = t;
        o.taken = false;
        objects_.push_back(o);
    }
}

bool Sim::validateLayout(std::string& reason) const {
    int required = kGeneratorFuelNeed * kActiveGenerators;
    int fuel = 0;
    int battery = 0;
    int file = 0;
    for (const auto& o : objects_) {
        if (o.taken) continue;
        if (o.type == ObjType::FuelCan) ++fuel;
        if (o.type == ObjType::Battery) ++battery;
        if (o.type == ObjType::File) ++file;
    }
    if (fuel < (required * 13) / 10 + 1) {
        reason = "fuel redundancy below 1.3x";
        return false;
    }
    if (battery < (kActiveGenerators * 13) / 10 + 1) {
        reason = "battery redundancy below 1.3x";
        return false;
    }
    if (file < kFilesForBigTeam) {
        reason = "not enough files";
        return false;
    }
    if (activeExit_ < 0 || exitCandidates_.empty()) {
        reason = "no exit";
        return false;
    }
    Vec2 exitPos = exitCandidates_[static_cast<size_t>(activeExit_)];

    std::vector<unsigned char> passable = grid_;
    for (const auto& o : objects_) {
        if (o.type != ObjType::Door) continue;
        int c = cellOf(o.pos.x);
        int r = cellOf(o.pos.z);
        if (c < 0 || r < 0 || c >= cols_ || r >= rows_) continue;
        passable[static_cast<size_t>(r) * static_cast<size_t>(cols_) + static_cast<size_t>(c)] = 0;
    }
    for (const auto& s : spawns_) {
        if (!reachableFrom(s, exitPos, passable)) {
            reason = "spawn cannot reach exit";
            return false;
        }
    }
    for (const auto& o : objects_) {
        if (o.type != ObjType::Generator && o.type != ObjType::FuelCan &&
            o.type != ObjType::Battery && o.type != ObjType::File) {
            continue;
        }
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
    std::vector<SimObject> keep;
    for (const auto& o : objects_) {
        if (o.type == ObjType::Door || o.type == ObjType::WaterTower) {
            SimObject d = o;
            if (d.type == ObjType::Door) {
                d.phase = 0;
                d.open = true;
            }
            keep.push_back(d);
        }
    }
    objects_ = keep;
    int nextId = 1;
    for (auto& o : objects_) {
        o.id = nextId++;
    }
    Vec2 base = spawns_.empty() ? centerOf(cols_ / 2, rows_ / 2) : spawns_[0];

    std::vector<Vec2> freeCells;
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            if (grid_[static_cast<size_t>(r) * static_cast<size_t>(cols_) + static_cast<size_t>(c)] != 0) continue;
            freeCells.push_back(centerOf(c, r));
        }
    }
    std::sort(freeCells.begin(), freeCells.end(), [&](const Vec2& a, const Vec2& b) {
        return a.distance(base) < b.distance(base);
    });
    size_t idx = 0;
    auto takeCell = [&]() -> Vec2 {
        if (freeCells.empty()) return base;
        Vec2 p = freeCells[idx % freeCells.size()];
        ++idx;
        return p;
    };

    for (int i = 0; i < kActiveGenerators; ++i) {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Generator;
        o.pos = takeCell();
        objects_.push_back(o);
    }
    for (int i = 0; i < kFuelCanTotal; ++i) {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::FuelCan;
        o.pos = takeCell();
        objects_.push_back(o);
    }
    for (int i = 0; i < kBatteryTotal; ++i) {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Battery;
        o.pos = takeCell();
        objects_.push_back(o);
    }
    for (int i = 0; i < kFileTotal; ++i) {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::File;
        o.pos = takeCell();
        objects_.push_back(o);
    }
    for (int i = 0; i < kMasterLockTotal; ++i) {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::MasterLock;
        o.pos = takeCell();
        objects_.push_back(o);
    }
    for (const auto& t : trapCandidates_) {
        SimObject o;
        o.id = nextId++;
        o.type = ObjType::Trap;
        o.pos = t;
        objects_.push_back(o);
    }
    activeExit_ = 0;
    if (!exitCandidates_.empty()) {
        exitCandidates_[0] = base;
    }
}

void Sim::refreshDynamicBlocks() {
    dynamicGrid_ = grid_;
    for (const auto& o : objects_) {
        if (o.type != ObjType::Door) continue;
        if (o.open && o.phase != 1) continue;
        int c = cellOf(o.pos.x);
        int r = cellOf(o.pos.z);
        if (c < 0 || r < 0 || c >= cols_ || r >= rows_) continue;
        dynamicGrid_[static_cast<size_t>(r) * static_cast<size_t>(cols_) + static_cast<size_t>(c)] = 1;
    }
}

bool Sim::hasLineOfSight(const Vec2& a, const Vec2& b) const {
    const std::vector<unsigned char>& g = dynamicGrid_.empty() ? grid_ : dynamicGrid_;
    Vec2 d = b - a;
    double dist = d.length();
    int steps = static_cast<int>(dist / 0.5) + 1;
    for (int i = 1; i < steps; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(steps);
        Vec2 p = a + d * t;
        if (blockedCell(g, cols_, rows_, p.x, p.z)) return false;
    }
    return true;
}

void Sim::addNoise(const Vec2& pos) {
    Noise n;
    n.pos = pos;
    n.room = roomIdAt(pos);
    noises_.push_back(n);
}

Vec2 Sim::randomFreeSpot() {
    if (!lootCandidates_.empty()) {
        return lootCandidates_[actionRng_() % lootCandidates_.size()];
    }
    return centerOf(cols_ / 2, rows_ / 2);
}

int Sim::addPlayer(const std::string& name) {
    PlayerState p;
    p.id = static_cast<int>(players_.size());
    p.name = name.empty() ? ("P" + std::to_string(p.id + 1)) : name;
    if (!spawns_.empty()) {
        p.pos = spawns_[static_cast<size_t>(p.id) % spawns_.size()];
    } else {
        p.pos = centerOf(cols_ / 2, rows_ / 2);
    }
    p.prevPos = p.pos;
    players_.push_back(p);
    inputs_.push_back(InputCmd{});
    prevInteract_.push_back(0);
    prevHeldInteract_.push_back(0);
    prevButtons_.push_back(0);
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

    if (p.rootTimer > 0.0) {
        p.rootTimer -= dt;
        if (p.carrying >= 0) {
            for (auto& o : objects_) {
                if (o.id == p.carrying) {
                    o.pos = p.pos + forward * 0.9;
                    o.holder = p.id;
                }
            }
        }
        return;
    }

    Vec2 dir{cmd.moveX, cmd.moveZ};
    if (dir.length() > 1e-6) {
        dir = dir.normalized();
        double speed = p.sprinting ? kSprintSpeed : kWalkSpeed;
        if (p.carrying >= 0) speed *= 0.8;
        if (p.crouched) speed *= kCrouchSpeedScale;
        int pc = cellOf(p.pos.x);
        int pr = cellOf(p.pos.z);
        if (pc >= 0 && pr >= 0 && pc < cols_ && pr < rows_ &&
            grass_[static_cast<size_t>(pr) * static_cast<size_t>(cols_) + static_cast<size_t>(pc)] != 0) {
            speed *= kGrassSpeedMul;
        }
        Vec2 before = p.pos;
        const std::vector<unsigned char>& g = dynamicGrid_.empty() ? grid_ : dynamicGrid_;
        moveOnGrid(p.pos, dir, speed, dt, g, cols_, rows_);
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
    double bestD = p.crouched ? kReachCrouched : kReachStanding;
    for (auto& o : objects_) {
        if (o.id == p.carrying) continue;
        if (o.holder == -2) continue;
        if (o.type == ObjType::Generator || o.type == ObjType::Vehicle ||
            o.type == ObjType::WaterTower || o.type == ObjType::Trap) {
            continue;
        }
        if (o.type == ObjType::Pickup && o.taken) continue;
        if ((o.type == ObjType::FuelCan || o.type == ObjType::Battery ||
             o.type == ObjType::MasterLock) &&
            o.taken) {
            continue;
        }
        bool carryable = o.type == ObjType::Crate || o.type == ObjType::FuelCan ||
                         o.type == ObjType::Battery || o.type == ObjType::MasterLock;
        if (carryable && o.holder >= 0 && o.holder != p.id) continue;
        if (carryable && p.carrying >= 0) continue;
        double d = p.pos.distance(o.pos);
        if (d < bestD) {
            bestD = d;
            best = &o;
        }
    }

    if (!best) return;

    if (best->type == ObjType::Door) {
        if (best->phase == 1) {
            if (p.carrying >= 0) {
                SimObject* lockObj = nullptr;
                for (auto& o : objects_) {
                    if (o.id == p.carrying && o.type == ObjType::MasterLock) lockObj = &o;
                }
                if (lockObj) {
                    lockObj->taken = true;
                    lockObj->holder = -1;
                    p.carrying = -1;
                    if ((actionRng_() % 100) < 70) {
                        best->phase = 0;
                        best->open = true;
                    }
                    addNoise(p.pos);
                }
            }
            return;
        }
        best->open = !best->open;
        addNoise(p.pos);
    } else if (best->type == ObjType::Pickup) {
        best->taken = true;
        p.hp = std::min(100.0, p.hp + 40.0);
        p.ammo += 12;
        addNoise(p.pos);
    } else if (best->type == ObjType::File) {
        best->taken = true;
        p.files += 1;
        addNoise(p.pos);
    } else if (best->type == ObjType::Crate || best->type == ObjType::FuelCan ||
               best->type == ObjType::Battery || best->type == ObjType::MasterLock) {
        if (p.carrying < 0) {
            best->holder = p.id;
            p.carrying = best->id;
        } else {
            for (auto& o : objects_) {
                if (o.id == p.carrying) o.holder = -1;
            }
            p.carrying = -1;
        }
        addNoise(p.pos);
    }
}

void Sim::handleActions(PlayerState& p, uint8_t pressed, const InputCmd& cmd) {
    auto dropCarried = [&](double dist) {
        if (p.carrying < 0) return;
        Vec2 f{std::sin(cmd.yaw), std::cos(cmd.yaw)};
        for (auto& o : objects_) {
            if (o.id != p.carrying) continue;
            Vec2 place = p.pos;
            const std::vector<unsigned char>& g = dynamicGrid_.empty() ? grid_ : dynamicGrid_;
            for (double d = dist; d >= 0.0; d -= 2.0) {
                Vec2 cand = p.pos + f * d;
                if (!blockedCell(g, cols_, rows_, cand.x, cand.z)) {
                    place = cand;
                    break;
                }
            }
            o.pos = place;
            o.holder = -1;
            addNoise(place);
        }
        p.carrying = -1;
    };

    if (pressed & 0x01) dropCarried(1.2);
    if (pressed & 0x02) dropCarried(kThrowDistance);
    if (pressed & 0x04) {
        if (p.carrying >= 0) {
            int slot = p.stash0 < 0 ? 0 : (p.stash1 < 0 ? 1 : -1);
            if (slot == 0) {
                p.stash0 = p.carrying;
            } else if (slot == 1) {
                p.stash1 = p.carrying;
            }
            if (slot >= 0) {
                for (auto& o : objects_) {
                    if (o.id == p.carrying) o.holder = -2;
                }
                p.carrying = -1;
            }
        }
    }
    if (pressed & 0x08) {
        if (p.carrying < 0) {
            int id = p.stash0 >= 0 ? p.stash0 : p.stash1;
            if (id >= 0) {
                for (auto& o : objects_) {
                    if (o.id == id) o.holder = p.id;
                }
                p.carrying = id;
                if (p.stash0 == id) {
                    p.stash0 = -1;
                } else {
                    p.stash1 = -1;
                }
            }
        }
    }
    if (pressed & 0x10) {
        dropCarried(1.2);
        int stashIds[2] = {p.stash0, p.stash1};
        for (int id : stashIds) {
            if (id < 0) continue;
            for (auto& o : objects_) {
                if (o.id == id) {
                    o.holder = -1;
                    o.pos = p.pos;
                }
            }
            addNoise(p.pos);
        }
        p.stash0 = -1;
        p.stash1 = -1;
    }
}

void Sim::updateTraps(double dt) {
    (void)dt;
    for (auto& t : objects_) {
        if (t.type != ObjType::Trap || t.taken) continue;
        for (auto& p : players_) {
            if (!p.alive || p.extracted) continue;
            if (p.pos.distance(t.pos) > kTrapTriggerRange) continue;
            t.taken = true;
            p.hp = std::max(1.0, p.hp - kTrapDamage);
            p.rootTimer = kTrapRootTime;
            addNoise(p.pos);
            break;
        }
    }
}

void Sim::updateAnger(double dt) {
    for (auto& m : monsters_) {
        double rate = rageActive_ ? 2.5 : 0.35;
        m.anger = std::min(100, m.anger + static_cast<int>(rate * dt));
        if (rageActive_ && m.anger < 80) m.anger = 80;
    }
}

void Sim::updateObjectives(double dt) {
    matchTime_ += dt;
    if (!rageActive_ && matchTime_ >= kRageTime) {
        rageActive_ = true;
    }
    if (matchTime_ >= kMatchTimeLimit) {
        status_ = -1;
    }
    updateAnger(dt);
    updateTraps(dt);

    int need = requiredFuelPerGenerator();
    if (players_.size() >= 4 && filesRequired_ == 0) {
        filesRequired_ = kFilesForBigTeam;
    }

    if (!helicopterSpawned_ && generatorsPowered() && filesFound() >= filesRequired_ &&
        activeExit_ >= 0) {
        SimObject v;
        v.id = 9000;
        v.type = ObjType::Vehicle;
        v.pos = exitCandidates_[static_cast<size_t>(activeExit_)];
        v.phase = 1;
        objects_.push_back(v);
        vehicleId_ = v.id;
        helicopterSpawned_ = true;
        addNoise(v.pos);
    }

    for (auto& p : players_) {
        if (!p.alive || p.extracted) continue;
        InputCmd& cmd = inputs_[static_cast<size_t>(p.id)];
        bool held = cmd.interact;
        bool pressed = held && prevHeldInteract_[static_cast<size_t>(p.id)] == 0;
        prevHeldInteract_[static_cast<size_t>(p.id)] = held ? 1 : 0;

        int carrying = p.carrying;
        ObjType carryType = ObjType::Door;
        for (const auto& o : objects_) {
            if (o.id == carrying) carryType = o.type;
        }

        for (auto& g : objects_) {
            if (g.type != ObjType::Generator) continue;
            if (p.pos.distance(g.pos) > 2.4) continue;

            if (g.charge < need && carryType == ObjType::FuelCan) {
                if (!held) {
                    if (g.op == p.id) g.op = -1;
                } else if (g.op == -1 || g.op == p.id) {
                    g.op = p.id;
                    if (p.lastSpeed > kSpillMoveThreshold) {
                        for (auto& o : objects_) {
                            if (o.id == carrying) {
                                o.holder = -1;
                                o.pos = p.pos;
                            }
                        }
                        p.carrying = -1;
                        g.progress = 0.0f;
                        g.op = -1;
                        addNoise(p.pos);
                    } else {
                        g.progress += static_cast<float>(dt);
                        if (g.progress >= kChannelTime) {
                            g.progress = 0.0f;
                            g.charge += 1;
                            for (auto& o : objects_) {
                                if (o.id == carrying) {
                                    o.taken = true;
                                    o.holder = -1;
                                }
                            }
                            p.carrying = -1;
                            g.op = -1;
                            addNoise(g.pos);
                        }
                    }
                }
                break;
            }

            if (g.charge >= need && g.aux < 1 && carryType == ObjType::Battery) {
                if (!held && g.phase == 0) {
                    if (g.op == p.id) g.op = -1;
                }
                if (g.phase == 0) {
                    if (held && (g.op == -1 || g.op == p.id)) {
                        g.op = p.id;
                        g.progress += static_cast<float>(dt);
                        if (g.progress >= kBatteryClampTime) {
                            g.phase = 1;
                            g.progress = 0.0f;
                        }
                    } else {
                        g.progress = 0.0f;
                    }
                } else if (g.phase == 1) {
                    g.progress += static_cast<float>(dt);
                    if (pressed) {
                        g.aux = 1;
                        g.phase = 0;
                        g.progress = 0.0f;
                        g.op = -1;
                        for (auto& o : objects_) {
                            if (o.id == carrying) {
                                o.taken = true;
                                o.holder = -1;
                            }
                        }
                        p.carrying = -1;
                        addNoise(g.pos);
                    } else if (g.progress >= kBatteryWindow) {
                        g.phase = 0;
                        g.progress = 0.0f;
                        g.op = -1;
                        p.hp = std::max(1.0, p.hp - 10.0);
                        addNoise(p.pos);
                    }
                }
                break;
            }
        }

        const SimObject* vehicle = nullptr;
        for (const auto& o : objects_) {
            if (o.type == ObjType::Vehicle) vehicle = &o;
        }
        if (vehicle && vehicle->phase == 1 && p.pos.distance(vehicle->pos) <= kExtractRange &&
            held) {
            p.extractTimer += dt;
            if (p.extractTimer >= kExtractTime) {
                p.extracted = true;
            }
        } else {
            p.extractTimer = 0.0;
        }
    }

    bool anyActive = false;
    bool anyExtracted = false;
    bool anyAlive = false;
    for (const auto& p : players_) {
        if (p.alive) anyAlive = true;
        if (p.alive && !p.extracted) anyActive = true;
        if (p.extracted) anyExtracted = true;
    }
    if (!anyActive && !players_.empty() && status_ == 0) {
        if (anyExtracted) {
            status_ = 1;
        } else if (!anyAlive) {
            status_ = -1;
        }
    }
}

bool Sim::generatorsPowered() const {
    int gens = 0;
    int need = requiredFuelPerGenerator();
    for (const auto& o : objects_) {
        if (o.type != ObjType::Generator) continue;
        ++gens;
        if (o.charge < need || o.aux < 1) return false;
    }
    return gens >= kActiveGenerators;
}

int Sim::requiredFuelPerGenerator() const {
    int n = static_cast<int>(players_.size());
    if (n <= 1) return 1;
    if (n == 2) return 2;
    if (n == 3) return 3;
    return kGeneratorFuelNeed;
}

int Sim::generatorsFueled() const {
    int count = 0;
    int need = requiredFuelPerGenerator();
    for (const auto& o : objects_) {
        if (o.type == ObjType::Generator && o.charge >= need) ++count;
    }
    return count;
}

int Sim::generatorsBatteries() const {
    int count = 0;
    for (const auto& o : objects_) {
        if (o.type == ObjType::Generator && o.aux >= 1) ++count;
    }
    return count;
}

int Sim::filesFound() const {
    int count = 0;
    for (const auto& o : objects_) {
        if (o.type == ObjType::File && o.taken) ++count;
    }
    return count;
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
    players_[static_cast<size_t>(id)].prevPos = Vec2{x, z};
    players_[static_cast<size_t>(id)].lastSpeed = 0.0;
}

void Sim::updateMonsters(double dt) {
    const std::vector<unsigned char>& g = dynamicGrid_.empty() ? grid_ : dynamicGrid_;
    for (auto& m : monsters_) {
        double sight = kMonsterSight * (1.0 + static_cast<double>(m.anger) / 400.0);
        int seen = -1;
        Vec2 seenPos;
        for (const auto& p : players_) {
            if (!p.alive) continue;
            Vec2 d = p.pos - m.pos;
            double dist = d.length();
            if (dist > sight) continue;
            bool moving = p.lastSpeed > kPlayerMovingThreshold;
            bool close = dist <= kMonsterCloseRange;
            if (!moving && !close) continue;
            Vec2 dir = d.normalized();
            Vec2 f{std::sin(m.yaw), std::cos(m.yaw)};
            double dot = dir.x * f.x + dir.z * f.z;
            bool inFov = close || dot > 0.819;
            if (!inFov) continue;
            if (grass_[static_cast<size_t>(cellOf(p.pos.z)) * static_cast<size_t>(cols_) +
                       static_cast<size_t>(cellOf(p.pos.x))] != 0) {
                if (dist > sight * kGrassExposureMul) continue;
            }
            if (hasLineOfSight(m.pos, p.pos)) {
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

        for (const auto& noise : noises_) {
            if (m.state == 1) break;
            double d = m.pos.distance(noise.pos);
            if (noise.room >= 0 && noise.room != roomIdAt(m.pos)) {
                d *= 1.6;
            }
            if (d <= 28.0) {
                m.state = 2;
                m.lastSeen = noise.pos;
                m.searchTimer = 6.0;
                m.anger = std::min(100, m.anger + 8);
                break;
            }
        }

        double speedScale = 1.0 + static_cast<double>(m.anger) / 250.0;
        Vec2 goal = m.patrolTarget;
        if (m.state == 1) {
            goal = (seen >= 0) ? seenPos : m.lastSeen;
        } else if (m.state == 2) {
            goal = m.lastSeen;
            m.searchTimer -= dt;
            if (m.searchTimer <= 0.0 || m.pos.distance(m.lastSeen) < 0.8) {
                m.state = 0;
                m.patrolTarget = randomFreeSpot();
                m.patrolTimer = 0.0;
            }
        }

        double baseSpeed = (m.state == 1) ? kMonsterSpeedChase : kMonsterSpeedPatrol;
        double speed = baseSpeed * speedScale;

        m.repathTimer -= dt;
        if (m.repathTimer <= 0.0 || m.path.empty()) {
            m.repathTimer = (m.state == 1) ? 0.4 : 1.2;
            m.path = findPath(m.pos, goal);
            m.pathIndex = 0;
        }
        if (!m.path.empty() && m.pathIndex < m.path.size()) {
            Vec2 wp = m.path[m.pathIndex];
            if (m.pos.distance(wp) < 0.7) {
                ++m.pathIndex;
                if (m.pathIndex < m.path.size()) wp = m.path[m.pathIndex];
            }
            Vec2 to = wp - m.pos;
            double dist = to.length();
            if (dist > 0.05) {
                Vec2 dir = to.normalized();
                m.yaw = std::atan2(dir.x, dir.z);
                moveOnGrid(m.pos, dir, speed, dt, g, cols_, rows_);
            }
        } else {
            Vec2 to = goal - m.pos;
            double dist = to.length();
            if (dist > 0.05) {
                Vec2 dir = to.normalized();
                m.yaw = std::atan2(dir.x, dir.z);
                moveOnGrid(m.pos, dir, speed, dt, g, cols_, rows_);
            }
        }
        if (m.state == 0) {
            m.patrolTimer += dt;
            if (m.pos.distance(m.patrolTarget) < 0.8 || m.patrolTimer > 8.0) {
                m.patrolTarget = randomFreeSpot();
                m.patrolTimer = 0.0;
                m.path.clear();
            }
        }
    }
    noises_.clear();
}

void Sim::step(double dt) {
    if (dt <= 0.0) return;
    ++tick_;
    refreshDynamicBlocks();
    for (auto& p : players_) {
        if (!p.alive) continue;
        InputCmd& cmd = inputs_[static_cast<size_t>(p.id)];
        p.interact = cmd.interact;
        p.crouched = cmd.crouch;

        uint8_t buttons = 0;
        if (cmd.drop) buttons |= 0x01;
        if (cmd.throwItem) buttons |= 0x02;
        if (cmd.stash) buttons |= 0x04;
        if (cmd.unstash) buttons |= 0x08;
        if (cmd.dropAll) buttons |= 0x10;
        uint8_t pressed = static_cast<uint8_t>(buttons & ~prevButtons_[static_cast<size_t>(p.id)]);
        prevButtons_[static_cast<size_t>(p.id)] = buttons;

        bool edge = cmd.interact && prevInteract_[static_cast<size_t>(p.id)] == 0;
        prevInteract_[static_cast<size_t>(p.id)] = cmd.interact ? 1 : 0;
        if (edge) {
            handleInteract(p);
        }
        if (pressed) {
            handleActions(p, pressed, cmd);
        }
        p.prevPos = p.pos;
        movePlayer(p, cmd, dt);
        p.lastSpeed = p.pos.distance(p.prevPos) / dt;
    }
    updateObjectives(dt);
    updateMonsters(dt);
}
}
