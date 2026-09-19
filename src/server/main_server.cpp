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

#include <chrono>
#include <cmath>
#include <cstring>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <enet/enet.h>

#include "core/Config.hpp"
#include "core/Logger.hpp"
#include "shared/Protocol.hpp"
#include "shared/Sim.hpp"

namespace {
constexpr int kChannelControl = 0;
constexpr int kChannelSnapshot = 1;
constexpr double kTickSeconds = 0.05;

struct ClientLink {
    int playerId = -1;
    ENetPeer* peer = nullptr;
    bool sentOnce = false;
    std::unordered_map<int, sc::SnapshotPlayer> lastPlayers;
    std::unordered_map<int, sc::SnapshotObject> lastObjects;
    std::unordered_map<int, sc::SnapshotMonster> lastMonsters;
};

class Server {
public:
    bool start(int port, unsigned int seed, int mapSize, int maxPlayers, uint32_t roomCode,
               double aoiRadius) {
        roomCode_ = roomCode;
        aoiRadius_ = aoiRadius;
        if (enet_initialize() != 0) {
            core::Logger::error("[SERVER] enet init failed");
            return false;
        }
        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = static_cast<enet_uint16>(port);
        host_ = enet_host_create(&address, static_cast<size_t>(maxPlayers), 2, 0, 0);
        if (!host_) {
            core::Logger::error("[SERVER] host create failed");
            return false;
        }
        sim_.generate(seed, mapSize);
        core::Logger::info("[SERVER] layout: " + sim_.layoutReason() + " fuelCans=" +
                           std::to_string(sim_.fuelCansInWorld()));
        core::Logger::info("[SERVER] listening port=" + std::to_string(port) + " room=" +
                           std::to_string(roomCode_) + " seed=" + std::to_string(seed) +
                           " map=" + std::to_string(mapSize) + " aoi=" +
                           std::to_string(static_cast<int>(aoiRadius_)));
        return true;
    }

    void shutdown() {
        if (host_) {
            enet_host_destroy(host_);
            host_ = nullptr;
            enet_deinitialize();
        }
    }

    void serviceNetwork() {
        if (!host_) return;
        ENetEvent event;
        while (enet_host_service(host_, &event, 0) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                core::Logger::info("[SERVER] peer connected");
            } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                for (auto it = clients_.begin(); it != clients_.end(); ++it) {
                    if (it->second.peer == event.peer) {
                        core::Logger::info("[SERVER] player " + std::to_string(it->first) +
                                           " disconnected");
                        sim_.removePlayer(it->first);
                        clients_.erase(it);
                        break;
                    }
                }
            } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                handlePacket(event.peer, event.packet->data, event.packet->dataLength);
                enet_packet_destroy(event.packet);
            }
        }
    }

    void tickOnce(double dt) {
        sim_.step(dt);
        if (!gensPowered_ && sim_.generatorsPowered()) {
            gensPowered_ = true;
            core::Logger::info("[SERVER] all generators powered - vehicle unlocked");
        }
        int vehicleCharge = 0;
        for (const auto& o : sim_.objects()) {
            if (o.type == sc::ObjType::Vehicle) vehicleCharge = o.charge;
        }
        if (vehicleCharge > vehicleCharge_) {
            vehicleCharge_ = vehicleCharge;
            core::Logger::info("[SERVER] vehicle fuel " + std::to_string(vehicleCharge_) + "/" +
                               std::to_string(sc::kVehicleFuelNeed));
        }
        if (status_ == 0 && sim_.status() == 1) {
            status_ = 1;
            core::Logger::info("[SERVER] extraction complete");
        }
        broadcastSnapshot();
    }

    void step(double dt) {
        accumulator_ += dt;
        while (accumulator_ >= kTickSeconds) {
            accumulator_ -= kTickSeconds;
            tickOnce(kTickSeconds);
        }
    }

    uint32_t roomCode() const { return roomCode_; }
    const sc::Sim& sim() const { return sim_; }
    sc::Sim& sim() { return sim_; }
    size_t clientCount() const { return clients_.size(); }

private:
    void handlePacket(ENetPeer* peer, const enet_uint8* data, size_t size) {
        uint8_t type = 0;
        if (size > 0) type = data[0];
        if (type == static_cast<uint8_t>(sc::MsgType::Join)) {
            uint32_t roomCode = 0;
            std::string name;
            std::string version;
            if (!sc::decodeJoin(data, size, roomCode, name, version)) return;

            if (roomCode != roomCode_) {
                auto reject = sc::encodeReject(sc::RejectReason::WrongRoom, "wrong room code");
                ENetPacket* packet = enet_packet_create(reject.data(), reject.size(),
                                                        ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(peer, kChannelControl, packet);
                enet_host_flush(host_);
                enet_peer_disconnect_later(peer, 0);
                core::Logger::warn("[SERVER] rejected join: wrong room code " +
                                   std::to_string(roomCode));
                return;
            }
            if (version != SLASHCO_VERSION) {
                auto reject = sc::encodeReject(sc::RejectReason::VersionMismatch, "version mismatch");
                ENetPacket* packet = enet_packet_create(reject.data(), reject.size(),
                                                        ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(peer, kChannelControl, packet);
                enet_host_flush(host_);
                enet_peer_disconnect_later(peer, 0);
                core::Logger::warn("[SERVER] rejected join: version " + version + " != " +
                                   SLASHCO_VERSION);
                return;
            }

            int id = sim_.addPlayer(name);
            ClientLink link;
            link.playerId = id;
            link.peer = peer;
            clients_[id] = link;

            sc::WelcomeData welcome;
            welcome.roomCode = roomCode_;
            welcome.playerId = id;
            welcome.seed = sim_.seed();
            welcome.mapSize = sim_.size();
            welcome.version = SLASHCO_VERSION;
            welcome.blocks = sim_.blocks();
            for (const auto& o : sim_.objects()) {
                sc::WelcomeObject wo;
                wo.id = o.id;
                wo.type = static_cast<uint8_t>(o.type);
                wo.x = static_cast<float>(o.pos.x);
                wo.z = static_cast<float>(o.pos.z);
                wo.open = o.open;
                wo.taken = o.taken;
                wo.holder = o.holder;
                wo.charge = o.charge;
                welcome.objects.push_back(wo);
            }
            auto payload = sc::encodeWelcome(welcome);
            ENetPacket* packet = enet_packet_create(payload.data(), payload.size(),
                                                    ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(peer, kChannelControl, packet);
            enet_host_flush(host_);

            core::Logger::info("[SERVER] player " + std::to_string(id) + " joined as '" + name +
                               "' players=" + std::to_string(sim_.players().size()));
        } else if (type == static_cast<uint8_t>(sc::MsgType::Input)) {
            sc::InputCmd cmd;
            if (!sc::decodeInput(data, size, cmd)) return;
            for (const auto& kv : clients_) {
                if (kv.second.peer == peer) {
                    sim_.setInput(kv.first, cmd);
                    break;
                }
            }
        }
    }

    void broadcastSnapshot() {
        std::vector<sc::SnapshotPlayer> players;
        for (const auto& p : sim_.players()) {
            if (!p.alive) continue;
            sc::SnapshotPlayer sp;
            sp.id = p.id;
            sp.x = static_cast<float>(p.pos.x);
            sp.z = static_cast<float>(p.pos.z);
            sp.yaw = static_cast<float>(p.yaw);
            sp.hp = static_cast<float>(p.hp);
            sp.flags = p.sprinting ? 0x01 : 0x00;
            players.push_back(sp);
        }
        std::vector<sc::SnapshotObject> objects;
        for (const auto& o : sim_.objects()) {
            sc::SnapshotObject so;
            so.id = o.id;
            so.type = static_cast<uint8_t>(o.type);
            so.x = static_cast<float>(o.pos.x);
            so.z = static_cast<float>(o.pos.z);
            so.holder = o.holder;
            so.flags = 0;
            if (o.open) so.flags |= 0x01;
            if (o.taken) so.flags |= 0x02;
            so.charge = static_cast<uint8_t>(std::max(0, std::min(255, o.charge)));
            so.progress = static_cast<uint8_t>(std::max(
                0.0f, std::min(255.0f, o.progress / static_cast<float>(sc::kChannelTime) * 255.0f)));
            objects.push_back(so);
        }
        std::vector<sc::SnapshotMonster> monsters;
        for (const auto& m : sim_.monsters()) {
            sc::SnapshotMonster sm;
            sm.id = m.id;
            sm.x = static_cast<float>(m.pos.x);
            sm.z = static_cast<float>(m.pos.z);
            sm.yaw = static_cast<float>(m.yaw);
            sm.state = m.state;
            monsters.push_back(sm);
        }

        bool baselineTick = (sim_.tick() % 10) == 0;
        bool sendFar = (sim_.tick() % 4) == 0;
        double aoi2 = aoiRadius_ * aoiRadius_;

        for (auto& kv : clients_) {
            ClientLink& link = kv.second;
            sc::Vec2 own{0.0, 0.0};
            for (const auto& p : sim_.players()) {
                if (p.id == kv.first) own = p.pos;
            }
            auto farBy = [&](double x, double z) {
                if (aoiRadius_ <= 0.0) return false;
                double dx = x - own.x;
                double dz = z - own.z;
                return dx * dx + dz * dz > aoi2;
            };

            sc::Snapshot snap;
            snap.tick = sim_.tick();
            snap.status = static_cast<uint8_t>(sim_.status());
            snap.baseline = baselineTick || !link.sentOnce;

            if (snap.baseline) {
                snap.players = players;
                snap.objects = objects;
                snap.monsters = monsters;
                link.lastPlayers.clear();
                link.lastObjects.clear();
                link.lastMonsters.clear();
                for (const auto& p : players) link.lastPlayers[p.id] = p;
                for (const auto& o : objects) link.lastObjects[o.id] = o;
                for (const auto& m : monsters) link.lastMonsters[m.id] = m;
            } else {
                for (const auto& p : players) {
                    auto it = link.lastPlayers.find(p.id);
                    bool changed = it == link.lastPlayers.end() || it->second.x != p.x ||
                                   it->second.z != p.z || it->second.yaw != p.yaw ||
                                   it->second.hp != p.hp || it->second.flags != p.flags;
                    if (!changed) continue;
                    if (farBy(p.x, p.z) && !sendFar) continue;
                    snap.players.push_back(p);
                    link.lastPlayers[p.id] = p;
                }
                for (const auto& o : objects) {
                    auto it = link.lastObjects.find(o.id);
                    bool changed = it == link.lastObjects.end() || it->second.x != o.x ||
                                   it->second.z != o.z || it->second.flags != o.flags ||
                                   it->second.holder != o.holder;
                    if (!changed) continue;
                    if (farBy(o.x, o.z) && !sendFar) continue;
                    snap.objects.push_back(o);
                    link.lastObjects[o.id] = o;
                }
                for (const auto& m : monsters) {
                    auto it = link.lastMonsters.find(m.id);
                    bool changed = it == link.lastMonsters.end() || it->second.x != m.x ||
                                   it->second.z != m.z || it->second.yaw != m.yaw ||
                                   it->second.state != m.state;
                    if (!changed) continue;
                    if (farBy(m.x, m.z) && !sendFar) continue;
                    snap.monsters.push_back(m);
                    link.lastMonsters[m.id] = m;
                }
            }
            link.sentOnce = true;

            auto payload = sc::encodeSnapshot(snap);
            ENetPacket* packet = enet_packet_create(payload.data(), payload.size(), 0);
            enet_peer_send(link.peer, kChannelSnapshot, packet);
        }
        if (host_) enet_host_flush(host_);
    }

    ENetHost* host_ = nullptr;
    uint32_t roomCode_ = 0;
    double aoiRadius_ = 0.0;
    sc::Sim sim_;
    bool gensPowered_ = false;
    int vehicleCharge_ = 0;
    int status_ = 0;
    std::unordered_map<int, ClientLink> clients_;
    double accumulator_ = 0.0;
};

struct BotClient {
    ENetHost* host = nullptr;
    ENetPeer* peer = nullptr;
    int playerId = -1;
    int snapshots = 0;
    int welcomeCount = 0;
    int rejects = 0;
    int objectCount = 0;
    int monsterCount = 0;
    bool monsterSeen = false;
    bool monsterMoved = false;
    float monsterFirstX = 0.0f;
    float monsterFirstZ = 0.0f;
    bool expectReject = false;
    double phase = 0.0;
    double inputTimer = 0.0;
    std::string pendingName;
    uint32_t roomCode = 0;

    bool connect(int port, const std::string& name, uint32_t room) {
        host = enet_host_create(nullptr, 1, 2, 0, 0);
        if (!host) return false;
        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = static_cast<enet_uint16>(port);
        enet_address_set_host(&address, "127.0.0.1");
        peer = enet_host_connect(host, &address, 2, 0);
        if (!peer) return false;
        pendingName = name;
        roomCode = room;
        return true;
    }

    void service(double dt) {
        if (!host) return;
        ENetEvent event;
        while (enet_host_service(host, &event, 0) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                auto join = sc::encodeJoin(roomCode, pendingName, SLASHCO_VERSION);
                ENetPacket* packet = enet_packet_create(join.data(), join.size(),
                                                        ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(peer, kChannelControl, packet);
                enet_host_flush(host);
            } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                uint8_t type = event.packet->dataLength > 0 ? event.packet->data[0] : 0;
                if (type == static_cast<uint8_t>(sc::MsgType::Welcome)) {
                    sc::WelcomeData welcome;
                    if (sc::decodeWelcome(event.packet->data, event.packet->dataLength, welcome)) {
                        ++welcomeCount;
                        playerId = welcome.playerId;
                        objectCount = static_cast<int>(welcome.objects.size());
                        core::Logger::info("[BOT] welcome id=" + std::to_string(playerId) +
                                           " blocks=" + std::to_string(welcome.blocks.size()) +
                                           " objects=" + std::to_string(welcome.objects.size()));
                    }
                } else if (type == static_cast<uint8_t>(sc::MsgType::Snapshot)) {
                    sc::Snapshot snap;
                    if (sc::decodeSnapshot(event.packet->data, event.packet->dataLength, snap)) {
                        ++snapshots;
                        for (const auto& m : snap.monsters) {
                            ++monsterCount;
                            if (!monsterSeen) {
                                monsterSeen = true;
                                monsterFirstX = m.x;
                                monsterFirstZ = m.z;
                            } else if (std::fabs(m.x - monsterFirstX) +
                                           std::fabs(m.z - monsterFirstZ) >
                                       0.8f) {
                                monsterMoved = true;
                            }
                        }
                    }
                } else if (type == static_cast<uint8_t>(sc::MsgType::Reject)) {
                    uint8_t reason = 0;
                    std::string text;
                    if (sc::decodeReject(event.packet->data, event.packet->dataLength, reason,
                                         text)) {
                        ++rejects;
                        core::Logger::info("[BOT] reject reason=" + std::to_string(reason) + " " +
                                           text);
                    }
                }
                enet_packet_destroy(event.packet);
            }
        }

        if (welcomeCount > 0 && !expectReject) {
            inputTimer += dt;
            if (inputTimer >= 0.05) {
                inputTimer = 0.0;
                phase += 0.1;
                sc::InputCmd cmd;
                cmd.moveX = std::cos(phase);
                cmd.moveZ = std::sin(phase);
                cmd.yaw = phase;
                cmd.sprint = (static_cast<int>(phase) % 2) == 0;
                cmd.interact = (static_cast<int>(phase * 10.0) % 37) == 0;
                cmd.seq = static_cast<unsigned>((phase * 100));
                auto payload = sc::encodeInput(cmd);
                ENetPacket* packet = enet_packet_create(payload.data(), payload.size(), 0);
                enet_peer_send(peer, kChannelSnapshot, packet);
                enet_host_flush(host);
            }
        }
    }

    void stop() {
        if (host) {
            enet_host_destroy(host);
            host = nullptr;
        }
    }
};

int runSelfTest(int port, double seconds) {
    const uint32_t roomCode = 12345678u;
    Server server;
    if (!server.start(port, 20260919u, 48, 16, roomCode, 10.0)) return 2;

    BotClient botA;
    BotClient botB;
    BotClient botBad;
    botBad.expectReject = true;
    if (!botA.connect(port, "BotA", roomCode) || !botB.connect(port, "BotB", roomCode) ||
        !botBad.connect(port, "BotBad", roomCode + 1)) {
        core::Logger::error("[SELFTEST] bot connect failed");
        return 2;
    }

    auto start = std::chrono::steady_clock::now();
    double elapsed = 0.0;
    double last = 0.0;
    while (elapsed < seconds) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        auto now = std::chrono::steady_clock::now();
        double t = std::chrono::duration<double>(now - start).count();
        double dt = t - last;
        last = t;
        elapsed = t;

        server.serviceNetwork();
        server.step(dt);
        botA.service(dt);
        botB.service(dt);
        botBad.service(dt);
    }

    bool ok = true;
    if (server.sim().players().size() != 2) {
        core::Logger::error("[SELFTEST] expected 2 players, got " +
                            std::to_string(server.sim().players().size()));
        ok = false;
    }
    if (botA.snapshots < 10 || botB.snapshots < 10) {
        core::Logger::error("[SELFTEST] snapshot counts A=" + std::to_string(botA.snapshots) +
                            " B=" + std::to_string(botB.snapshots));
        ok = false;
    }
    if (botA.objectCount <= 0) {
        core::Logger::error("[SELFTEST] no objects received in welcome");
        ok = false;
    }
    if (botBad.rejects <= 0) {
        core::Logger::error("[SELFTEST] wrong room code was not rejected");
        ok = false;
    }
    if (botA.monsterCount <= 0 || !botA.monsterMoved) {
        core::Logger::error("[SELFTEST] monster not synced/moved (count=" +
                            std::to_string(botA.monsterCount) + ")");
        ok = false;
    }
    const auto& players = server.sim().players();
    if (players.size() == 2) {
        if (players[0].pos.x == players[1].pos.x && players[0].pos.z == players[1].pos.z) {
            core::Logger::error("[SELFTEST] players did not separate");
            ok = false;
        }
    }
    if (server.sim().rejects() > 200) {
        core::Logger::error("[SELFTEST] too many speed rejections: " +
                            std::to_string(server.sim().rejects()));
        ok = false;
    }

    core::Logger::info(std::string("[SELFTEST] ") + (ok ? "PASS" : "FAIL") +
                       " players=" + std::to_string(server.sim().players().size()) +
                       " snapA=" + std::to_string(botA.snapshots) +
                       " snapB=" + std::to_string(botB.snapshots) +
                       " objects=" + std::to_string(botA.objectCount) +
                       " badReject=" + std::to_string(botBad.rejects) +
                       " monsters=" + std::to_string(botA.monsterCount) +
                       " monsterMoved=" + std::string(botA.monsterMoved ? "1" : "0") +
                       " speedRejects=" + std::to_string(server.sim().rejects()) +
                       " ticks=" + std::to_string(server.sim().tick()));

    botA.stop();
    botB.stop();
    botBad.stop();
    server.shutdown();
    return ok ? 0 : 1;
}
bool runQuestSelfTest() {
    sc::Sim sim;
    sim.generate(777u, 48);
    int pid = sim.addPlayer("QuestBot");

    auto stepN = [&](int n) {
        for (int i = 0; i < n; ++i) sim.step(0.05);
    };
    auto setInteract = [&](bool held) {
        sc::InputCmd c;
        c.interact = held;
        sim.setInput(pid, c);
    };
    auto grabCan = [&]() -> bool {
        for (const auto& o : sim.objects()) {
            if (o.type != sc::ObjType::FuelCan || o.taken || o.holder >= 0) continue;
            sim.debugTeleportPlayer(pid, o.pos.x, o.pos.z);
            setInteract(true);
            sim.step(0.05);
            setInteract(false);
            sim.step(0.05);
            return true;
        }
        return false;
    };
    auto isCarryingCan = [&]() -> bool {
        for (const auto& o : sim.objects()) {
            if (o.id >= 0 && o.type == sc::ObjType::FuelCan && o.holder == pid) return true;
        }
        return false;
    };
    auto channelAt = [&](sc::ObjType targetType, int maxSteps) -> bool {
        const sc::SimObject* target = nullptr;
        for (const auto& o : sim.objects()) {
            if (o.type != targetType) continue;
            if (targetType == sc::ObjType::Generator && o.charge >= sc::kGeneratorFuelNeed) continue;
            if (targetType == sc::ObjType::Vehicle && o.charge >= sc::kVehicleFuelNeed) continue;
            target = &o;
            break;
        }
        if (!target) return false;
        sim.debugTeleportPlayer(pid, target->pos.x, target->pos.z);
        setInteract(true);
        for (int i = 0; i < maxSteps; ++i) {
            sim.step(0.05);
            if (!isCarryingCan()) break;
        }
        setInteract(false);
        sim.step(0.05);
        return true;
    };

    for (int i = 0; i < 2; ++i) {
        if (!grabCan()) return false;
        if (!channelAt(sc::ObjType::Generator, 60)) return false;
    }
    if (!sim.generatorsPowered()) {
        core::Logger::error("[QUEST] generators not powered");
        return false;
    }
    for (int i = 0; i < sc::kVehicleFuelNeed; ++i) {
        if (!grabCan()) return false;
        if (!channelAt(sc::ObjType::Vehicle, 60)) return false;
    }

    const sc::SimObject* vehicle = nullptr;
    for (const auto& o : sim.objects()) {
        if (o.type == sc::ObjType::Vehicle) vehicle = &o;
    }
    if (!vehicle || vehicle->charge < sc::kVehicleFuelNeed) {
        core::Logger::error("[QUEST] vehicle not fueled");
        return false;
    }
    sim.debugTeleportPlayer(pid, vehicle->pos.x, vehicle->pos.z);
    stepN(40);

    bool extracted = false;
    for (const auto& p : sim.players()) {
        if (p.id == pid && p.extracted) extracted = true;
    }
    bool ok = extracted && sim.status() == 1;
    core::Logger::info(std::string("[QUEST] ") + (ok ? "PASS" : "FAIL") + " fuelLeft=" +
                       std::to_string(sim.fuelCansInWorld()) + " gensPowered=" +
                       std::string(sim.generatorsPowered() ? "1" : "0") + " vehicle=" +
                       std::to_string(vehicle->charge) + " extracted=" +
                       std::string(extracted ? "1" : "0") + " status=" +
                       std::to_string(sim.status()));
    return ok;
}
}

int main(int argc, char** argv) {
    core::Config::instance().load("config.ini");

    int port = std::stoi(core::Config::instance().get("server.port", "7777"));
    int mapSize = std::stoi(core::Config::instance().get("server.mapsize", "48"));
    int maxPlayers = std::stoi(core::Config::instance().get("server.maxplayers", "16"));
    unsigned int seed = static_cast<unsigned int>(
        std::stoul(core::Config::instance().get("server.seed", "20260919")));
    double selftest = std::stod(core::Config::instance().get("server.selftest", "0"));
    double aoiRadius = std::stod(core::Config::instance().get("server.aoi", "12"));
    uint32_t roomCode = static_cast<uint32_t>(
        std::stoul(core::Config::instance().get("server.roomcode", "0")));

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--selftest" && i + 1 < argc) {
            selftest = std::stod(argv[++i]);
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = static_cast<unsigned int>(std::stoul(argv[++i]));
        } else if (arg == "--room" && i + 1 < argc) {
            roomCode = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--aoi" && i + 1 < argc) {
            aoiRadius = std::stod(argv[++i]);
        }
    }

    if (roomCode == 0) {
        roomCode = 10000000u + (seed % 90000000u);
    }

    if (selftest > 0.0) {
        bool questOk = runQuestSelfTest();
        int netResult = runSelfTest(port, selftest);
        return (questOk && netResult == 0) ? 0 : 1;
    }

    Server server;
    if (!server.start(port, seed, mapSize, maxPlayers, roomCode, aoiRadius)) return 2;

    core::Logger::info("[SERVER] share code: " + std::to_string(roomCode) +
                       "  (client: SlashCoClient --room " + std::to_string(roomCode) + ")");
    core::Logger::info("[SERVER] running (Ctrl+C to stop)");
    auto last = std::chrono::steady_clock::now();
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - last).count();
        last = now;
        server.serviceNetwork();
        server.step(dt);
    }
    return 0;
}
