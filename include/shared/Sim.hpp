#pragma once
#include <random>
#include <string>
#include <vector>

#include "shared/Math.hpp"

namespace sc {
struct Block {
    int x = 0;
    int z = 0;
};

enum class ObjType : uint8_t {
    Door = 1,
    Crate = 2,
    Pickup = 3,
};

struct SimObject {
    int id = 0;
    ObjType type = ObjType::Door;
    Vec2 pos;
    bool open = false;
    bool taken = false;
    int holder = -1;
};

struct PlayerState {
    int id = -1;
    std::string name;
    Vec2 pos;
    Vec2 prevPos;
    double lastSpeed = 0.0;
    double yaw = 0.0;
    double hp = 100.0;
    int ammo = 0;
    int carrying = -1;
    bool sprinting = false;
    bool interact = false;
    bool alive = true;
};

struct MonsterState {
    int id = 0;
    Vec2 pos;
    Vec2 patrolTarget;
    Vec2 lastSeen;
    double yaw = 0.0;
    uint8_t state = 0;
    int target = -1;
    double loseTimer = 0.0;
    double patrolTimer = 0.0;
};

constexpr double kMonsterSpeedPatrol = 2.0;
constexpr double kMonsterSpeedChase = 4.6;
constexpr double kMonsterSight = 18.0;
constexpr double kMonsterCloseRange = 3.0;
constexpr double kMonsterLoseTime = 3.0;
constexpr double kPlayerMovingThreshold = 0.6;

struct InputCmd {
    double moveX = 0.0;
    double moveZ = 0.0;
    double yaw = 0.0;
    bool sprint = false;
    bool interact = false;
    unsigned int seq = 0;
};

constexpr double kPlayerRadius = 0.35;
constexpr double kWalkSpeed = 4.2;
constexpr double kSprintSpeed = 6.8;

InputCmd sanitizeInput(const InputCmd& in);

class Sim {
public:
    void generate(unsigned int seed, int size);
    int addPlayer(const std::string& name);
    void removePlayer(int id);
    void setInput(int id, const InputCmd& cmd);
    void step(double dt);

    const std::vector<PlayerState>& players() const { return players_; }
    const std::vector<Block>& blocks() const { return blocks_; }
    const std::vector<SimObject>& objects() const { return objects_; }
    const std::vector<MonsterState>& monsters() const { return monsters_; }
    const std::vector<Vec2>& spawns() const { return spawns_; }
    int size() const { return size_; }
    unsigned int seed() const { return seed_; }
    unsigned int tick() const { return tick_; }
    int rejects() const { return rejects_; }

    bool blocked(int x, int z) const;
    bool blockedAt(const Vec2& p) const;

    static bool blockedCell(const std::vector<unsigned char>& grid, int size, double x, double z);
    static void moveOnGrid(Vec2& pos, const Vec2& dir, double speed, double dt,
                           const std::vector<unsigned char>& grid, int size);

private:
    void movePlayer(PlayerState& p, const InputCmd& cmd, double dt);
    void handleInteract(PlayerState& p);
    void refreshDynamicBlocks();
    void updateMonsters(double dt);
    bool hasLineOfSight(const Vec2& a, const Vec2& b) const;
    Vec2 randomFreeSpot();
    bool blockedOnAxis(double x, double z) const;

    int size_ = 48;
    unsigned int seed_ = 0;
    unsigned int tick_ = 0;
    int rejects_ = 0;
    std::vector<Block> blocks_;
    std::vector<Vec2> spawns_;
    std::vector<PlayerState> players_;
    std::vector<InputCmd> inputs_;
    std::vector<unsigned char> prevInteract_;
    std::vector<unsigned char> grid_;
    std::vector<unsigned char> dynamicGrid_;
    std::vector<SimObject> objects_;
    std::vector<MonsterState> monsters_;
    std::mt19937 rng_;
};
}
