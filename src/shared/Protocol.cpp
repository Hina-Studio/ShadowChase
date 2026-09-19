#include "shared/Protocol.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace sc {
namespace {
int16_t quantPos(double v) {
    double q = std::round(v * 100.0);
    if (q > 32000.0) q = 32000.0;
    if (q < -32000.0) q = -32000.0;
    return static_cast<int16_t>(q);
}

double dequantPos(int16_t q) {
    return static_cast<double>(q) / 100.0;
}

uint8_t quantYaw(double yaw) {
    const double twoPi = 6.28318530717958647692;
    while (yaw > 3.14159265358979323846) yaw -= twoPi;
    while (yaw < -3.14159265358979323846) yaw += twoPi;
    double t = (yaw + 3.14159265358979323846) / twoPi;
    int v = static_cast<int>(std::round(t * 255.0));
    return static_cast<uint8_t>(std::max(0, std::min(255, v)));
}

double dequantYaw(uint8_t q) {
    const double twoPi = 6.28318530717958647692;
    return (static_cast<double>(q) / 255.0) * twoPi - 3.14159265358979323846;
}
}

void Writer::u8(uint8_t v) {
    buf_.push_back(v);
}

void Writer::u16(uint16_t v) {
    buf_.push_back(static_cast<uint8_t>(v & 0xFF));
    buf_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void Writer::i16(int16_t v) {
    u16(static_cast<uint16_t>(v));
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

bool Reader::i16(int16_t& v) {
    uint16_t raw = 0;
    if (!u16(raw)) return false;
    v = static_cast<int16_t>(raw);
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

std::vector<uint8_t> encodeJoin(uint32_t roomCode, const std::string& name,
                                const std::string& version) {
    Writer w;
    w.u8(static_cast<uint8_t>(MsgType::Join));
    w.u32(roomCode);
    w.str(name);
    w.str(version);
    return w.data();
}

bool decodeJoin(const uint8_t* data, size_t size, uint32_t& roomCode, std::string& name,
                std::string& version) {
    Reader r(data, size);
    uint8_t type = 0;
    if (!r.u8(type) || type != static_cast<uint8_t>(MsgType::Join)) return false;
    if (!r.u32(roomCode)) return false;
    if (!r.str(name)) return false;
    return r.str(version);
}

std::vector<uint8_t> encodeWelcome(const WelcomeData& welcome) {
    Writer w;
    w.u8(static_cast<uint8_t>(MsgType::Welcome));
    w.u32(welcome.roomCode);
    w.u16(static_cast<uint16_t>(welcome.playerId));
    w.u32(static_cast<uint32_t>(welcome.seed));
    w.u16(static_cast<uint16_t>(welcome.mapSize));
    w.str(welcome.version);
    w.u16(static_cast<uint16_t>(welcome.blocks.size()));
    for (const auto& b : welcome.blocks) {
        w.u16(static_cast<uint16_t>(b.x));
        w.u16(static_cast<uint16_t>(b.z));
    }
    w.u16(static_cast<uint16_t>(welcome.objects.size()));
    for (const auto& o : welcome.objects) {
        w.u16(static_cast<uint16_t>(o.id));
        w.u8(o.type);
        w.f32(o.x);
        w.f32(o.z);
        uint8_t flags = 0;
        if (o.open) flags |= 0x01;
        if (o.taken) flags |= 0x02;
        w.u8(flags);
        w.i16(static_cast<int16_t>(o.holder));
    }
    return w.data();
}

bool decodeWelcome(const uint8_t* data, size_t size, WelcomeData& welcome) {
    Reader r(data, size);
    uint8_t type = 0;
    uint32_t roomCode = 0;
    uint16_t pid = 0;
    uint32_t seed = 0;
    uint16_t mapSize = 0;
    if (!r.u8(type) || type != static_cast<uint8_t>(MsgType::Welcome)) return false;
    if (!r.u32(roomCode) || !r.u16(pid) || !r.u32(seed) || !r.u16(mapSize)) return false;
    if (!r.str(welcome.version)) return false;

    welcome.roomCode = roomCode;
    welcome.playerId = pid;
    welcome.seed = seed;
    welcome.mapSize = mapSize;

    uint16_t blockCount = 0;
    if (!r.u16(blockCount)) return false;
    welcome.blocks.clear();
    welcome.blocks.reserve(blockCount);
    for (uint16_t i = 0; i < blockCount; ++i) {
        uint16_t x = 0;
        uint16_t z = 0;
        if (!r.u16(x) || !r.u16(z)) return false;
        welcome.blocks.push_back(Block{x, z});
    }

    uint16_t objCount = 0;
    if (!r.u16(objCount)) return false;
    welcome.objects.clear();
    welcome.objects.reserve(objCount);
    for (uint16_t i = 0; i < objCount; ++i) {
        WelcomeObject o;
        uint16_t id = 0;
        uint8_t flags = 0;
        int16_t holder = -1;
        if (!r.u16(id) || !r.u8(o.type) || !r.f32(o.x) || !r.f32(o.z) || !r.u8(flags) ||
            !r.i16(holder)) {
            return false;
        }
        o.id = id;
        o.open = (flags & 0x01) != 0;
        o.taken = (flags & 0x02) != 0;
        o.holder = holder;
        welcome.objects.push_back(o);
    }
    return true;
}

std::vector<uint8_t> encodeReject(RejectReason reason, const std::string& text) {
    Writer w;
    w.u8(static_cast<uint8_t>(MsgType::Reject));
    w.u8(static_cast<uint8_t>(reason));
    w.str(text);
    return w.data();
}

bool decodeReject(const uint8_t* data, size_t size, uint8_t& reason, std::string& text) {
    Reader r(data, size);
    uint8_t type = 0;
    if (!r.u8(type) || type != static_cast<uint8_t>(MsgType::Reject)) return false;
    if (!r.u8(reason)) return false;
    return r.str(text, 64);
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
    w.u8(snap.baseline ? 1 : 0);
    w.u16(static_cast<uint16_t>(snap.players.size()));
    for (const auto& p : snap.players) {
        w.u16(static_cast<uint16_t>(p.id));
        w.i16(quantPos(p.x));
        w.i16(quantPos(p.z));
        w.u8(quantYaw(p.yaw));
        w.u8(static_cast<uint8_t>(std::max(0.0f, std::min(100.0f, p.hp))));
        w.u8(p.flags);
    }
    w.u16(static_cast<uint16_t>(snap.objects.size()));
    for (const auto& o : snap.objects) {
        w.u16(static_cast<uint16_t>(o.id));
        w.u8(o.type);
        w.i16(quantPos(o.x));
        w.i16(quantPos(o.z));
        w.u8(o.flags);
        w.i16(static_cast<int16_t>(o.holder));
    }
    w.u16(static_cast<uint16_t>(snap.monsters.size()));
    for (const auto& m : snap.monsters) {
        w.u16(static_cast<uint16_t>(m.id));
        w.i16(quantPos(m.x));
        w.i16(quantPos(m.z));
        w.u8(quantYaw(m.yaw));
        w.u8(m.state);
    }
    return w.data();
}

bool decodeSnapshot(const uint8_t* data, size_t size, Snapshot& snap) {
    Reader r(data, size);
    uint8_t type = 0;
    uint32_t tick = 0;
    uint8_t baseline = 0;
    if (!r.u8(type) || type != static_cast<uint8_t>(MsgType::Snapshot)) return false;
    if (!r.u32(tick) || !r.u8(baseline)) return false;
    snap.tick = tick;
    snap.baseline = baseline != 0;

    uint16_t pcount = 0;
    if (!r.u16(pcount)) return false;
    snap.players.clear();
    snap.players.reserve(pcount);
    for (uint16_t i = 0; i < pcount; ++i) {
        uint16_t id = 0;
        int16_t qx = 0;
        int16_t qz = 0;
        uint8_t yaw = 0;
        uint8_t hp = 0;
        uint8_t flags = 0;
        if (!r.u16(id) || !r.i16(qx) || !r.i16(qz) || !r.u8(yaw) || !r.u8(hp) || !r.u8(flags)) {
            return false;
        }
        SnapshotPlayer p;
        p.id = id;
        p.x = static_cast<float>(dequantPos(qx));
        p.z = static_cast<float>(dequantPos(qz));
        p.yaw = static_cast<float>(dequantYaw(yaw));
        p.hp = static_cast<float>(hp);
        p.flags = flags;
        snap.players.push_back(p);
    }

    uint16_t ocount = 0;
    if (!r.u16(ocount)) return false;
    snap.objects.clear();
    snap.objects.reserve(ocount);
    for (uint16_t i = 0; i < ocount; ++i) {
        uint16_t id = 0;
        uint8_t otype = 0;
        int16_t qx = 0;
        int16_t qz = 0;
        uint8_t flags = 0;
        int16_t holder = -1;
        if (!r.u16(id) || !r.u8(otype) || !r.i16(qx) || !r.i16(qz) || !r.u8(flags) ||
            !r.i16(holder)) {
            return false;
        }
        SnapshotObject o;
        o.id = id;
        o.type = otype;
        o.x = static_cast<float>(dequantPos(qx));
        o.z = static_cast<float>(dequantPos(qz));
        o.flags = flags;
        o.holder = holder;
        snap.objects.push_back(o);
    }

    uint16_t mcount = 0;
    if (!r.u16(mcount)) return false;
    snap.monsters.clear();
    snap.monsters.reserve(mcount);
    for (uint16_t i = 0; i < mcount; ++i) {
        uint16_t id = 0;
        int16_t qx = 0;
        int16_t qz = 0;
        uint8_t yaw = 0;
        uint8_t st = 0;
        if (!r.u16(id) || !r.i16(qx) || !r.i16(qz) || !r.u8(yaw) || !r.u8(st)) {
            return false;
        }
        SnapshotMonster m;
        m.id = id;
        m.x = static_cast<float>(dequantPos(qx));
        m.z = static_cast<float>(dequantPos(qz));
        m.yaw = static_cast<float>(dequantYaw(yaw));
        m.state = st;
        snap.monsters.push_back(m);
    }
    return true;
}
}
