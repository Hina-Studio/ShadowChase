#include "shared/Protocol.hpp"

#include <cstring>

namespace sc {
void Writer::u8(uint8_t v) {
    buf_.push_back(v);
}

void Writer::u16(uint16_t v) {
    buf_.push_back(static_cast<uint8_t>(v & 0xFF));
    buf_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void Writer::u32(uint32_t v) {
    buf_.push_back(static_cast<uint8_t>(v & 0xFF));
    buf_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

void Writer::f32(float v) {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    u32(bits);
}

void Writer::str(const std::string& s) {
    std::string trimmed = s.substr(0, 31);
    u8(static_cast<uint8_t>(trimmed.size()));
    for (char c : trimmed) {
        buf_.push_back(static_cast<uint8_t>(c));
    }
}

bool Reader::u8(uint8_t& v) {
    if (i_ + 1 > n_) return false;
    v = p_[i_++];
    return true;
}

bool Reader::u16(uint16_t& v) {
    if (i_ + 2 > n_) return false;
    v = static_cast<uint16_t>(p_[i_]) | static_cast<uint16_t>(p_[i_ + 1] << 8);
    i_ += 2;
    return true;
}

bool Reader::u32(uint32_t& v) {
    if (i_ + 4 > n_) return false;
    v = static_cast<uint32_t>(p_[i_]) | (static_cast<uint32_t>(p_[i_ + 1]) << 8) |
        (static_cast<uint32_t>(p_[i_ + 2]) << 16) | (static_cast<uint32_t>(p_[i_ + 3]) << 24);
    i_ += 4;
    return true;
}

bool Reader::f32(float& v) {
    uint32_t bits = 0;
    if (!u32(bits)) return false;
    std::memcpy(&v, &bits, sizeof(v));
    return true;
}

bool Reader::str(std::string& s, size_t maxLen) {
    uint8_t len = 0;
    if (!u8(len)) return false;
    if (len > maxLen || i_ + len > n_) return false;
    s.assign(reinterpret_cast<const char*>(p_ + i_), len);
    i_ += len;
    return true;
}

std::vector<uint8_t> encodeJoin(const std::string& name) {
    Writer w;
    w.u8(static_cast<uint8_t>(MsgType::Join));
    w.str(name);
    return w.data();
}

bool decodeJoin(const uint8_t* data, size_t size, std::string& name) {
    Reader r(data, size);
    uint8_t type = 0;
    if (!r.u8(type) || type != static_cast<uint8_t>(MsgType::Join)) return false;
    return r.str(name);
}

std::vector<uint8_t> encodeWelcome(int playerId, unsigned int seed, int mapSize,
                                   const std::vector<Block>& blocks) {
    Writer w;
    w.u8(static_cast<uint8_t>(MsgType::Welcome));
    w.u16(static_cast<uint16_t>(playerId));
    w.u32(static_cast<uint32_t>(seed));
    w.u16(static_cast<uint16_t>(mapSize));
    w.u16(static_cast<uint16_t>(blocks.size()));
    for (const auto& b : blocks) {
        w.u16(static_cast<uint16_t>(b.x));
        w.u16(static_cast<uint16_t>(b.z));
    }
    return w.data();
}

bool decodeWelcome(const uint8_t* data, size_t size, int& playerId, unsigned int& seed,
                   int& mapSize, std::vector<Block>& blocks) {
    Reader r(data, size);
    uint8_t type = 0;
    uint16_t pid = 0;
    uint32_t s = 0;
    uint16_t ms = 0;
    uint16_t count = 0;
    if (!r.u8(type) || type != static_cast<uint8_t>(MsgType::Welcome)) return false;
    if (!r.u16(pid) || !r.u32(s) || !r.u16(ms) || !r.u16(count)) return false;
    playerId = pid;
    seed = s;
    mapSize = ms;
    blocks.clear();
    blocks.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
        uint16_t x = 0;
        uint16_t z = 0;
        if (!r.u16(x) || !r.u16(z)) return false;
        blocks.push_back(Block{x, z});
    }
    return true;
}

std::vector<uint8_t> encodeInput(const InputCmd& cmd) {
    Writer w;
    w.u8(static_cast<uint8_t>(MsgType::Input));
    w.u16(static_cast<uint16_t>(cmd.seq & 0xFFFF));
    w.f32(static_cast<float>(cmd.moveX));
    w.f32(static_cast<float>(cmd.moveZ));
    w.f32(static_cast<float>(cmd.yaw));
    uint8_t flags = 0;
    if (cmd.sprint) flags |= 0x01;
    if (cmd.interact) flags |= 0x02;
    w.u8(flags);
    return w.data();
}

bool decodeInput(const uint8_t* data, size_t size, InputCmd& cmd) {
    Reader r(data, size);
    uint8_t type = 0;
    uint16_t seq = 0;
    float mx = 0.0f;
    float mz = 0.0f;
    float yaw = 0.0f;
    uint8_t flags = 0;
    if (!r.u8(type) || type != static_cast<uint8_t>(MsgType::Input)) return false;
    if (!r.u16(seq) || !r.f32(mx) || !r.f32(mz) || !r.f32(yaw) || !r.u8(flags)) return false;
    cmd.seq = seq;
    cmd.moveX = mx;
    cmd.moveZ = mz;
    cmd.yaw = yaw;
    cmd.sprint = (flags & 0x01) != 0;
    cmd.interact = (flags & 0x02) != 0;
    return true;
}

std::vector<uint8_t> encodeSnapshot(const Snapshot& snap) {
    Writer w;
    w.u8(static_cast<uint8_t>(MsgType::Snapshot));
    w.u32(snap.tick);
    w.u16(static_cast<uint16_t>(snap.players.size()));
    for (const auto& p : snap.players) {
        w.u16(static_cast<uint16_t>(p.id));
        w.f32(p.x);
        w.f32(p.z);
        w.f32(p.yaw);
        w.f32(p.hp);
        w.u8(p.flags);
    }
    return w.data();
}

bool decodeSnapshot(const uint8_t* data, size_t size, Snapshot& snap) {
    Reader r(data, size);
    uint8_t type = 0;
    uint32_t tick = 0;
    uint16_t count = 0;
    if (!r.u8(type) || type != static_cast<uint8_t>(MsgType::Snapshot)) return false;
    if (!r.u32(tick) || !r.u16(count)) return false;
    snap.tick = tick;
    snap.players.clear();
    snap.players.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
        uint16_t id = 0;
        SnapshotPlayer p;
        if (!r.u16(id) || !r.f32(p.x) || !r.f32(p.z) || !r.f32(p.yaw) || !r.f32(p.hp) ||
            !r.u8(p.flags)) {
            return false;
        }
        p.id = id;
        snap.players.push_back(p);
    }
    return true;
}
}
