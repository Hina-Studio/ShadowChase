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
    roomCode_ = static_cast<uint32_t>(
        std::stoul(core::Config::instance().get("client.room", "0")));

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--server" && i + 1 < argc) {
            serverAddr_ = argv[++i];
        } else if (arg == "--name" && i + 1 < argc) {
            playerName_ = argv[++i];
        } else if (arg == "--room" && i + 1 < argc) {
            roomCode_ = static_cast<uint32_t>(std::stoul(argv[++i]));
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
    core::Logger::info("[CLIENT] connecting to " + serverAddr_ + ":" + std::to_string(serverPort_) +
                       " room=" + std::to_string(roomCode_));
    return true;
}

void ClientApp::rebuildDynamicGrid() {
    if (grid_.empty()) return;
    dynGrid_ = grid_;
    for (const auto& kv : netObjects_) {
        const sc::SnapshotObject& o = kv.second;
        if (o.type != static_cast<uint8_t>(sc::ObjType::Door)) continue;
        if ((o.flags & 0x01) != 0) continue;
        int gx = static_cast<int>(std::floor(o.x));
        int gz = static_cast<int>(std::floor(o.z));
        if (gx < 0 || gz < 0 || gx >= mapSize_ || gz >= mapSize_) continue;
        dynGrid_[static_cast<size_t>(gz) * static_cast<size_t>(mapSize_) + static_cast<size_t>(gx)] = 1;
    }
}

void ClientApp::handleEvents() {
    if (!host_) return;
    ENetEvent event;
    while (enet_host_service(host_, &event, 0) > 0) {
        if (event.type == ENET_EVENT_TYPE_CONNECT) {
            auto join = sc::encodeJoin(roomCode_, playerName_, SLASHCO_VERSION);
            ENetPacket* packet = enet_packet_create(join.data(), join.size(),
                                                    ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(peer_, 0, packet);
            connected_ = true;
            core::Logger::info("[CLIENT] connected, joining room " + std::to_string(roomCode_));
        } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
            connected_ = false;
            if (!rejected_) {
                core::Logger::warn("[CLIENT] disconnected");
            }
        } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            dbg_.recvPackets += 1;
            dbg_.recvBytes += static_cast<long long>(event.packet->dataLength);
            uint8_t type = event.packet->dataLength > 0 ? event.packet->data[0] : 0;
            if (type == static_cast<uint8_t>(sc::MsgType::Welcome)) {
                sc::WelcomeData welcome;
                if (sc::decodeWelcome(event.packet->data, event.packet->dataLength, welcome)) {
                    blocks_ = welcome.blocks;
                    seed_ = welcome.seed;
                    mapSize_ = welcome.mapSize;
                    playerId_ = welcome.playerId;
                    grid_.assign(static_cast<size_t>(mapSize_) * static_cast<size_t>(mapSize_), 0);
                    for (const auto& b : blocks_) {
                        if (b.x >= 0 && b.z >= 0 && b.x < mapSize_ && b.z < mapSize_) {
                            grid_[static_cast<size_t>(b.z) * static_cast<size_t>(mapSize_) +
                                  static_cast<size_t>(b.x)] = 1;
                        }
                    }
                    netObjects_.clear();
                    for (const auto& o : welcome.objects) {
                        sc::SnapshotObject so;
                        so.id = o.id;
                        so.type = o.type;
                        so.x = o.x;
                        so.z = o.z;
                        so.holder = o.holder;
                        so.flags = 0;
                        if (o.open) so.flags |= 0x01;
                        if (o.taken) so.flags |= 0x02;
                        netObjects_[o.id] = so;
                    }
                    rebuildDynamicGrid();
                    predicted_ = sc::Vec2{mapSize_ / 2.0, mapSize_ / 2.0};
                    core::Logger::info("[CLIENT] welcome id=" + std::to_string(playerId_) +
                                       " room=" + std::to_string(welcome.roomCode) + " seed=" +
                                       std::to_string(seed_) + " blocks=" +
                                       std::to_string(blocks_.size()) + " objects=" +
                                       std::to_string(netObjects_.size()));
                }
            } else if (type == static_cast<uint8_t>(sc::MsgType::Reject)) {
                uint8_t reason = 0;
                std::string text;
                if (sc::decodeReject(event.packet->data, event.packet->dataLength, reason, text)) {
                    rejected_ = true;
                    core::Logger::error("[CLIENT] rejected: " + text);
                    quit_ = true;
                }
            } else if (type == static_cast<uint8_t>(sc::MsgType::Snapshot)) {
                sc::Snapshot snap;
                if (sc::decodeSnapshot(event.packet->data, event.packet->dataLength, snap)) {
                    serverTick_ = snap.tick;
                    if (snap.baseline) {
                        netPlayers_.clear();
                        netObjects_.clear();
                    }
                    for (const auto& p : snap.players) {
                        netPlayers_[p.id] = p;
                    }
                    for (const auto& o : snap.objects) {
                        netObjects_[o.id] = o;
                    }
                    rebuildDynamicGrid();

                    auto it = netPlayers_.find(playerId_);
                    if (it != netPlayers_.end()) {
                        sc::Vec2 server{it->second.x, it->second.z};
                        double err = predicted_.distance(server);
                        if (err > 1.5) {
                            predicted_ = server;
                        } else if (err > 0.02) {
                            predicted_ = predicted_ + (server - predicted_) * 0.2;
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
    bool sprint = IsKeyDown(KEY_LEFT_SHIFT);
    bool interact = IsKeyPressed(KEY_E);

    if (IsGamepadAvailable(0)) {
        fwd += GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y) * -1.0;
        side += GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        sprint = sprint || IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_2);
        interact = interact || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        dbg_.padActive = true;
    } else {
        dbg_.padActive = false;
    }

    fwd = std::max(-1.0, std::min(1.0, fwd));
    side = std::max(-1.0, std::min(1.0, side));

    double fx = std::sin(yaw_);
    double fz = std::cos(yaw_);
    sc::InputCmd cmd;
    cmd.moveX = fx * fwd + (-fz) * side;
    cmd.moveZ = fz * fwd + fx * side;
    cmd.yaw = yaw_;
    cmd.sprint = sprint;
    cmd.interact = interact;
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

    if (IsGamepadAvailable(0)) {
        yaw_ -= GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X) * 2.4 * dt;
        pitch_ -= GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y) * 1.8 * dt;
    }
    if (pitch_ > 1.45) pitch_ = 1.45;
    if (pitch_ < -1.45) pitch_ = -1.45;

    const std::vector<unsigned char>& useGrid = dynGrid_.empty() ? grid_ : dynGrid_;
    if (useGrid.empty()) return;

    double fwd = (IsKeyDown(KEY_W) ? 1.0 : 0.0) - (IsKeyDown(KEY_S) ? 1.0 : 0.0);
    double side = (IsKeyDown(KEY_D) ? 1.0 : 0.0) - (IsKeyDown(KEY_A) ? 1.0 : 0.0);
    bool sprint = IsKeyDown(KEY_LEFT_SHIFT);
    if (IsGamepadAvailable(0)) {
        fwd += GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y) * -1.0;
        side += GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        sprint = sprint || IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_2);
    }
    fwd = std::max(-1.0, std::min(1.0, fwd));
    side = std::max(-1.0, std::min(1.0, side));

    double fx = std::sin(yaw_);
    double fz = std::cos(yaw_);
    sc::Vec2 dir{fx * fwd + (-fz) * side, fz * fwd + fx * side};
    if (dir.length() < 1e-6) return;

    bool carrying = false;
    for (const auto& kv : netObjects_) {
        if (kv.second.type == static_cast<uint8_t>(sc::ObjType::Crate) &&
            kv.second.holder == playerId_) {
            carrying = true;
            break;
        }
    }

    sc::InputCmd cmd;
    cmd.moveX = dir.x;
    cmd.moveZ = dir.z;
    cmd.sprint = sprint;
    sc::InputCmd clean = sc::sanitizeInput(cmd);
    double speed = clean.sprint ? sc::kSprintSpeed : sc::kWalkSpeed;
    if (carrying) speed *= 0.8;
    sc::Sim::moveOnGrid(predicted_, sc::Vec2{clean.moveX, clean.moveZ}.normalized(), speed, dt,
                        useGrid, mapSize_);
}

std::string ClientApp::interactionPrompt() const {
    double fx = std::sin(yaw_);
    double fz = std::cos(yaw_);
    const sc::SnapshotObject* best = nullptr;
    double bestD = 1.9;
    for (const auto& kv : netObjects_) {
        const sc::SnapshotObject& o = kv.second;
        double dx = o.x - predicted_.x;
        double dz = o.z - predicted_.z;
        double d = std::sqrt(dx * dx + dz * dz);
        if (d > bestD) continue;
        if (dx * fx + dz * fz < 0.0) continue;
        if (o.type == static_cast<uint8_t>(sc::ObjType::Pickup) && (o.flags & 0x02)) continue;
        best = &o;
        bestD = d;
    }
    if (!best) return "";
    if (best->type == static_cast<uint8_t>(sc::ObjType::Door)) {
        return (best->flags & 0x01) ? "[E] close door" : "[E] open door";
    }
    if (best->type == static_cast<uint8_t>(sc::ObjType::Crate)) {
        if (best->holder == playerId_) return "[E] drop crate";
        return "[E] carry crate (slower)";
    }
    return "[E] pick up supplies (+40 hp, +12 ammo)";
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

    for (const auto& kv : netObjects_) {
        const sc::SnapshotObject& o = kv.second;
        uint8_t type = o.type;
        if (type == static_cast<uint8_t>(sc::ObjType::Door)) {
            bool open = (o.flags & 0x01) != 0;
            Color c = open ? Color{90, 220, 130, 220} : Color{170, 120, 60, 255};
            DrawCube(Vector3{o.x, 1.5f, o.z}, 0.9f, 3.0f, 0.25f, c);
            DrawCubeWires(Vector3{o.x, 1.5f, o.z}, 0.9f, 3.0f, 0.25f, Color{40, 30, 20, 255});
        } else if (type == static_cast<uint8_t>(sc::ObjType::Crate)) {
            Color c = o.holder >= 0 ? Color{255, 210, 90, 255} : Color{190, 150, 90, 255};
            DrawCube(Vector3{o.x, 0.5f, o.z}, 0.8f, 1.0f, 0.8f, c);
            DrawCubeWires(Vector3{o.x, 0.5f, o.z}, 0.8f, 1.0f, 0.8f, Color{60, 40, 20, 255});
        } else if (type == static_cast<uint8_t>(sc::ObjType::Pickup)) {
            if ((o.flags & 0x02) != 0) continue;
            DrawSphere(Vector3{o.x, 0.45f, o.z}, 0.35f, Color{90, 230, 160, 255});
            DrawSphereWires(Vector3{o.x, 0.45f, o.z}, 0.35f, 8, 8, Color{30, 90, 60, 255});
        }
    }

    for (const auto& kv : netPlayers_) {
        const sc::SnapshotPlayer& p = kv.second;
        if (p.id == playerId_) continue;
        Vector3 a{p.x, 0.45f, p.z};
        Vector3 b{p.x, 1.25f, p.z};
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
    dbg_.remoteCount = static_cast<int>(netPlayers_.size()) - (playerId_ >= 0 ? 1 : 0);
    dbg_.seed = seed_;
    dbg_.mapSize = mapSize_;
    dbg_.blockCount = static_cast<int>(blocks_.size());
    dbg_.objectCount = static_cast<int>(netObjects_.size());
    dbg_.ownX = predicted_.x;
    dbg_.ownZ = predicted_.z;
    dbg_.connected = connected_;
    auto ownIt = netPlayers_.find(playerId_);
    if (ownIt != netPlayers_.end()) {
        dbg_.hp = ownIt->second.hp;
    }
    dbg_.carrying = -1;
    for (const auto& kv : netObjects_) {
        if (kv.second.type == static_cast<uint8_t>(sc::ObjType::Crate) &&
            kv.second.holder == playerId_) {
            dbg_.carrying = kv.second.id;
        }
    }

    DrawText(TextFormat("SlashCo M1  room %u  %s  tick %u  players %d", roomCode_,
                        connected_ ? "online" : "offline", serverTick_,
                        static_cast<int>(netPlayers_.size())),
             12, 10, 18, Color{220, 220, 230, 255});

    std::string prompt = interactionPrompt();
    if (!prompt.empty()) {
        int w = MeasureText(prompt.c_str(), 20);
        DrawText(prompt.c_str(), GetScreenWidth() / 2 - w / 2, GetScreenHeight() / 2 + 40, 20,
                 Color{255, 230, 140, 255});
    }

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
