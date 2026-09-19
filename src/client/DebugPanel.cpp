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
        ImGui::Text("own pos %.2f, %.2f", state.ownX, state.ownZ);
        ImGui::Separator();
        ImGui::Text("Movement (server authoritative; sliders are local preview)");
        ImGui::SliderFloat("walk", const_cast<float*>(&state.walkSpeed), 1.0f, 10.0f);
        ImGui::SliderFloat("sprint", const_cast<float*>(&state.sprintSpeed), 2.0f, 14.0f);
    }
    ImGui::End();
#else
    (void)state;
    (void)open;
#endif
}
}
