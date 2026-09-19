#include "core/Input.hpp"

#ifdef _WIN32
#include <windows.h>

static bool keyDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

void Input::poll() {
    InputState s;
    s.moveUp = keyDown('W') || keyDown(VK_UP);
    s.moveDown = keyDown('S') || keyDown(VK_DOWN);
    s.moveLeft = keyDown('A') || keyDown(VK_LEFT);
    s.moveRight = keyDown('D') || keyDown(VK_RIGHT);
    s.interact = keyDown('E');
    s.sprint = keyDown(VK_SHIFT);
    s.quit = keyDown(VK_ESCAPE);

    bool interactEdge = s.interact && !prevInteract;
    bool quitEdge = s.quit && !prevQuit;
    prevInteract = s.interact;
    prevQuit = s.quit;

    s.interactPressed = interactEdge;
    s.quit = quitEdge;

    bool d1 = keyDown('1');
    bool d2 = keyDown('2');
    bool d3 = keyDown('3');
    s.slot1 = d1 && !prevSlot1;
    s.slot2 = d2 && !prevSlot2;
    s.slot3 = d3 && !prevSlot3;
    prevSlot1 = d1;
    prevSlot2 = d2;
    prevSlot3 = d3;

    bool bKey = keyDown('B');
    bool gKey = keyDown('G');
    s.buyCharm = bKey && !prevB;
    s.buyGold = gKey && !prevG;
    prevB = bKey;
    prevG = gKey;

    bool enterKey = keyDown(VK_RETURN) || keyDown(' ') || keyDown(VK_SPACE);
    bool rKey = keyDown('R');
    s.confirm = enterKey && !prevConfirm;
    s.restart = rKey && !prevRestart;
    prevConfirm = enterKey;
    prevRestart = rKey;

    bool upK = keyDown('W') || keyDown(VK_UP);
    bool downK = keyDown('S') || keyDown(VK_DOWN);
    bool leftK = keyDown('A') || keyDown(VK_LEFT);
    bool rightK = keyDown('D') || keyDown(VK_RIGHT);
    bool oK = keyDown('O');
    s.navUp = upK && !prevUp;
    s.navDown = downK && !prevDown;
    s.navLeft = leftK && !prevLeft;
    s.navRight = rightK && !prevRight;
    s.settingsOpen = oK && !prevO;
    prevUp = upK;
    prevDown = downK;
    prevLeft = leftK;
    prevRight = rightK;
    prevO = oK;

    bool pK = keyDown('P');
    s.panelToggle = pK && !prevP;
    prevP = pK;

    bool tK = keyDown('T');
    s.tutorialOpen = tK && !prevT;
    prevT = tK;

    bool lK = keyDown('L');
    s.replayOpen = lK && !prevL;
    prevL = lK;

    bool brL = keyDown(VK_OEM_4);
    bool brR = keyDown(VK_OEM_6);
    s.spectatePrev = brL && !prevBracketL;
    s.spectateNext = brR && !prevBracketR;
    prevBracketL = brL;
    prevBracketR = brR;

    bool jK = keyDown('J');
    s.lobbyOpen = jK && !prevJ;
    prevJ = jK;

    st = s;
}
#else
void Input::poll() {
    InputState s;
    st = s;
}
#endif
