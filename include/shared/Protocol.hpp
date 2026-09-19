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
};

struct SnapshotPlayer {
    int id = 0;
    float x = 0.0f;
    float z = 0.0f;
    float yaw = 0.0f;
    float hp = 100.0f;
    uint8_t flags = 0;
};

struct Snapshot {
    uint32_t tick = 0;
    std::vector<SnapshotPlayer> players;
};

class Writer {
public:
    void u8(uint8_t v);
    void u16(uint16_t v);
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
    bool u32(uint32_t& v);
    bool f32(float& v);
    bool str(std::string& s, size_t maxLen = 32);

private:
    const uint8_t* p_;
    size_t n_;
    size_t i_ = 0;
};

std::vector<uint8_t> encodeJoin(const std::string& name);
bool decodeJoin(const uint8_t* data, size_t size, std::string& name);

std::vector<uint8_t> encodeWelcome(int playerId, unsigned int seed, int mapSize,
                                   const std::vector<Block>& blocks);
bool decodeWelcome(const uint8_t* data, size_t size, int& playerId, unsigned int& seed,
                   int& mapSize, std::vector<Block>& blocks);

std::vector<uint8_t> encodeInput(const InputCmd& cmd);
bool decodeInput(const uint8_t* data, size_t size, InputCmd& cmd);

std::vector<uint8_t> encodeSnapshot(const Snapshot& snap);
bool decodeSnapshot(const uint8_t* data, size_t size, Snapshot& snap);
}
