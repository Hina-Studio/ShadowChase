#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "shared/Sim.hpp"

namespace sc {
enum class MsgType : uint8_t {
    Join = 1,
    Welcome = 2,
    Input = 3,
    Snapshot = 4,
    Reject = 5,
};

enum class RejectReason : uint8_t {
    WrongRoom = 1,
    VersionMismatch = 2,
    Full = 3,
};

struct SnapshotPlayer {
    int id = 0;
    float x = 0.0f;
    float z = 0.0f;
    float yaw = 0.0f;
    float hp = 100.0f;
    uint8_t flags = 0;
};

struct SnapshotObject {
    int id = 0;
    uint8_t type = 0;
    float x = 0.0f;
    float z = 0.0f;
    uint8_t flags = 0;
    int holder = -1;
};

struct SnapshotMonster {
    int id = 0;
    float x = 0.0f;
    float z = 0.0f;
    float yaw = 0.0f;
    uint8_t state = 0;
};

struct Snapshot {
    uint32_t tick = 0;
    bool baseline = false;
    std::vector<SnapshotPlayer> players;
    std::vector<SnapshotObject> objects;
    std::vector<SnapshotMonster> monsters;
};

struct WelcomeObject {
    int id = 0;
    uint8_t type = 0;
    float x = 0.0f;
    float z = 0.0f;
    bool open = false;
    bool taken = false;
    int holder = -1;
};

struct WelcomeData {
    uint32_t roomCode = 0;
    int playerId = -1;
    unsigned int seed = 0;
    int mapSize = 0;
    std::vector<Block> blocks;
    std::vector<WelcomeObject> objects;
    std::string version;
};

class Writer {
public:
    void u8(uint8_t v);
    void u16(uint16_t v);
    void i16(int16_t v);
    void u32(uint32_t v);
    void f32(float v);
    void str(const std::string& s);

    const std::vector<uint8_t>& data() const { return buf_; }
    std::vector<uint8_t>& data() { return buf_; }

private:
    std::vector<uint8_t> buf_;
};

class Reader {
public:
    Reader(const uint8_t* data, size_t size) : p_(data), n_(size) {}

    bool u8(uint8_t& v);
    bool u16(uint16_t& v);
    bool i16(int16_t& v);
    bool u32(uint32_t& v);
    bool f32(float& v);
    bool str(std::string& s, size_t maxLen = 32);

private:
    const uint8_t* p_;
    size_t n_;
    size_t i_ = 0;
};

std::vector<uint8_t> encodeJoin(uint32_t roomCode, const std::string& name, const std::string& version);
bool decodeJoin(const uint8_t* data, size_t size, uint32_t& roomCode, std::string& name,
                std::string& version);

std::vector<uint8_t> encodeWelcome(const WelcomeData& welcome);
bool decodeWelcome(const uint8_t* data, size_t size, WelcomeData& welcome);

std::vector<uint8_t> encodeReject(RejectReason reason, const std::string& text);
bool decodeReject(const uint8_t* data, size_t size, uint8_t& reason, std::string& text);

std::vector<uint8_t> encodeInput(const InputCmd& cmd);
bool decodeInput(const uint8_t* data, size_t size, InputCmd& cmd);

std::vector<uint8_t> encodeSnapshot(const Snapshot& snap);
bool decodeSnapshot(const uint8_t* data, size_t size, Snapshot& snap);
}
