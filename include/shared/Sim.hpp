#pragma once
#include <random>
#include <string>
#include <vector>

#include "shared/MapData.hpp"
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
    FuelCan = 4,
    Generator = 5,
    Vehicle = 6,
    Battery = 7,
    File = 8,
    MasterLock = 9,
    Trap = 10,
    WaterTower = 11,
};

struct SimObject {
    int id = 0;
    ObjType type = ObjType::Door;
    Vec2 pos;
    bool open = false;
    bool taken = false;
    int holder = -1;
    int charge = 0;
    float progress = 0.0f;
    int aux = 0;
    uint8_t phase = 0;
    int op = -1;
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
    bool extracted = false;
    double extractTimer = 0.0;
    int files = 0;
    bool crouched = false;
    int stash0 = -1;
    int stash1 = -1;
    double rootTimer = 0.0;
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
    int anger = 0;
    double searchTimer = 0.0;
    std::vector<Vec2> path;
    size_t pathIndex = 0;
    double repathTimer = 0.0;
};

constexpr double kMonsterSpeedPatrol = 2.0;
constexpr double kMonsterSpeedChase = 4.6;
constexpr double kMonsterSight = 18.0;
constexpr double kMonsterCloseRange = 3.0;
constexpr double kMonsterLoseTime = 3.0;
constexpr double kPlayerMovingThreshold = 0.6;
constexpr int kGeneratorFuelNeed = 4;
constexpr int kFuelCanTotal = 14;
constexpr int kBatteryTotal = 3;
constexpr int kFileTotal = 5;
constexpr int kGeneratorCount = 2;
constexpr double kChannelTime = 2.0;
constexpr double kExtractTime = 1.5;
constexpr double kExtractRange = 3.0;
constexpr double kMatchTimeLimit = 1200.0;
constexpr double kRageTime = 900.0;
constexpr double kBatteryClampTime = 0.8;
constexpr double kBatteryWindow = 1.5;
constexpr double kSpillMoveThreshold = 0.35;
constexpr int kFilesForBigTeam = 3;
constexpr int kMasterLockTotal = 2;
constexpr double kThrowDistance = 6.0;
constexpr double kReachStanding = 1.6;
constexpr double kReachCrouched = 2.0;
constexpr double kCrouchSpeedScale = 0.6;

struct Noise {
    Vec2 pos;
    int room = -1;
};

struct InputCmd {
    double moveX = 0.0;
    double moveZ = 0.0;
    double yaw = 0.0;
    bool sprint = false;
    bool interact = false;
    bool drop = false;
    bool throwItem = false;
    bool crouch = false;
    bool stash = false;
    bool unstash = false;
    bool dropAll = false;
    unsigned int seq = 0;
};

constexpr double kPlayerRadius = 0.35;
constexpr double kWalkSpeed = 4.2;
constexpr double kSprintSpeed = 6.8;

InputCmd sanitizeInput(const InputCmd& in);

class Sim {
public:
    void generate(unsigned int seed);
    int addPlayer(const std::string& name);
    void removePlayer(int id);
    void setInput(int id, const InputCmd& cmd);
    void step(double dt);

    const std::vector<PlayerState>& players() const { return players_; }
    const std::vector<Block>& blocks() const { return blocks_; }
    const std::vector<SimObject>& objects() const { return objects_; }
    const std::vector<MonsterState>& monsters() const { return monsters_; }
    const std::vector<Vec2>& spawns() const { return spawns_; }
    int size() const { return cols_ * rows_; }
    int cols() const { return cols_; }
    int rows() const { return rows_; }
    double cellSize() const { return kCellSize; }
    int roomIdAt(const Vec2& p) const;
    unsigned int seed() const { return seed_; }
    unsigned int tick() const { return tick_; }
    int rejects() const { return rejects_; }
    int status() const { return status_; }
    const std::string& layoutReason() const { return layoutReason_; }
    double matchTime() const { return matchTime_; }
    bool rageActive() const { return rageActive_; }
    int filesRequired() const { return filesRequired_; }
    int filesFound() const;
    int generatorsFueled() const;
    int generatorsBatteries() const;
    int requiredFuelPerGenerator() const;
    bool generatorsPowered() const;
    int fuelCansInWorld() const;
    void debugTeleportPlayer(int id, double x, double z);

    bool blocked(int x, int z) const;
    bool blockedAt(const Vec2& p) const;

    static bool blockedCell(const std::vector<unsigned char>& grid, int cols, int rows, double x,
                            double z);
    static void moveOnGrid(Vec2& pos, const Vec2& dir, double speed, double dt,
                           const std::vector<unsigned char>& grid, int cols, int rows);

private:
    void movePlayer(PlayerState& p, const InputCmd& cmd, double dt);
    void handleInteract(PlayerState& p);
    void refreshDynamicBlocks();
    void updateMonsters(double dt);
    void updateObjectives(double dt);
    void updateAnger(double dt);
    void addNoise(const Vec2& pos);
    void handleActions(PlayerState& p, uint8_t pressed, const InputCmd& cmd);
    void placeQuestObjects();
    bool validateLayout(std::string& reason) const;
    bool reachableFrom(const Vec2& start, const Vec2& goal,
                       const std::vector<unsigned char>& grid) const;
    void applyFallback();
    void buildRooms();
    bool passableCell(int c, int r) const;
    std::vector<Vec2> findPath(const Vec2& from, const Vec2& to) const;
    void updateTraps(double dt);
    bool hasLineOfSight(const Vec2& a, const Vec2& b) const;
    Vec2 randomFreeSpot();
    bool blockedOnAxis(double x, double z) const;

    int cols_ = 36;
    int rows_ = 26;
    unsigned int seed_ = 0;
    unsigned int tick_ = 0;
    int rejects_ = 0;
    std::vector<Block> blocks_;
    std::vector<Vec2> spawns_;
    std::vector<PlayerState> players_;
    std::vector<InputCmd> inputs_;
    std::vector<unsigned char> prevInteract_;
    std::vector<unsigned char> prevHeldInteract_;
    std::vector<unsigned char> prevButtons_;
    std::mt19937 actionRng_;
    std::vector<unsigned char> grid_;
    std::vector<unsigned char> dynamicGrid_;
    std::vector<unsigned char> grass_;
    std::vector<int> room_;
    std::vector<Vec2> genCandidates_;
    std::vector<Vec2> lootCandidates_;
    std::vector<Vec2> trapCandidates_;
    std::vector<Vec2> exitCandidates_;
    std::vector<Vec2> spawnCandidates_;
    int activeExit_ = -1;
    std::vector<SimObject> objects_;
    std::vector<MonsterState> monsters_;
    std::mt19937 rng_;
    int status_ = 0;
    int vehicleId_ = -1;
    std::string layoutReason_ = "ok";
    std::vector<Noise> noises_;
    double matchTime_ = 0.0;
    bool rageActive_ = false;
    int filesRequired_ = 0;
    bool helicopterSpawned_ = false;
};
}
