#include "ClientApp.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

#include <raylib.h>

#ifdef SLASHCO_IMGUI
#include "rlImGui.h"
#endif

#include "DebugPanel.hpp"
#include "core/Config.hpp"
#include "core/Logger.hpp"

namespace {
double nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

Color colorForId(int id) {
    static const Color palette[] = {
        Color{90, 220, 130, 255}, Color{255, 190, 70, 255}, Color{90, 170, 255, 255},
        Color{230, 110, 220, 255}, Color{120, 230, 220, 255}, Color{255, 120, 110, 255},
    };
    return palette[static_cast<size_t>(std::abs(id)) % 6];
}
}

bool ClientApp::init(int argc, char** argv) {
    core::Config::instance().load("config.ini");
    serverAddr_ = core::Config::instance().get("client.server", "127.0.0.1:7777");
    playerName_ = core::Config::instance().get("client.name", "Player");

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--server" && i + 1 < argc) {
            serverAddr_ = argv[++i];
        } else if (arg == "--name" && i + 1 < argc) {
            playerName_ = argv[++i];
        }
    }
    auto colon = serverAddr_.rfind(':');
    if (colon != std::string::npos) {
        serverPort_ = std::stoi(serverAddr_.substr(colon + 1));
        serverAddr_ = serverAddr_.substr(0, colon);
    }

    int width = std::stoi(core::Config::instance().get("window.width", "1280"));
    int height = std::stoi(core::Config::instance().get("window.height", "720"));
    std::string title = core::Config::instance().get("window.title", "SlashCo");

    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(width, height, title.c_str());
    SetTargetFPS(144);
    DisableCursor();
#ifdef SLASHCO_IMGUI
    rlImGuiSetup(true);
#endif

    if (enet_initialize() != 0) {
        core::Logger::error("[CLIENT] enet init failed");
        return false;
    }
    host_ = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!host_) {
        core::Logger::error("[CLIENT] host create failed");
        return false;
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = static_cast<enet_uint16>(serverPort_);
    enet_address_set_host(&address, serverAddr_.c_str());
    peer_ = enet_host_connect(host_, &address, 2, 0);
    if (!peer_) {
        core::Logger::error("[CLIENT] connect failed");
        return false;
    }
    core::Logger::info("[CLIENT] connecting to " + serverAddr_ + ":" + std::to_string(serverPort_));
    return true;
}

void ClientApp::handleEvents() {
    if (!host_) return;
    ENetEvent event;
    while (enet_host_service(host_, &event, 0) > 0) {
        if (event.type == ENET_EVENT_TYPE_CONNECT) {
            auto join = sc::encodeJoin(playerName_);
            ENetPacket* packet = enet_packet_create(join.data(), join.size(),
                                                    ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(peer_, 0, packet);
            connected_ = true;
            core::Logger::info("[CLIENT] connected, joined as " + playerName_);
        } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
            connected_ = false;
            core::Logger::warn("[CLIENT] disconnected");
        } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            dbg_.recvPackets += 1;
            dbg_.recvBytes += static_cast<long long>(event.packet->dataLength);
            uint8_t type = event.packet->dataLength > 0 ? event.packet->data[0] : 0;
            if (type == static_cast<uint8_t>(sc::MsgType::Welcome)) {
                std::vector<sc::Block> blocks;
                unsigned int seed = 0;
                int mapSize = 0;
                int pid = -1;
                if (sc::decodeWelcome(event.packet->data, event.packet->dataLength, pid, seed,
                                      mapSize, blocks)) {
                    blocks_ = blocks;
                    seed_ = seed;
                    mapSize_ = mapSize;
                    playerId_ = pid;
                    grid_.assign(static_cast<size_t>(mapSize_) * static_cast<size_t>(mapSize_), 0);
                    for (const auto& b : blocks_) {
                        if (b.x >= 0 && b.z >= 0 && b.x < mapSize_ && b.z < mapSize_) {
                            grid_[static_cast<size_t>(b.z) * static_cast<size_t>(mapSize_) +
                                  static_cast<size_t>(b.x)] = 1;
                        }
                    }
                    predicted_ = sc::Vec2{mapSize_ / 2.0, mapSize_ / 2.0};
                    core::Logger::info("[CLIENT] welcome id=" + std::to_string(pid) + " seed=" +
                                       std::to_string(seed) + " map=" + std::to_string(mapSize) +
                                       " blocks=" + std::to_string(blocks_.size()));
                }
            } else if (type == static_cast<uint8_t>(sc::MsgType::Snapshot)) {
                sc::Snapshot snap;
                if (sc::decodeSnapshot(event.packet->data, event.packet->dataLength, snap)) {
                    snapPrev_ = snapCurr_;
                    snapPrevTime_ = snapCurrTime_;
                    snapCurr_ = snap;
                    snapCurrTime_ = nowSeconds();
                    dbg_.serverTick = snap.tick;
                    for (const auto& p : snap.players) {
                        if (p.id == playerId_) {
                            sc::Vec2 server{p.x, p.z};
                            double err = predicted_.distance(server);
                            if (err > 1.5) {
                                predicted_ = server;
                            } else if (err > 0.02) {
                                predicted_ = predicted_ + (server - predicted_) * 0.2;
                            }
                        }
                    }
                }
            }
            enet_packet_destroy(event.packet);
        }
    }
}

void ClientApp::sendInput(double dt) {
    if (!connected_ || !peer_) return;
    inputTimer_ += dt;
    if (inputTimer_ < 0.05) return;
    inputTimer_ = 0.0;

    double fwd = (IsKeyDown(KEY_W) ? 1.0 : 0.0) - (IsKeyDown(KEY_S) ? 1.0 : 0.0);
    double side = (IsKeyDown(KEY_D) ? 1.0 : 0.0) - (IsKeyDown(KEY_A) ? 1.0 : 0.0);
    if (fwd != 0.0) side = side;
    double yaw = yaw_;
    double fx = std::sin(yaw);
    double fz = std::cos(yaw);
    sc::InputCmd cmd;
    cmd.moveX = fx * fwd + (-fz) * side;
    cmd.moveZ = fz * fwd + (fx)*side;
    cmd.yaw = yaw;
    cmd.sprint = IsKeyDown(KEY_LEFT_SHIFT);
    cmd.interact = IsKeyDown(KEY_E);
    cmd.seq = ++seq_;

    auto payload = sc::encodeInput(cmd);
    ENetPacket* packet = enet_packet_create(payload.data(), payload.size(), 0);
    enet_peer_send(peer_, 1, packet);
    enet_host_flush(host_);
}

void ClientApp::updateLocal(double dt) {
    Vector2 md = GetMouseDelta();
    yaw_ -= md.x * 0.003;
    pitch_ -= md.y * 0.003;
    if (pitch_ > 1.45) pitch_ = 1.45;
    if (pitch_ < -1.45) pitch_ = -1.45;

    if (grid_.empty()) return;
    double fwd = (IsKeyDown(KEY_W) ? 1.0 : 0.0) - (IsKeyDown(KEY_S) ? 1.0 : 0.0);
    double side = (IsKeyDown(KEY_D) ? 1.0 : 0.0) - (IsKeyDown(KEY_A) ? 1.0 : 0.0);
    double fx = std::sin(yaw_);
    double fz = std::cos(yaw_);
    sc::Vec2 dir{fx * fwd + (-fz) * side, fz * fwd + fx * side};
    if (dir.length() < 1e-6) return;

    sc::InputCmd cmd;
    cmd.moveX = dir.x;
    cmd.moveZ = dir.z;
    cmd.sprint = IsKeyDown(KEY_LEFT_SHIFT);
    sc::InputCmd clean = sc::sanitizeInput(cmd);
    double speed = clean.sprint ? sc::kSprintSpeed : sc::kWalkSpeed;
    sc::Sim::moveOnGrid(predicted_, sc::Vec2{clean.moveX, clean.moveZ}.normalized(), speed, dt,
                        grid_, mapSize_);
}

void ClientApp::render() {
    double half = mapSize_ > 0 ? mapSize_ / 2.0 : 24.0;
    Vector3 eye{static_cast<float>(predicted_.x), 1.65f, static_cast<float>(predicted_.z)};
    float cp = static_cast<float>(std::cos(pitch_));
    Vector3 fwd{static_cast<float>(std::sin(yaw_)) * cp, static_cast<float>(std::sin(pitch_)),
                static_cast<float>(std::cos(yaw_)) * cp};

    BeginDrawing();
    ClearBackground(Color{18, 18, 24, 255});

    Camera3D cam{};
    cam.position = eye;
    cam.target = Vector3{eye.x + fwd.x, eye.y + fwd.y, eye.z + fwd.z};
    cam.up = Vector3{0.0f, 1.0f, 0.0f};
    cam.fovy = 72.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    BeginMode3D(cam);
    DrawPlane(Vector3{static_cast<float>(half), 0.0f, static_cast<float>(half)},
              Vector2{static_cast<float>(mapSize_), static_cast<float>(mapSize_)},
              Color{28, 28, 34, 255});
    DrawGrid(mapSize_, 1.0f);
    for (const auto& b : blocks_) {
        Vector3 c{static_cast<float>(b.x) + 0.5f, 1.5f, static_cast<float>(b.z) + 0.5f};
        DrawCube(c, 1.0f, 3.0f, 1.0f, Color{140, 105, 70, 255});
        DrawCubeWires(c, 1.0f, 3.0f, 1.0f, Color{80, 60, 40, 255});
    }

    double now = nowSeconds();
    double span = snapCurrTime_ - snapPrevTime_;
    double alpha = 0.0;
    if (span > 1e-4) {
        alpha = (now - 0.1 - snapPrevTime_) / span;
        alpha = std::max(0.0, std::min(1.0, alpha));
    }
    auto findPrev = [&](int id) -> const sc::SnapshotPlayer* {
        for (const auto& p : snapPrev_.players) {
            if (p.id == id) return &p;
        }
        return nullptr;
    };
    for (const auto& p : snapCurr_.players) {
        if (p.id == playerId_) continue;
        const sc::SnapshotPlayer* q = findPrev(p.id);
        float x = p.x;
        float z = p.z;
        if (q) {
            x = static_cast<float>(q->x + (p.x - q->x) * alpha);
            z = static_cast<float>(q->z + (p.z - q->z) * alpha);
        }
        Vector3 a{x, 0.45f, z};
        Vector3 b{x, 1.25f, z};
        DrawCapsule(a, b, 0.35f, 8, 8, colorForId(p.id));
    }
    EndMode3D();

    DrawLine(GetScreenWidth() / 2 - 8, GetScreenHeight() / 2, GetScreenWidth() / 2 + 8,
             GetScreenHeight() / 2, Color{230, 230, 230, 200});
    DrawLine(GetScreenWidth() / 2, GetScreenHeight() / 2 - 8, GetScreenWidth() / 2,
             GetScreenHeight() / 2 + 8, Color{230, 230, 230, 200});

    dbg_.fps = static_cast<float>(GetFPS());
    dbg_.frameMs = static_cast<float>(GetFrameTime() * 1000.0);
    dbg_.rttMs = peer_ ? peer_->roundTripTime : -1;
    dbg_.playerId = playerId_;
    dbg_.remoteCount = static_cast<int>(snapCurr_.players.size()) - (playerId_ >= 0 ? 1 : 0);
    dbg_.seed = seed_;
    dbg_.mapSize = mapSize_;
    dbg_.blockCount = static_cast<int>(blocks_.size());
    dbg_.ownX = predicted_.x;
    dbg_.ownZ = predicted_.z;
    dbg_.connected = connected_;

    DrawText(TextFormat("SlashCo M0  %s  tick %u  players %d", connected_ ? "online" : "offline",
                        dbg_.serverTick, static_cast<int>(snapCurr_.players.size())),
             12, 10, 18, Color{220, 220, 230, 255});
    DrawText("WASD move  SHIFT sprint  E interact  F1 panel  ESC quit", 12,
             GetScreenHeight() - 26, 16, Color{180, 180, 190, 255});

#ifdef SLASHCO_IMGUI
    rlImGuiBegin();
    client_debug::drawPanel(dbg_, showPanel_);
    rlImGuiEnd();
#endif

    EndDrawing();
}

void ClientApp::run() {
    while (!WindowShouldClose() && !quit_) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            quit_ = true;
        }
        if (IsKeyPressed(KEY_F1)) {
            showPanel_ = !showPanel_;
        }
        double dt = GetFrameTime();
        if (dt > 0.1) dt = 0.1;
        handleEvents();
        sendInput(dt);
        updateLocal(dt);
        render();
    }
}

void ClientApp::shutdown() {
    if (peer_ && host_) {
        enet_peer_disconnect(peer_, 0);
        ENetEvent event;
        while (enet_host_service(host_, &event, 200) > 0) {
            if (event.type == ENET_EVENT_TYPE_RECEIVE) enet_packet_destroy(event.packet);
        }
    }
    if (host_) {
        enet_host_destroy(host_);
        host_ = nullptr;
    }
    enet_deinitialize();
#ifdef SLASHCO_IMGUI
    rlImGuiShutdown();
#endif
    CloseWindow();
}
