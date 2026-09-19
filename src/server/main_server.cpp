#include <chrono>
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
};

class Server {
public:
    bool start(int port, unsigned int seed, int mapSize, int maxPlayers) {
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
        core::Logger::info("[SERVER] listening port=" + std::to_string(port) + " seed=" +
                           std::to_string(seed) + " map=" + std::to_string(mapSize));
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
        double acc = 0.0;
        sim_.step(dt);
        broadcastSnapshot();
        (void)acc;
    }

    void step(double dt) {
        accumulator_ += dt;
        while (accumulator_ >= kTickSeconds) {
            accumulator_ -= kTickSeconds;
            tickOnce(kTickSeconds);
        }
    }

    const sc::Sim& sim() const { return sim_; }
    sc::Sim& sim() { return sim_; }
    size_t clientCount() const { return clients_.size(); }

private:
    void handlePacket(ENetPeer* peer, const enet_uint8* data, size_t size) {
        uint8_t type = 0;
        if (size > 0) type = data[0];
        if (type == static_cast<uint8_t>(sc::MsgType::Join)) {
            std::string name;
            if (!sc::decodeJoin(data, size, name)) return;
            int id = sim_.addPlayer(name);
            ClientLink link;
            link.playerId = id;
            link.peer = peer;
            clients_[id] = link;

            auto welcome = sc::encodeWelcome(id, sim_.seed(), sim_.size(), sim_.blocks());
            ENetPacket* packet = enet_packet_create(welcome.data(), welcome.size(),
                                                    ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(peer, kChannelControl, packet);

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
        sc::Snapshot snap;
        snap.tick = sim_.tick();
        for (const auto& p : sim_.players()) {
            if (!p.alive) continue;
            sc::SnapshotPlayer sp;
            sp.id = p.id;
            sp.x = static_cast<float>(p.pos.x);
            sp.z = static_cast<float>(p.pos.z);
            sp.yaw = static_cast<float>(p.yaw);
            sp.hp = static_cast<float>(p.hp);
            sp.flags = p.sprinting ? 0x01 : 0x00;
            snap.players.push_back(sp);
        }
        auto payload = sc::encodeSnapshot(snap);
        for (const auto& kv : clients_) {
            ENetPacket* packet = enet_packet_create(payload.data(), payload.size(), 0);
            enet_peer_send(kv.second.peer, kChannelSnapshot, packet);
        }
        if (host_) enet_host_flush(host_);
    }

    ENetHost* host_ = nullptr;
    sc::Sim sim_;
    std::unordered_map<int, ClientLink> clients_;
    double accumulator_ = 0.0;
};

struct BotClient {
    ENetHost* host = nullptr;
    ENetPeer* peer = nullptr;
    int playerId = -1;
    int snapshots = 0;
    int welcomeCount = 0;
    double phase = 0.0;
    double inputTimer = 0.0;

    bool connect(int port, const std::string& name) {
        host = enet_host_create(nullptr, 1, 2, 0, 0);
        if (!host) return false;
        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = static_cast<enet_uint16>(port);
        enet_address_set_host(&address, "127.0.0.1");
        peer = enet_host_connect(host, &address, 2, 0);
        if (!peer) return false;
        pendingName = name;
        return true;
    }

    void service(double dt) {
        if (!host) return;
        ENetEvent event;
        while (enet_host_service(host, &event, 0) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                auto join = sc::encodeJoin(pendingName);
                ENetPacket* packet = enet_packet_create(join.data(), join.size(),
                                                        ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(peer, kChannelControl, packet);
                enet_host_flush(host);
            } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                uint8_t type = event.packet->dataLength > 0 ? event.packet->data[0] : 0;
                if (type == static_cast<uint8_t>(sc::MsgType::Welcome)) {
                    std::vector<sc::Block> blocks;
                    unsigned seed = 0;
                    int mapSize = 0;
                    if (sc::decodeWelcome(event.packet->data, event.packet->dataLength, playerId,
                                          seed, mapSize, blocks)) {
                        ++welcomeCount;
                        core::Logger::info("[BOT] welcome id=" + std::to_string(playerId) +
                                           " blocks=" + std::to_string(blocks.size()));
                    }
                } else if (type == static_cast<uint8_t>(sc::MsgType::Snapshot)) {
                    sc::Snapshot snap;
                    if (sc::decodeSnapshot(event.packet->data, event.packet->dataLength, snap)) {
                        ++snapshots;
                    }
                }
                enet_packet_destroy(event.packet);
            }
        }

        if (welcomeCount > 0) {
            inputTimer += dt;
            if (inputTimer >= 0.05) {
                inputTimer = 0.0;
                phase += 0.1;
                sc::InputCmd cmd;
                cmd.moveX = std::cos(phase);
                cmd.moveZ = std::sin(phase);
                cmd.yaw = phase;
                cmd.sprint = (static_cast<int>(phase) % 2) == 0;
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

    std::string pendingName;
};

int runSelfTest(int port, double seconds) {
    Server server;
    if (!server.start(port, 20260919u, 48, 16)) return 2;

    BotClient botA;
    BotClient botB;
    if (!botA.connect(port, "BotA") || !botB.connect(port, "BotB")) {
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
    const auto& players = server.sim().players();
    if (players.size() == 2) {
        double movedA = players[0].pos.distance(sc::Vec2{players[0].pos.x, players[0].pos.z});
        (void)movedA;
        if (players[0].pos.x == players[1].pos.x && players[0].pos.z == players[1].pos.z) {
            core::Logger::error("[SELFTEST] players did not separate");
            ok = false;
        }
    }

    core::Logger::info(std::string("[SELFTEST] ") + (ok ? "PASS" : "FAIL") +
                       " players=" + std::to_string(server.sim().players().size()) +
                       " snapA=" + std::to_string(botA.snapshots) +
                       " snapB=" + std::to_string(botB.snapshots) +
                       " ticks=" + std::to_string(server.sim().tick()));

    botA.stop();
    botB.stop();
    server.shutdown();
    return ok ? 0 : 1;
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

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--selftest" && i + 1 < argc) {
            selftest = std::stod(argv[++i]);
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = static_cast<unsigned int>(std::stoul(argv[++i]));
        }
    }

    if (selftest > 0.0) {
        return runSelfTest(port, selftest);
    }

    Server server;
    if (!server.start(port, seed, mapSize, maxPlayers)) return 2;

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
