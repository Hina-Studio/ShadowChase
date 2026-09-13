#include "../include/Game.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <sstream>
#include <string>
#include <thread>

#include "core/Loc.hpp"

#ifndef SLASHCO_VERSION
#define SLASHCO_VERSION "dev"
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

struct DummyAsset : public core::Asset {};

namespace {
bool parseInputMsg(const std::string& msg, int& id, game::Control& c) {
    if (msg.size() < 3 || msg[0] != 'I' || msg[1] != '|') return false;
    std::stringstream ss(msg.substr(2));
    std::string tok;
    if (!std::getline(ss, tok, '|')) return false;
    id = std::atoi(tok.c_str());
    if (!std::getline(ss, tok, '|')) return false;
    std::stringstream fs(tok);
    if (!std::getline(fs, tok, ',')) return false;
    c.move.x = std::stod(tok);
    if (!std::getline(fs, tok, ',')) return false;
    c.move.z = std::stod(tok);
    if (!std::getline(fs, tok, ',')) return false;
    c.interact = std::atoi(tok.c_str()) != 0;
    if (!std::getline(fs, tok, ',')) return false;
    c.sprint = std::atoi(tok.c_str()) != 0;
    return true;
}

int parseSurvivorId(const std::string& event) {
    auto p = event.find("Survivor#");
    if (p == std::string::npos) return -1;
    return std::atoi(event.c_str() + p + 9);
}

std::vector<game::KillerAI::Kind> parseKillerKinds(const std::string& kts) {
    std::vector<game::KillerAI::Kind> kinds;
    std::string token;
    for (char ch : kts + ",") {
        if (ch == ' ') continue;
        if (ch == ',') {
            if (!token.empty()) {
                if (token == "Whisper") kinds.push_back(game::KillerAI::Kind::Whisper);
                else if (token == "Herd") kinds.push_back(game::KillerAI::Kind::Herd);
                else if (token == "Butcher") kinds.push_back(game::KillerAI::Kind::Butcher);
                else if (token == "Warden") kinds.push_back(game::KillerAI::Kind::Warden);
                else kinds.push_back(game::KillerAI::Kind::Stalker);
                token.clear();
            }
        } else {
            token += ch;
        }
    }
    if (kinds.empty()) kinds.push_back(game::KillerAI::Kind::Stalker);
    return kinds;
}

double clampd(double v, double lo, double hi) {
    return std::max(lo, std::min(hi, v));
}
}

Game::Game() {}

void Game::init() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    core::Logger::info("Initializing game");
    core::Config::instance().load("config.ini");
    core::Logger::info("Config loaded");
    core::Loc::instance().setLocale(core::Config::instance().get("locale", "zh"));

    profile.load(core::Config::instance().get("profile.file", "profile.dat"));
    core::Logger::info("Profile: matches=" + std::to_string(profile.data().matches) +
                       " escapes=" + std::to_string(profile.data().escapes) +
                       " achievements=" +
                       std::to_string(static_cast<int>(profile.data().achievements.size())));

    std::string pstr = core::Config::instance().get("sim.players", "4");
    players = std::max(1, std::stoi(pstr));

    benchMatches = std::max(0, std::stoi(core::Config::instance().get("bench.matches", "0")));
    benchMode = benchMatches > 0;
    benchSelfTest = core::Config::instance().get("bench.selftest", "0") == "1";

    profEnabled = core::Config::instance().get("profile.enabled", "0") == "1";
    profInterval = std::stod(core::Config::instance().get("profile.interval", "2.0"));
    core::Profiler::instance().setEnabled(profEnabled);
    if (profEnabled) {
        core::Logger::info("Profiler enabled (interval " + std::to_string(profInterval) + "s)");
    }

    core::EventManager::instance().subscribe("GameStart", [](const std::string&) {
        core::Logger::info("Event GameStart received");
    });
    core::Logger::info("Subscribed to GameStart");

    auto asset = core::AssetManager::instance().load<DummyAsset>("dummy");
    (void)asset;
    core::Logger::info("Loaded dummy asset");

    std::string seedStr = core::Config::instance().get("sim.seed", "");
    if (seedStr.empty()) {
        simSeed = static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    } else {
        simSeed = std::stoull(seedStr);
    }
    core::Random::instance().seed(simSeed);
    core::Logger::info("Simulation seed: " + std::to_string(simSeed));

    RendererSettings rs;
    rs.width = std::max(320, std::stoi(core::Config::instance().get("window.width", "1280")));
    rs.height = std::max(240, std::stoi(core::Config::instance().get("window.height", "720")));
    rs.title = core::Config::instance().get("window.title", "SlashCo");
    rs.gpuPreset = core::Config::instance().get("gpu.preset", "auto");
    std::string msaa = core::Config::instance().get("render.msaa", "");
    if (!msaa.empty()) rs.msaa = std::stoi(msaa);
    std::string fpsCap = core::Config::instance().get("render.maxfps", "");
    if (!fpsCap.empty()) rs.maxFPS = std::stoi(fpsCap);
    std::string gridOpt = core::Config::instance().get("render.grid", "");
    if (!gridOpt.empty()) rs.grid = std::stoi(gridOpt);
    std::string vhsOpt = core::Config::instance().get("render.vhs", "");
    if (!vhsOpt.empty()) rs.vhs = std::stoi(vhsOpt);
    if (!benchMode) renderer.init(rs);
    if (!benchMode && core::Loc::instance().locale() == "zh" && !renderer.hasUnicodeFont()) {
        core::Logger::warn("CJK font unavailable, falling back to English UI");
        core::Loc::instance().setLocale("en");
    }

    audio.init(core::Config::instance().get("audio.enabled", "1") != "0" && !benchMode,
               std::stof(core::Config::instance().get("audio.volume", "0.6")));

    volumePercent = static_cast<int>(
        std::stof(core::Config::instance().get("audio.volume", "0.6")) * 100.0f + 0.5f);
    vhsOn = renderer.vhsEnabled();
    gridOn = renderer.gridEnabled();
    {
        int widths[3] = {1280, 1600, 1920};
        int heights[3] = {720, 900, 1080};
        resolutionIndex = 0;
        for (int i = 0; i < 3; ++i) {
            if (widths[i] == rs.width && heights[i] == rs.height) resolutionIndex = i;
        }
    }

    platform.init(core::Config::instance().get("steam.enabled", "0") == "1");

    {
        auto& cfg = core::Config::instance();
        auto& bal = world.balance();
        auto getD = [&](const char* key, double def) {
            std::string v = cfg.get(key, "");
            return v.empty() ? def : std::stod(v);
        };
        auto getI = [&](const char* key, int def) {
            std::string v = cfg.get(key, "");
            return v.empty() ? def : std::stoi(v);
        };
        bal.moveSpeed = getD("balance.moveSpeed", bal.moveSpeed);
        bal.sprintSpeed = getD("balance.sprintSpeed", bal.sprintSpeed);
        bal.staminaDrain = getD("balance.staminaDrain", bal.staminaDrain);
        bal.staminaRegen = getD("balance.staminaRegen", bal.staminaRegen);
        bal.repairTime = getD("balance.repairTime", bal.repairTime);
        bal.downWindow = getD("balance.downWindow", bal.downWindow);
        bal.damageScale = getD("balance.damageScale", bal.damageScale);
        bal.angerGainScale = getD("balance.angerGainScale", bal.angerGainScale);
        bal.generatorFuel = getI("balance.generatorFuel", bal.generatorFuel);
    }

    NetConfig ncfg;
    ncfg.mode = core::Config::instance().get("net.mode", "off");
    ncfg.address = core::Config::instance().get("net.address", "127.0.0.1");
    ncfg.port = std::stoi(core::Config::instance().get("net.port", "7777"));
    spectate = core::Config::instance().get("net.spectate", "0") == "1";

    std::string candStr = core::Config::instance().get("net.candidates", "");
    bool haveCandidates = !candStr.empty();
    if (haveCandidates) {
        connectivity.importString(candStr);
        for (const auto& c : connectivity.candidates()) {
            ncfg.endpoints.push_back(c.address + ":" + std::to_string(c.port));
        }
    }

    network.start(ncfg);
    core::Logger::info("Network: " + network.status());

    if (network.isHost()) {
        lan.startHostBeacon(core::Config::instance().get("net.name", "ShadowChase Host"), ncfg.port,
                            std::stoi(core::Config::instance().get("net.beacon_port", "7778")));
    }

    if (ncfg.mode != "off" && !ncfg.mode.empty()) {
        if (!haveCandidates) {
            connectivity.probe(core::Config::instance().get("net.stun", "stun.l.google.com:19302"),
                               ncfg.port);
        }
        core::Logger::info("Connectivity: " + connectivity.summary());
        core::Logger::info("Candidates export: " + connectivity.exportString());
    }

    states.add("Menu",
        [this](double) {
            core::Logger::info("State: Menu (enter)");
            platform.setRichPresence("state", "menu");
        },
        [this](double) {
            const InputState& is = input.state();
            if (is.lobbyOpen) {
                audio.playEvent("ui");
                states.change("Lobby");
                return;
            }
            if (is.replayOpen) {
                audio.playEvent("ui");
                states.change("Replay");
                return;
            }
            if (is.tutorialOpen) {
                audio.playEvent("ui");
                states.change("Tutorial");
                return;
            }
            if (is.settingsOpen) {
                audio.playEvent("ui");
                states.change("Settings");
                return;
            }
            if (is.quit) {
                quit = true;
                return;
            }
            if (is.confirm) {
                audio.playEvent("ui");
                states.change("Playing");
            }
        },
        [](double) {});

    states.add("Settings",
        [this](double) { core::Logger::info("State: Settings (enter)"); },
        [this](double) {
            const InputState& is = input.state();
            if (is.navUp) settingsSelected = (settingsSelected + 6) % 7;
            if (is.navDown) settingsSelected = (settingsSelected + 1) % 7;
            if (is.navRight) applySetting(1);
            if (is.navLeft) applySetting(-1);
            if (is.confirm) applySetting(1);
            if (is.quit) {
                saveSettings();
                states.change("Menu");
            }
        },
        [](double) { core::Logger::info("State: Settings (exit)"); });

    states.add("Playing",
        [this](double) {
            core::Logger::info("State: Playing (enter)");
            std::string kts = core::Config::instance().get("killer.types",
                                 core::Config::instance().get("killer.type", "Stalker"));
            std::vector<game::KillerAI::Kind> kinds = parseKillerKinds(kts);

            std::string mapName = core::Config::instance().get("map.name", "Factory");
            std::string gameMode = core::Config::instance().get("mode.name", "Classic");
            std::string rotation = core::Config::instance().get("map.rotation", "");
            if (!rotation.empty()) {
                std::vector<std::string> maps;
                std::string token;
                for (char ch : rotation + ",") {
                    if (ch == ' ') continue;
                    if (ch == ',') {
                        if (!token.empty()) {
                            maps.push_back(token);
                            token.clear();
                        }
                    } else {
                        token += ch;
                    }
                }
                if (!maps.empty()) {
                    bool randomMap = core::Config::instance().get("map.random", "0") == "1";
                    size_t idx = randomMap
                                     ? static_cast<size_t>(core::Random::instance().range(
                                           0, static_cast<int>(maps.size())))
                                     : static_cast<size_t>(matchCounter % maps.size());
                    mapName = maps[idx];
                }
            }
            ++matchCounter;

            world.configure(players, kinds, mapName, gameMode);
            clientMode = network.isClient();
            netSendAccum = 0.0;
            netInputAccum = 0.0;
            lastSnapshot.clear();
            if (clientMode && spectate) {
                world.bindLocalPlayer(-1);
                spectateIndex = 0;
                spectateId = -1;
                renderer.setSpectateId(-1);
            } else if (clientMode) {
                clientId = players >= 2 ? 1 : 0;
                world.bindLocalPlayer(clientId);
            } else {
                world.bindLocalPlayer(0);
            }
            world.setPrediction(clientMode);
            if (network.isHost()) {
                lan.updateBeacon(core::Config::instance().get("net.name", "ShadowChase Host") + "@" +
                                     mapName,
                                 std::stoi(core::Config::instance().get("net.port", "7777")));
            }
            platform.setRichPresence("map", mapName);
            platform.setRichPresence("state", "playing");

            replayRecording = !clientMode;
            trapTriggered = false;
            replayRec = game::ReplayData{};
            replayRec.seed = simSeed;
            replayRec.players = players;
            replayRec.map = mapName;
            replayRec.mode = gameMode;
            replayRec.killerTypes = kts;
            {
                const auto& b = world.balance();
                replayRec.balMove = b.moveSpeed;
                replayRec.balSprint = b.sprintSpeed;
                replayRec.balDrain = b.staminaDrain;
                replayRec.balRegen = b.staminaRegen;
                replayRec.balRepair = b.repairTime;
                replayRec.balDown = b.downWindow;
                replayRec.balDmg = b.damageScale;
                replayRec.balAnger = b.angerGainScale;
                replayRec.balFuel = b.generatorFuel;
            }
            lastLoggedSecond = -1;
            core::Logger::info("Match configured with " + std::to_string(players) +
                               " survivors, map " + mapName + ", mode " + gameMode + ", killers: " +
                               kts + ", seed " + std::to_string(simSeed) +
                               (clientMode ? " [CLIENT: awaiting host snapshots]"
                                           : " [LOCAL/HOST authority]") +
                               ", you control Survivor#0 (WASD/arrows move, SHIFT sprint, E repair)");
        },
        [this](double dt) {
            if (input.state().panelToggle) {
                balancePanel = !balancePanel;
                audio.playEvent("ui");
            }
            if (balancePanel) {
                const InputState& is = input.state();
                if (is.navUp) balanceSelected = (balanceSelected + 10) % 11;
                if (is.navDown) balanceSelected = (balanceSelected + 1) % 11;
                if (is.navLeft) adjustBalance(-1);
                if (is.navRight) adjustBalance(1);
                if (is.confirm) {
                    if (balanceSelected == 9) {
                        saveBalance();
                    } else if (balanceSelected == 10) {
                        balancePanel = false;
                    } else {
                        resetBalanceRow(balanceSelected);
                    }
                }
                if (is.quit) balancePanel = false;
                return;
            }

            if (clientMode) {
                for (const auto& msg : network.takeReceived()) {
                    if (!msg.empty() && msg[0] == 'S') {
                        world.applySnapshot(msg);
                    }
                }

                if (spectate) {
                    std::vector<int> ids;
                    for (const auto& s : world.survivors()) {
                        if (!s.eliminated && !s.escaped) ids.push_back(s.id);
                    }
                    if (!ids.empty()) {
                        if (input.state().spectateNext) {
                            spectateIndex = (spectateIndex + 1) % ids.size();
                        }
                        if (input.state().spectatePrev) {
                            spectateIndex = (spectateIndex + ids.size() - 1) % ids.size();
                        }
                        if (spectateIndex >= ids.size()) spectateIndex = 0;
                        spectateId = ids[spectateIndex];
                    } else {
                        spectateId = -1;
                    }
                    renderer.setSpectateId(spectateId);
                } else {
                    world.predictLocal(dt, localCtrl);
                }

                world.smoothNetwork(dt);
                netInputAccum += dt;
                if (!spectate && netInputAccum >= 0.05 && network.isConnected()) {
                    netInputAccum = 0.0;
                    std::string im = "I|" + std::to_string(clientId) + "|" +
                                     std::to_string(localCtrl.move.x) + "," +
                                     std::to_string(localCtrl.move.z) + "," +
                                     (localCtrl.interact ? "1" : "0") + "," +
                                     (localCtrl.sprint ? "1" : "0");
                    network.send(im);
                }
            } else {
                for (const auto& msg : network.takeReceived()) {
                    if (!msg.empty() && msg[0] == 'I') {
                        int rid = 0;
                        game::Control rc;
                        if (parseInputMsg(msg, rid, rc)) {
                            world.setRemoteControl(rid, rc);
                            if (replayRecording) {
                                game::ReplayData::RemoteInput ri;
                                ri.t = static_cast<float>(world.clock());
                                ri.id = rid;
                                ri.mx = static_cast<float>(rc.move.x);
                                ri.mz = static_cast<float>(rc.move.z);
                                ri.interact = rc.interact;
                                ri.sprint = rc.sprint;
                                replayRec.remotes.push_back(ri);
                            }
                        }
                    }
                }
                world.update(dt, localCtrl);
                for (const auto& e : world.takeEvents()) {
                    core::Logger::info(e);
                    audio.playEvent(e);
                    trackAchievement(e);
                    handleEventFx(e);
                }
                if (replayRecording) {
                    game::ReplayFrame rf;
                    rf.dt = static_cast<float>(dt);
                    rf.mx = static_cast<float>(localCtrl.move.x);
                    rf.mz = static_cast<float>(localCtrl.move.z);
                    rf.interact = localCtrl.interact;
                    rf.sprint = localCtrl.sprint;
                    replayRec.frames.push_back(rf);
                }
                if (network.isHost()) {
                    netSendAccum += dt;
                    if (netSendAccum >= 0.05) {
                        float elapsed = static_cast<float>(netSendAccum);
                        netSendAccum = 0.0;
                        std::string snap = world.serializeSnapshot();
                        if (snap != lastSnapshot) {
                            lastSnapshot = snap;
                            network.send(snap);
                            if (replayRecording) {
                                game::ReplayData::NetFrame nf;
                                nf.dt = elapsed;
                                nf.data = snap;
                                replayRec.netFrames.push_back(nf);
                                replayRec.network = true;
                            }
                        }
                    }
                }
            }
            int sec = static_cast<int>(world.clock());
            if (sec != lastLoggedSecond) {
                lastLoggedSecond = sec;
                logMatch();
            }
            if (world.over()) states.change("GameOver");
        },
        [](double) { core::Logger::info("State: Playing (exit)"); });

    states.add("Lobby",
        [this](double) {
            core::Logger::info("State: Lobby (enter)");
            lobbySelected = 0;
            lan.startClient(std::stoi(core::Config::instance().get("net.beacon_port", "7778")));
        },
        [this](double) {
            lan.poll();
            auto servers = lan.servers();
            if (!servers.empty()) {
                int count = static_cast<int>(servers.size());
                if (input.state().navUp) lobbySelected = (lobbySelected + count - 1) % count;
                if (input.state().navDown) lobbySelected = (lobbySelected + 1) % count;
                if (lobbySelected >= count) lobbySelected = 0;

                if (input.state().confirm) {
                    const LanServer& srv = servers[static_cast<size_t>(lobbySelected)];
                    NetConfig cfg;
                    cfg.mode = "client";
                    cfg.address = srv.address;
                    cfg.port = srv.port;
                    cfg.endpoints.push_back(srv.address + ":" + std::to_string(srv.port));
                    network.shutdown();
                    network.start(cfg);
                    core::Logger::info("Joining LAN server " + srv.name + " @ " + srv.address + ":" +
                                       std::to_string(srv.port));
                    audio.playEvent("ui");
                    states.change("Playing");
                }
            }
            if (input.state().quit) {
                states.change("Menu");
            }
        },
        [this](double) { lan.stop(); });

    states.add("Replay",
        [this](double) {
            core::Logger::info("State: Replay (enter)");
            std::string path = core::Config::instance().get("replay.file", "replays/last.rep");
            if (!replayPlay.load(path)) {
                core::Logger::warn("Replay load failed: " + path);
                states.change("Menu");
                return;
            }
            core::Random::instance().seed(replayPlay.seed);

            auto& b = world.balance();
            if (replayPlay.balMove != 0.0) b.moveSpeed = replayPlay.balMove;
            if (replayPlay.balSprint != 0.0) b.sprintSpeed = replayPlay.balSprint;
            if (replayPlay.balDrain != 0.0) b.staminaDrain = replayPlay.balDrain;
            if (replayPlay.balRegen != 0.0) b.staminaRegen = replayPlay.balRegen;
            if (replayPlay.balRepair != 0.0) b.repairTime = replayPlay.balRepair;
            if (replayPlay.balDown != 0.0) b.downWindow = replayPlay.balDown;
            if (replayPlay.balDmg != 0.0) b.damageScale = replayPlay.balDmg;
            if (replayPlay.balAnger != 0.0) b.angerGainScale = replayPlay.balAnger;
            if (replayPlay.balFuel > 0) b.generatorFuel = replayPlay.balFuel;

            world.configure(replayPlay.players, parseKillerKinds(replayPlay.killerTypes),
                            replayPlay.map, replayPlay.mode);
            world.syncBalance();
            world.bindLocalPlayer(0);
            world.setPrediction(false);
            replayIndex = 0;
            remoteIndex = 0;
            replayFinished = false;
            renderer.setTutorialMarker(false, 0.0, 0.0);
        },
        [this](double) {
            if (replayFinished) {
                if (input.state().confirm || input.state().quit) states.change("Menu");
                return;
            }
            if (input.state().quit) {
                states.change("Menu");
                return;
            }
            if (replayPlay.network) {
                if (replayIndex >= replayPlay.netFrames.size()) {
                    replayFinished = true;
                    return;
                }
                const game::ReplayData::NetFrame& nf = replayPlay.netFrames[replayIndex++];
                world.applySnapshot(nf.data);
                world.smoothNetwork(nf.dt);
                return;
            }

            while (remoteIndex < replayPlay.remotes.size() &&
                   replayPlay.remotes[remoteIndex].t <= static_cast<float>(world.clock()) + 1e-4f) {
                const game::ReplayData::RemoteInput& ri = replayPlay.remotes[remoteIndex++];
                game::Control rc;
                rc.move = game::Vec3{ri.mx, 0.0, ri.mz};
                rc.interact = ri.interact;
                rc.sprint = ri.sprint;
                world.setRemoteControl(ri.id, rc);
            }

            if (replayIndex >= replayPlay.frames.size()) {
                replayFinished = true;
                return;
            }
            const game::ReplayFrame& f = replayPlay.frames[replayIndex++];
            game::Control c;
            c.move = game::Vec3{f.mx, 0.0, f.mz};
            c.interact = f.interact;
            c.sprint = f.sprint;
            world.update(f.dt, c);
            for (const auto& e : world.takeEvents()) {
                core::Logger::info(e);
                audio.playEvent(e);
                handleEventFx(e);
            }
            if (world.over()) replayFinished = true;
        },
        [](double) {});

    states.add("Tutorial",
        [this](double) {
            core::Logger::info("State: Tutorial (enter)");
            world.configureTutorial();
            world.bindLocalPlayer(0);
            localCtrl = game::Control{};
            tutorialStep = 0;
            tutorialComplete = false;
            tutorialTimer = 0.0;
            renderer.setTutorialMarker(true, 0.0, -1.0);
            audio.playEvent("ui");
        },
        [this](double dt) {
            world.update(dt, localCtrl);
            for (const auto& e : world.takeEvents()) {
                core::Logger::info(e);
                audio.playEvent(e);
                handleEventFx(e);
            }
            const game::Survivor* lp = world.localSurvivor();
            if (!lp) return;

            if (!tutorialComplete) {
                if (tutorialStep == 0 && lp->position.distance(game::Vec3{0.0, 0.0, -1.0}) <= 0.9) {
                    tutorialStep = 1;
                    renderer.setTutorialMarker(true, 0.0, 2.0);
                    audio.playEvent("ui");
                } else if (tutorialStep == 1 && !world.generators().empty() &&
                           world.generators()[0].activated) {
                    tutorialStep = 2;
                    renderer.setTutorialMarker(true, 10.5, 0.0);
                    audio.playEvent("ui");
                } else if (tutorialStep == 2 && lp->escaped) {
                    tutorialComplete = true;
                    tutorialTimer = 0.0;
                    renderer.setTutorialMarker(false, 0.0, 0.0);
                    core::Config::instance().set("tutorial.done", "1");
                    core::Config::instance().save("config.ini");
                    audio.playEvent("outcome");
                }
            } else {
                tutorialTimer += dt;
                if (input.state().confirm || tutorialTimer > 4.0) {
                    states.change("Menu");
                }
            }

            if (input.state().quit) {
                renderer.setTutorialMarker(false, 0.0, 0.0);
                states.change("Menu");
            }
        },
        [this](double) { renderer.setTutorialMarker(false, 0.0, 0.0); });

    states.add("GameOver",
        [this](double) {
            core::Logger::info("Result: " + world.result());
            const game::MatchStats& st = world.stats();
            core::Logger::info("Stats: time=" + std::to_string(static_cast<int>(st.matchTime)) +
                               "s escaped=" + std::to_string(st.survivorsEscaped) +
                               " eliminated=" + std::to_string(st.survivorsEliminated) +
                               " gens=" + std::to_string(st.generatorsActivated) +
                               " rescues=" + std::to_string(st.rescues) +
                               " items=" + std::to_string(st.itemsUsed) +
                               " charms=" + std::to_string(st.charmsUsed) +
                               " damageTaken=" + std::to_string(st.damageDealt) +
                               " topKiller=" + st.topKiller);
            ++sessionMatches;
            sessionEscapes += st.survivorsEscaped;
            sessionEliminated += st.survivorsEliminated;
            profile.addMatch(st.survivorsEscaped, st.survivorsEliminated,
                             world.result() == "SurvivorsWin", st.matchTime);
            for (const auto& kv : st.killsByKiller) {
                profile.addKill(kv.first, kv.second);
            }
            profile.save(core::Config::instance().get("profile.file", "profile.dat"));
            if (world.result() == "SurvivorsWin") {
                platform.unlockAchievement("ACH_TEAM_ESCAPE");
                profile.unlock("ACH_TEAM_ESCAPE");
                if (st.downs == 0) {
                    platform.unlockAchievement("ACH_NO_DOWNS");
                    profile.unlock("ACH_NO_DOWNS");
                }
                if (st.matchTime > 0.0 && st.matchTime <= 60.0) {
                    platform.unlockAchievement("ACH_FAST_ESCAPE");
                    profile.unlock("ACH_FAST_ESCAPE");
                }
                if (trapTriggered) {
                    platform.unlockAchievement("ACH_TRAP_SURVIVOR");
                    profile.unlock("ACH_TRAP_SURVIVOR");
                }
            }
            platform.setRichPresence("state", "gameover");
            if (replayRecording) {
                finishReplayRecording();
                replayRecording = false;
            }
            audio.playEvent("outcome");
        },
        [this](double) {
            if (input.state().restart) {
                overElapsed = 0.0;
                states.change("Playing");
            }
            if (input.state().quit) {
                quit = true;
            }
        },
        [](double) {});

    core::Logger::info("Game initialization complete");
}

void Game::run() {
    if (benchMode) {
        runBenchmark();
        return;
    }

    core::Logger::info("Game loop started");
    core::EventManager::instance().publish("GameStart");
    states.change("Menu");

    core::Timer timer;
    while (!quit) {
        {
            core::Profiler::Scope pScope("loop.input");
            timer.tick();
            input.poll();
            network.poll();
            platform.runCallbacks();
        }
        const InputState& is = input.state();
        game::Control c;
        c.move.x = (is.moveRight ? 1.0 : 0.0) - (is.moveLeft ? 1.0 : 0.0);
        c.move.z = (is.moveDown ? 1.0 : 0.0) - (is.moveUp ? 1.0 : 0.0);
        c.interact = is.interact;
        c.sprint = is.sprint;
        localCtrl = c;

        int slot = -1;
        if (is.slot1) slot = 0;
        else if (is.slot2) slot = 1;
        else if (is.slot3) slot = 2;
        if (states.current() == "Playing") {
            const game::Survivor* lp = world.localSurvivor();
            if (lp) {
                if (slot >= 0) world.useLocalItem(slot);
                if (is.buyCharm) world.buyCharm(lp->id, false);
                if (is.buyGold) world.buyCharm(lp->id, true);
            }
        }

        double raw = timer.deltaSeconds();
        if (raw > 0.1) raw = 0.1;
        if (slowTimer > 0.0) slowTimer -= raw;
        timeScale = slowTimer > 0.0 ? 0.35 : 1.0;
        double dt = raw * 3.0 * timeScale;
        {
            core::Profiler::Scope pScope("loop.world");
            states.update(dt);
        }

        std::string status = "State: " + states.current() + "  seed=" + std::to_string(simSeed);
        if (world.over()) status += "  -  " + world.result();

        uiFrame.mode = "none";
        uiFrame.lines.clear();
        uiFrame.options.clear();
        uiFrame.selected = -1;

        auto fmt2 = [](double v) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.2f", v);
            return std::string(buf);
        };

        if (states.current() == "Lobby") {
            uiFrame.mode = "settings";
            uiFrame.title = core::Loc::instance().t("lobby.title");
            uiFrame.subtitle = core::Loc::instance().t("lobby.hint");
            auto servers = lan.servers();
            if (servers.empty()) {
                uiFrame.options = {core::Loc::instance().t("lobby.empty")};
                uiFrame.selected = -1;
            } else {
                for (const auto& srv : servers) {
                    uiFrame.options.push_back(srv.name + "   " + srv.address + ":" +
                                              std::to_string(srv.port));
                }
                uiFrame.selected = lobbySelected;
            }
        } else if (states.current() == "Replay") {
            uiFrame.mode = "tutorial";
            if (replayFinished) {
                uiFrame.title = core::Loc::instance().t("replay.end");
                uiFrame.subtitle = core::Loc::instance().t("tutorial.return");
            } else {
                uiFrame.title = core::Loc::instance().t("replay.title");
                uiFrame.subtitle = "seed " + std::to_string(replayPlay.seed) + "   frame " +
                                   std::to_string(replayIndex) + "/" +
                                   std::to_string(replayPlay.network
                                                      ? replayPlay.netFrames.size()
                                                      : replayPlay.frames.size()) +
                                   "   map " + replayPlay.map +
                                   (replayPlay.network ? "   [net snapshots]" : "   [inputs]");
                uiFrame.lines = {core::Loc::instance().t("replay.stop")};
            }
        } else if (states.current() == "Tutorial") {
            uiFrame.mode = "tutorial";
            if (tutorialComplete) {
                uiFrame.title = core::Loc::instance().t("tutorial.complete");
                uiFrame.subtitle = core::Loc::instance().t("tutorial.return");
            } else {
                uiFrame.title = core::Loc::instance().t("tutorial.title");
                if (tutorialStep == 0) {
                    uiFrame.subtitle = core::Loc::instance().t("tutorial.step1");
                } else if (tutorialStep == 1) {
                    uiFrame.subtitle = core::Loc::instance().t("tutorial.step2");
                } else {
                    uiFrame.subtitle = core::Loc::instance().t("tutorial.step3");
                }
                uiFrame.lines = {core::Loc::instance().t("tutorial.skip")};
            }
        } else if (states.current() == "Menu") {
            uiFrame.mode = "menu";
            uiFrame.title = "SHADOW CHASE";
            uiFrame.subtitle = core::Loc::instance().t("menu.subtitle");
            uiFrame.lines = {
                core::Loc::instance().t("menu.controls"),
                core::Loc::instance().t("menu.goal"),
                core::Loc::instance().t("menu.killers") + core::Config::instance().get("killer.types", "Stalker"),
                core::Loc::instance().t("menu.map") + core::Config::instance().get("map.name", "Factory") +
                    "   (Factory/Farmyard/HighSchool/Hospital/Research/Mansion/Subway/Sewer)",
                core::Loc::instance().t("menu.mode") + core::Config::instance().get("mode.name", "Classic") +
                    "   (Classic/Blackout)",
                core::Loc::instance().t("menu.rotation") + core::Config::instance().get("map.rotation", "(fixed)"),
                core::Loc::instance().t("menu.seed") + std::to_string(simSeed) +
                    "  (set sim.seed= to reproduce)",
                core::Loc::instance().t("menu.session") + std::to_string(sessionMatches) + "  escapes " +
                    std::to_string(sessionEscapes) + "  lost " +
                    std::to_string(sessionEliminated),
                core::Loc::instance().t("menu.network") + network.status(),
                std::string("spectator: ") + (spectate ? "ON" : "OFF"),
                core::Loc::instance().t("menu.candidates") +
                    std::to_string(static_cast<int>(connectivity.candidates().size())),
                core::Loc::instance().t("menu.platform") + platform.backendName() + "   achievements " +
                    std::to_string(static_cast<int>(platform.unlockedAchievements().size())),
                std::string("version ") + SLASHCO_VERSION,
                "lifetime: matches " + std::to_string(profile.data().matches) + "  wins " +
                    std::to_string(profile.data().survivorWins) + "  escapes " +
                    std::to_string(profile.data().escapes) + "  lost " +
                    std::to_string(profile.data().eliminated) + "  ach " +
                    std::to_string(static_cast<int>(profile.data().achievements.size())),
                core::Loc::instance().t("menu.start"),
                core::Loc::instance().t("menu.tutorial"),
                core::Loc::instance().t("menu.lobby"),
                core::Loc::instance().t("menu.replay"),
                core::Loc::instance().t("menu.settings"),
                core::Loc::instance().t("menu.quit"),
            };
        } else if (states.current() == "Settings") {
            uiFrame.mode = "settings";
            uiFrame.title = core::Loc::instance().t("settings.title");
            uiFrame.subtitle = core::Loc::instance().t("settings.hint");
            static const char* resNames[3] = {"1280x720", "1600x900", "1920x1080"};
            uiFrame.options = {
                core::Loc::instance().t("settings.resolution") + ": " + resNames[resolutionIndex],
                core::Loc::instance().t("settings.volume") + ": " + std::to_string(volumePercent),
                core::Loc::instance().t("settings.vhs") + ": " + (vhsOn ? "ON" : "OFF"),
                core::Loc::instance().t("settings.grid") + ": " + (gridOn ? "ON" : "OFF"),
                core::Loc::instance().t("settings.msaa"),
                core::Loc::instance().t("settings.language") + ": " + core::Loc::instance().locale(),
                core::Loc::instance().t("settings.back"),
            };
            uiFrame.selected = settingsSelected;
        } else if (states.current() == "Playing" && balancePanel) {
            const auto& b = world.balance();
            uiFrame.mode = "balance";
            uiFrame.title = core::Loc::instance().t("balance.title");
            uiFrame.subtitle = core::Loc::instance().t("balance.hint");
            uiFrame.options = {
                "MoveSpeed: " + fmt2(b.moveSpeed),
                "SprintSpeed: " + fmt2(b.sprintSpeed),
                "StaminaDrain: " + fmt2(b.staminaDrain),
                "StaminaRegen: " + fmt2(b.staminaRegen),
                "RepairTime: " + fmt2(b.repairTime),
                "DownWindow: " + fmt2(b.downWindow),
                "DamageScale: " + fmt2(b.damageScale),
                "AngerGainScale: " + fmt2(b.angerGainScale),
                "GeneratorFuel: " + std::to_string(b.generatorFuel),
                core::Loc::instance().t("balance.save"),
                core::Loc::instance().t("balance.close"),
            };
            uiFrame.selected = balanceSelected;
        } else if (states.current() == "GameOver") {
            const game::MatchStats& st = world.stats();
            uiFrame.mode = "gameover";
            uiFrame.title = world.result();
            uiFrame.subtitle = core::Loc::instance().t("gameover.finished");
            uiFrame.lines = {
                "map " + world.mapName() + "   seed " + std::to_string(simSeed),
                core::Loc::instance().t("gameover.time") + " " +
                    std::to_string(static_cast<int>(st.matchTime)) + "s   " +
                    core::Loc::instance().t("gameover.escaped") + " " + std::to_string(st.survivorsEscaped) +
                    "   " + core::Loc::instance().t("gameover.eliminated") + " " +
                    std::to_string(st.survivorsEliminated),
                core::Loc::instance().t("gameover.gens") + " " + std::to_string(st.generatorsActivated) + "/" +
                    std::to_string(static_cast<int>(world.generators().size())) + "   " +
                    core::Loc::instance().t("gameover.rescues") + " " + std::to_string(st.rescues) + "   " +
                    core::Loc::instance().t("gameover.items") + " " + std::to_string(st.itemsUsed) + "   " +
                    core::Loc::instance().t("gameover.charms") + " " + std::to_string(st.charmsUsed) + "   downs " +
                    std::to_string(st.downs),
                core::Loc::instance().t("gameover.damage") + " " + std::to_string(st.damageDealt) + "   " +
                    core::Loc::instance().t("gameover.topkiller") + ": " + st.topKiller,
                core::Loc::instance().t("gameover.restart"),
                core::Loc::instance().t("gameover.quit"),
            };
        }

        {
            core::Profiler::Scope pScope("loop.present");
            renderer.present(world, status, uiFrame);
        }
        if (renderer.shouldClose()) {
            quit = true;
            break;
        }

        if (profEnabled) {
            profAccum += raw;
            if (profAccum >= profInterval) {
                profAccum = 0.0;
                core::Logger::info("Profiler:\n" + core::Profiler::instance().report());
                core::Profiler::instance().reset();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    audio.shutdown();
    profile.save(core::Config::instance().get("profile.file", "profile.dat"));
    core::Logger::info("Game loop finished");
}

void Game::logMatch() {
    const auto& sList = world.survivors();
    int alive = 0;
    std::string hpInfo;
    for (size_t i = 0; i < sList.size(); ++i) {
        if (!sList[i].eliminated && !sList[i].escaped) ++alive;
        if (i > 0) hpInfo += ", ";
        hpInfo += std::to_string(sList[i].health.hp()) + "(" + sList[i].health.band() + ")";
    }
    const auto& gList = world.generators();
    int done = 0;
    for (const auto& g : gList) {
        if (g.activated) ++done;
    }
    std::string extra;
    if (world.doorsOpen()) extra += " [DOORS OPEN]";
    std::string kinfo;
    int maxAnger = 0;
    for (const auto& k : world.killers()) {
        if (!kinfo.empty()) kinfo += ", ";
        kinfo += k.typeName() + ":" + k.stateName() + "(" + std::to_string(k.anger()) + ")";
        if (k.anger() > maxAnger) maxAnger = k.anger();
    }
    if (kinfo.empty()) kinfo = "none";
    core::Logger::info("t=" + std::to_string(static_cast<int>(world.clock())) + "s alive=" +
                       std::to_string(alive) + " gens=" + std::to_string(done) + "/" +
                       std::to_string(static_cast<int>(gList.size())) + " killers[" + kinfo +
                       "] angerMax=" + std::to_string(maxAnger) +
                       " hp[" + hpInfo + "]" + extra);
#ifndef SLASHCO_RAYLIB
    renderMap();
#endif
}

void Game::renderMap() {
    static constexpr int kSize = 23;
    static constexpr int kOffset = 11;
    char grid[kSize][kSize];
    for (int z = 0; z < kSize; ++z) {
        for (int x = 0; x < kSize; ++x) grid[z][x] = '.';
    }

    auto toIdx = [](double v) {
        int i = static_cast<int>(std::round(v + kOffset));
        if (i < 0) return 0;
        if (i >= kSize) return kSize - 1;
        return i;
    };

    for (const auto& e : world.exits()) {
        int x = toIdx(e.position.x);
        int z = toIdx(e.position.z);
        grid[z][x] = e.open ? 'E' : 'X';
    }
    for (const auto& g : world.generators()) {
        int x = toIdx(g.position.x);
        int z = toIdx(g.position.z);
        grid[z][x] = 'G';
    }
    for (const auto& s : world.survivors()) {
        if (s.eliminated || s.escaped) continue;
        int x = toIdx(s.position.x);
        int z = toIdx(s.position.z);
        char c = (s.id < 10) ? static_cast<char>('0' + s.id)
                             : static_cast<char>('A' + (s.id - 10));
        grid[z][x] = c;
    }
    if (!world.over()) {
        for (const auto& k : world.killers()) {
            int x = toIdx(k.position().x);
            int z = toIdx(k.position().z);
            grid[z][x] = 'K';
        }
    }

    core::Logger::info("map: X=locked door E=open door G=generator K=killer digits=survivors");
    for (int z = 0; z < kSize; ++z) {
        std::string line;
        line.reserve(kSize);
        for (int x = 0; x < kSize; ++x) line += grid[z][x];
        core::Logger::info(line);
    }
}

void Game::applySetting(int dir) {
    static const int widths[3] = {1280, 1600, 1920};
    static const int heights[3] = {720, 900, 1080};

    if (settingsSelected == 0) {
        resolutionIndex = std::max(0, std::min(2, resolutionIndex + dir));
        renderer.resize(widths[resolutionIndex], heights[resolutionIndex]);
    } else if (settingsSelected == 1) {
        volumePercent = std::max(0, std::min(100, volumePercent + dir * 10));
        audio.setVolume(static_cast<float>(volumePercent) / 100.0f);
    } else if (settingsSelected == 2) {
        if (dir != 0) {
            vhsOn = !vhsOn;
            renderer.setVhs(vhsOn);
        }
    } else if (settingsSelected == 3) {
        if (dir != 0) {
            gridOn = !gridOn;
            renderer.setGrid(gridOn);
        }
    } else if (settingsSelected == 5) {
        if (dir != 0) {
            std::string next = core::Loc::instance().locale() == "zh" ? "en" : "zh";
            if (next == "zh" && !renderer.hasUnicodeFont()) {
                core::Logger::warn("CJK font unavailable, keeping English UI");
                next = "en";
            }
            core::Loc::instance().setLocale(next);
            core::Config::instance().set("locale", next);
        }
    } else if (settingsSelected == 6) {
        saveSettings();
        states.change("Menu");
    }
}

void Game::saveSettings() {
    static const int widths[3] = {1280, 1600, 1920};
    static const int heights[3] = {720, 900, 1080};
    auto& cfg = core::Config::instance();
    cfg.set("window.width", std::to_string(widths[resolutionIndex]));
    cfg.set("window.height", std::to_string(heights[resolutionIndex]));
    char volBuf[16];
    std::snprintf(volBuf, sizeof(volBuf), "%.2f", volumePercent / 100.0f);
    cfg.set("audio.volume", volBuf);
    cfg.set("render.vhs", std::string(vhsOn ? "1" : "0"));
    cfg.set("render.grid", std::string(gridOn ? "1" : "0"));
    cfg.set("locale", core::Loc::instance().locale());
    if (cfg.save("config.ini")) {
        core::Logger::info("Settings saved to config.ini");
    }
}

void Game::trackAchievement(const std::string& event) {
    auto grant = [this](const char* id) {
        platform.unlockAchievement(id);
        profile.unlock(id);
    };

    if (event.find("escaped!") != std::string::npos) {
        grant("ACH_FIRST_ESCAPE");
        if (world.modeName() == "Blackout") {
            grant("ACH_BLACKOUT_ESCAPE");
        }
    } else if (event.find("was rescued") != std::string::npos) {
        grant("ACH_TEAM_PLAYER");
    } else if (event.find("GoldCharm revived") != std::string::npos) {
        grant("ACH_GOLDEN_SECOND_CHANCE");
    } else if (event.find("looted the vault") != std::string::npos) {
        grant("ACH_VAULT_LOOTER");
    } else if (event.find("looted a supply cache") != std::string::npos) {
        grant("ACH_SUPPLY_CACHE");
    } else if (event.find("Trap triggered") != std::string::npos) {
        trapTriggered = true;
    } else if (event.find("Butcher has awakened") != std::string::npos) {
        grant("ACH_VAULT_RAIDER");
    } else if (event.find("Herd split") != std::string::npos) {
        grant("ACH_SWARM_SURVIVOR");
    }
}

void Game::adjustBalance(int dir) {
    auto& b = world.balance();
    switch (balanceSelected) {
        case 0: b.moveSpeed = clampd(b.moveSpeed + dir * 0.25, 1.0, 8.0); break;
        case 1: b.sprintSpeed = clampd(b.sprintSpeed + dir * 0.25, 2.0, 12.0); break;
        case 2: b.staminaDrain = clampd(b.staminaDrain + dir * 2.0, 0.0, 40.0); break;
        case 3: b.staminaRegen = clampd(b.staminaRegen + dir * 1.0, 0.0, 30.0); break;
        case 4: b.repairTime = clampd(b.repairTime + dir * 0.5, 1.0, 20.0); break;
        case 5: b.downWindow = clampd(b.downWindow + dir * 5.0, 5.0, 120.0); break;
        case 6: b.damageScale = clampd(b.damageScale + dir * 0.1, 0.1, 5.0); break;
        case 7: b.angerGainScale = clampd(b.angerGainScale + dir * 0.1, 0.1, 5.0); break;
        case 8: b.generatorFuel = std::max(1, std::min(12, b.generatorFuel + dir)); break;
        default: break;
    }
    world.syncBalance();
    audio.playEvent("ui");
}

void Game::resetBalanceRow(int row) {
    game::Balance def;
    auto& b = world.balance();
    switch (row) {
        case 0: b.moveSpeed = def.moveSpeed; break;
        case 1: b.sprintSpeed = def.sprintSpeed; break;
        case 2: b.staminaDrain = def.staminaDrain; break;
        case 3: b.staminaRegen = def.staminaRegen; break;
        case 4: b.repairTime = def.repairTime; break;
        case 5: b.downWindow = def.downWindow; break;
        case 6: b.damageScale = def.damageScale; break;
        case 7: b.angerGainScale = def.angerGainScale; break;
        case 8: b.generatorFuel = def.generatorFuel; break;
        default: break;
    }
    world.syncBalance();
    audio.playEvent("ui");
}

void Game::saveBalance() {
    auto& cfg = core::Config::instance();
    const auto& b = world.balance();
    char buf[32];
    auto putD = [&](const char* key, double v) {
        std::snprintf(buf, sizeof(buf), "%.3f", v);
        cfg.set(key, buf);
    };
    putD("balance.moveSpeed", b.moveSpeed);
    putD("balance.sprintSpeed", b.sprintSpeed);
    putD("balance.staminaDrain", b.staminaDrain);
    putD("balance.staminaRegen", b.staminaRegen);
    putD("balance.repairTime", b.repairTime);
    putD("balance.downWindow", b.downWindow);
    putD("balance.damageScale", b.damageScale);
    putD("balance.angerGainScale", b.angerGainScale);
    cfg.set("balance.generatorFuel", std::to_string(b.generatorFuel));
    if (cfg.save("config.ini")) {
        core::Logger::info("Balance saved to config.ini");
    }
}

void Game::runBenchmark() {
    core::Logger::info("Benchmark mode: " + std::to_string(benchMatches) + " matches");
    int survivorWins = 0;
    int killerWins = 0;
    int totalEscaped = 0;
    int totalEliminated = 0;
    double totalSimTime = 0.0;
    double totalWallMs = 0.0;
    std::map<std::string, int> killsByKiller;

    std::string rotation = core::Config::instance().get("map.rotation", "");
    std::vector<std::string> maps;
    if (rotation.empty()) {
        maps.push_back(core::Config::instance().get("map.name", "Factory"));
    } else {
        std::string token;
        for (char ch : rotation + ",") {
            if (ch == ' ') continue;
            if (ch == ',') {
                if (!token.empty()) {
                    maps.push_back(token);
                    token.clear();
                }
            } else {
                token += ch;
            }
        }
        if (maps.empty()) maps.push_back("Factory");
    }

    std::string kts = core::Config::instance().get("killer.types", "Stalker");
    std::vector<game::KillerAI::Kind> kinds = parseKillerKinds(kts);
    std::string benchModeName = core::Config::instance().get("mode.name", "Classic");
    const double stepDt = 1.0 / 60.0;

    for (int i = 0; i < benchMatches; ++i) {
        unsigned long long seed = simSeed + static_cast<unsigned long long>(i);
        core::Random::instance().seed(seed);
        std::string map = maps[static_cast<size_t>(i) % maps.size()];
        world.configure(players, kinds, map, benchModeName);
        world.syncBalance();

        std::vector<game::ReplayFrame> rec;
        bool recordThis = (i == 0 && benchSelfTest);

        auto wallStart = std::chrono::steady_clock::now();
        double simTime = 0.0;
        game::Control idle;
        while (!world.over() && simTime < 300.0) {
            world.update(stepDt, idle);
            simTime += stepDt;
            if (recordThis) {
                game::ReplayFrame rf;
                rf.dt = static_cast<float>(stepDt);
                rec.push_back(rf);
            }
        }
        auto wallEnd = std::chrono::steady_clock::now();
        double wallMs = std::chrono::duration<double, std::milli>(wallEnd - wallStart).count();

        const game::MatchStats& st = world.stats();
        std::string mResult = world.result();
        int mEscaped = st.survivorsEscaped;
        int mEliminated = st.survivorsEliminated;
        for (const auto& kv : st.killsByKiller) {
            killsByKiller[kv.first] += kv.second;
        }

        if (i == 0 && benchSelfTest) {
            core::Random::instance().seed(seed);
            world.configure(players, kinds, map, benchModeName);
            world.syncBalance();
            game::Control idle2;
            for (const auto& fr : rec) {
                world.update(fr.dt, idle2);
            }
            const game::MatchStats& st2 = world.stats();
            bool pass = (world.result() == mResult) && (st2.survivorsEscaped == mEscaped) &&
                        (st2.survivorsEliminated == mEliminated);
            core::Logger::info(std::string("Selftest: ") + (pass ? "PASS" : "FAIL") + " rerun (" +
                               mResult + " -> " + world.result() + ")");
        }

        if (mResult == "SurvivorsWin") {
            ++survivorWins;
        } else {
            ++killerWins;
        }
        totalEscaped += mEscaped;
        totalEliminated += mEliminated;
        totalSimTime += simTime;
        totalWallMs += wallMs;

        core::Logger::info("bench#" + std::to_string(i) + " map=" + map + " seed=" +
                           std::to_string(seed) + " result=" + mResult + " t=" +
                           std::to_string(static_cast<int>(simTime)) + "s wall=" +
                           std::to_string(static_cast<int>(wallMs)) + "ms");
    }

    core::Logger::info("Benchmark summary: survivorWins=" + std::to_string(survivorWins) +
                       " killerWins=" + std::to_string(killerWins) + " avgSimTime=" +
                       std::to_string(totalSimTime / benchMatches) + "s avgWall=" +
                       std::to_string(totalWallMs / benchMatches) + "ms");
    core::Logger::info("Benchmark totals: escaped=" + std::to_string(totalEscaped) +
                       " eliminated=" + std::to_string(totalEliminated));
    std::string killLine;
    for (const auto& kv : killsByKiller) {
        if (!killLine.empty()) killLine += ", ";
        killLine += kv.first + "=" + std::to_string(kv.second);
    }
    core::Logger::info("Benchmark kills: " + (killLine.empty() ? std::string("none") : killLine));
}

void Game::finishReplayRecording() {
    std::filesystem::create_directories("replays");
    std::string path = core::Config::instance().get("replay.file", "replays/last.rep");
    auto slash = path.find_last_of("/\\");
    if (slash != std::string::npos) {
        std::filesystem::create_directories(path.substr(0, slash));
    }
    if (replayRec.save(path)) {
        core::Logger::info("Replay saved: " + path + " (" +
                           std::to_string(replayRec.frames.size()) + " frames)");
    } else {
        core::Logger::warn("Replay save failed: " + path);
    }
}

void Game::handleEventFx(const std::string& event) {
    int id = parseSurvivorId(event);
    bool local = false;
    if (const game::Survivor* lp = world.localSurvivor()) {
        local = (id >= 0 && id == lp->id);
    }

    auto fxAt = [&](int sid, const std::string& text, unsigned char r, unsigned char g,
                    unsigned char b) {
        for (const auto& s : world.survivors()) {
            if (s.id == sid) {
                renderer.addHitFx(s.position.x, s.position.z, text, r, g, b);
                return;
            }
        }
    };

    if (event.find("hit by") != std::string::npos) {
        std::string dmgText = "HIT";
        auto p = event.find("(-");
        if (p != std::string::npos) {
            auto q = event.find("hp", p);
            if (q != std::string::npos) dmgText = "-" + event.substr(p + 2, q - p - 2);
        }
        if (id >= 0) fxAt(id, dmgText, 255, 80, 80);
        renderer.addShake(local ? 1.0f : 0.55f);
    } else if (event.find("went down") != std::string::npos) {
        if (id >= 0) fxAt(id, "DOWN", 255, 200, 60);
        renderer.addShake(local ? 1.1f : 0.7f);
        slowTimer = std::max(slowTimer, 0.5);
    } else if (event.find("eliminated") != std::string::npos) {
        if (id >= 0) fxAt(id, "ELIMINATED", 255, 60, 60);
        renderer.addShake(1.4f);
        slowTimer = std::max(slowTimer, 0.9);
    } else if (event.find("escaped!") != std::string::npos) {
        if (id >= 0) fxAt(id, "ESCAPED", 120, 255, 160);
        renderer.addShake(0.3f);
        slowTimer = std::max(slowTimer, 0.4);
    } else if (event.find("Butcher has awakened") != std::string::npos) {
        renderer.addShake(1.6f);
        slowTimer = std::max(slowTimer, 0.6);
    } else if (event.find("looted the vault") != std::string::npos) {
        if (id >= 0) fxAt(id, "+GoldCharm", 255, 220, 90);
        renderer.addShake(0.4f);
    } else if (event.find("activated!") != std::string::npos) {
        renderer.addShake(0.25f);
    }
}
