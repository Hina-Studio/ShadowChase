#pragma once
#include "core/Renderer.hpp"
#include "core/Input.hpp"
#include "core/Network.hpp"
#include "core/Logger.hpp"
#include "core/Config.hpp"
#include "core/EventManager.hpp"
#include "core/AssetManager.hpp"
#include "core/Timer.hpp"
#include "core/Random.hpp"
#include "core/Audio.hpp"
#include "core/Connectivity.hpp"
#include "core/Platform.hpp"
#include "core/Profile.hpp"
#include "core/LanDiscovery.hpp"
#include "core/Profiler.hpp"
#include "game/World.hpp"
#include "game/StateMachine.hpp"
#include "game/Replay.hpp"

class Game {
public:
    Game();
    void init();
    void run();
private:
    Renderer renderer;
    Input input;
    Network network;
    Audio audio;
    Connectivity connectivity;
    Platform platform;
    Profile profile;
    LanDiscovery lan;
    int lobbySelected = 0;
    bool profEnabled = false;
    double profInterval = 2.0;
    double profAccum = 0.0;
    bool trapTriggered = false;
    bool mouseCapturedState = false;

    game::World world;
    game::StateMachine states;
    game::Control localCtrl;
    UiFrame uiFrame;

    int players = 4;
    double menuElapsed = 0.0;
    double overElapsed = 0.0;
    int lastLoggedSecond = -1;
    bool quit = false;
    bool clientMode = false;
    double netSendAccum = 0.0;
    double netInputAccum = 0.0;
    int clientId = 1;
    std::string lastSnapshot;
    unsigned long long simSeed = 0;
    int matchCounter = 0;
    int sessionMatches = 0;
    int sessionEscapes = 0;
    int sessionEliminated = 0;

    int settingsSelected = 0;
    int resolutionIndex = 0;
    int volumePercent = 60;
    bool vhsOn = true;
    bool gridOn = true;
    double slowTimer = 0.0;
    double timeScale = 1.0;
    bool balancePanel = false;
    int balanceSelected = 0;
    int tutorialStep = 0;
    double tutorialTimer = 0.0;
    bool tutorialComplete = false;
    int benchMatches = 0;
    bool benchMode = false;
    bool benchSelfTest = false;
    bool spectate = false;
    int spectateId = -1;
    size_t spectateIndex = 0;
    game::ReplayData replayRec;
    game::ReplayData replayPlay;
    bool replayRecording = false;
    size_t replayIndex = 0;
    size_t remoteIndex = 0;
    bool replayFinished = false;

    void logMatch();
    void renderMap();
    void applySetting(int dir);
    void saveSettings();
    void trackAchievement(const std::string& event);
    void handleEventFx(const std::string& event);
    void adjustBalance(int dir);
    void resetBalanceRow(int row);
    void saveBalance();
    void finishReplayRecording();
    void runBenchmark();
};
