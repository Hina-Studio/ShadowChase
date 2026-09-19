#pragma once

struct InputState {
    bool moveUp = false;
    bool moveDown = false;
    bool moveLeft = false;
    bool moveRight = false;
    bool interact = false;
    bool interactPressed = false;
    bool sprint = false;
    bool quit = false;
    bool slot1 = false;
    bool slot2 = false;
    bool slot3 = false;
    bool buyCharm = false;
    bool buyGold = false;
    bool confirm = false;
    bool restart = false;
    bool navUp = false;
    bool navDown = false;
    bool navLeft = false;
    bool navRight = false;
    bool settingsOpen = false;
    bool panelToggle = false;
    bool tutorialOpen = false;
    bool replayOpen = false;
    bool spectatePrev = false;
    bool spectateNext = false;
    bool lobbyOpen = false;
};

class Input {
public:
    void poll();
    const InputState& state() const { return st; }

private:
    InputState st;
    bool prevInteract = false;
    bool prevQuit = false;
    bool prevSlot1 = false;
    bool prevSlot2 = false;
    bool prevSlot3 = false;
    bool prevB = false;
    bool prevG = false;
    bool prevConfirm = false;
    bool prevRestart = false;
    bool prevUp = false;
    bool prevDown = false;
    bool prevLeft = false;
    bool prevRight = false;
    bool prevO = false;
    bool prevP = false;
    bool prevT = false;
    bool prevL = false;
    bool prevBracketL = false;
    bool prevBracketR = false;
    bool prevJ = false;
};
