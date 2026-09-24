#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <enet/enet.h>

#include "shared/Protocol.hpp"
#include "shared/Sim.hpp"

struct ClientDebugState {
    float fps = 0.0f;
    float frameMs = 0.0f;
    int rttMs = -1;
    uint32_t serverTick = 0;
    int playerId = -1;
    int remoteCount = 0;
    int recvPackets = 0;
    long long recvBytes = 0;
    unsigned int seed = 0;
    int mapCols = 0;
    int mapRows = 0;
    float cellSize = 2.0f;
    int blockCount = 0;
    int objectCount = 0;
    int monsterCount = 0;
    int monsterState = -1;
    double monsterX = 0.0;
    double monsterZ = 0.0;
    int fuelCans = 0;
    int gensPowered = 0;
    int gensFueled = 0;
    int gensBattery = 0;
    int filesFound = 0;
    int filesNeed = 0;
    int timerSec = 0;
    bool rage = false;
    int gensFuelNeed = 4;
    int matchStatus = 0;
    double ownX = 0.0;
    double ownZ = 0.0;
    float hp = 100.0f;
    int carrying = -1;
    bool connected = false;
    bool padActive = false;
};

class ClientApp {
public:
    bool init(int argc, char** argv);
    void run();
    void shutdown();

private:
    void handleEvents();
    void sendInput(double dt);
    void updateLocal(double dt);
    void render();
    void rebuildDynamicGrid();
    std::string interactionPrompt() const;

    ENetHost* host_ = nullptr;
    ENetPeer* peer_ = nullptr;
    bool connected_ = false;
    bool rejected_ = false;
    std::string serverAddr_ = "127.0.0.1";
    int serverPort_ = 7777;
    std::string playerName_ = "Player";
    uint32_t roomCode_ = 0;

    std::vector<sc::Block> blocks_;
    std::vector<unsigned char> grid_;
    std::vector<unsigned char> dynGrid_;
    int mapCols_ = 0;
    int mapRows_ = 0;
    float cell_ = 2.0f;
    unsigned int seed_ = 0;
    int playerId_ = -1;
    sc::Vec2 predicted_;
    double yaw_ = 0.0;
    double pitch_ = 0.0;

    std::unordered_map<int, sc::SnapshotPlayer> netPlayers_;
    std::unordered_map<int, sc::SnapshotObject> netObjects_;
    std::unordered_map<int, sc::SnapshotMonster> netMonsters_;
    uint32_t serverTick_ = 0;
    uint8_t serverStatus_ = 0;
    uint16_t serverTimer_ = 0;
    uint8_t serverFilesNeed_ = 0;
    uint8_t serverFilesDone_ = 0;
    uint8_t serverFuelNeed_ = 4;
    bool serverRage_ = false;

    double inputTimer_ = 0.0;
    unsigned int seq_ = 0;
    bool showPanel_ = true;
    bool quit_ = false;
    int altCount_ = 0;
    double altTimer_ = 0.0;
    bool flashlight_ = false;
    ClientDebugState dbg_;
};
