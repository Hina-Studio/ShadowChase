#include "DebugPanel.hpp"

#ifdef SLASHCO_IMGUI
#include "imgui.h"
#endif

namespace client_debug {
void drawPanel(const ClientDebugState& state, bool& open) {
#ifdef SLASHCO_IMGUI
    if (!open) return;
    ImGui::SetNextWindowSize(ImVec2(380, 420), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("SlashCo Debug [F1]", &open)) {
        ImGui::Text("FPS %.1f   frame %.2f ms", state.fps, state.frameMs);
        ImGui::Separator();
        ImGui::Text("Net: %s   rtt %d ms", state.connected ? "connected" : "offline", state.rttMs);
        ImGui::Text("server tick %u   recv %d pkts / %.1f KB", state.serverTick, state.recvPackets,
                    static_cast<double>(state.recvBytes) / 1024.0);
        ImGui::Text("player id %d   remotes %d", state.playerId, state.remoteCount);
        ImGui::Separator();
        ImGui::Text("Map seed %u  size %d  blocks %d", state.seed, state.mapSize, state.blockCount);
        ImGui::Text("objects %d   hp %.0f   carrying %d   gamepad %s", state.objectCount, state.hp,
                    state.carrying, state.padActive ? "on" : "off");
        ImGui::Text("own pos %.2f, %.2f", state.ownX, state.ownZ);
        ImGui::Separator();
        ImGui::Text("Movement is server-authoritative (M1: speed validated server side).");
        ImGui::Text("Pickups: +40 hp / +12 ammo   Crates: carry slows 20%%   Doors: E toggles");
    }
    ImGui::End();
#else
    (void)state;
    (void)open;
#endif
}
}
