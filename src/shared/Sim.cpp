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
    double r = kPlayerRadius;
    return blockedAt(Vec2{x - r, z - r}) || blockedAt(Vec2{x + r, z - r}) ||
           blockedAt(Vec2{x - r, z + r}) || blockedAt(Vec2{x + r, z + r});
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
    players_.push_back(p);
    inputs_.push_back(InputCmd{});
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
    Vec2 dir{cmd.moveX, cmd.moveZ};
    if (dir.length() < 1e-6) {
        p.sprinting = false;
        return;
    }
    dir = dir.normalized();
    double speed = cmd.sprint ? kSprintSpeed : kWalkSpeed;
    p.sprinting = cmd.sprint;
    p.yaw = cmd.yaw;
    moveOnGrid(p.pos, dir, speed, dt, grid_, size_);
}

void Sim::step(double dt) {
    if (dt <= 0.0) return;
    ++tick_;
    for (auto& p : players_) {
        if (!p.alive) continue;
        const InputCmd& cmd = inputs_[static_cast<size_t>(p.id)];
        p.interact = cmd.interact;
        movePlayer(p, cmd, dt);
    }
}
}
