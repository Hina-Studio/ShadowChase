#pragma once

// OpenXR renderer backend. One instance/session, per-eye swapchain framebuffers
// bound to raylib's GL context so ClientApp can render each eye with rlgl
// primitives. Falls back to the flat desktop render when no runtime or HMD
// exists. Desktop GL pipeline today; Quest ES/Vulkan pipeline is a follow-up.

#include <cstdint>

namespace slashco_vr {

struct EyePose {
    float px, py, pz;                       // eye/head position (meters, LOCAL space)
    float qx, qy, qz, qw;                   // orientation
    float fovUp = 0.0f, fovDown = 0.0f;     // vertical fov extents in radians
    float fovLeft = 0.0f, fovRight = 0.0f;  // horizontal fov extents in radians
};

struct VrInput {
    float moveX = 0.0f;   // left thumb X (-1..1)
    float moveY = 0.0f;   // left thumb Y (forward = +1)
    float lookX = 0.0f;   // right thumb X
    float lookY = 0.0f;   // right thumb Y
    bool triggerPressed = false;
    bool squeezePressed = false;
    bool hmdValid = false;
};

enum class VrMode { Flat, Ready };

// Attempt a complete OpenXR setup (instance → system → session → swapchains
// → actions → action spaces). Must be called AFTER the GL context is live
// (raylib InitWindow). Returns Ready on success; otherwise Flat (the caller
// keeps the normal 2D desktop path).
VrMode initSession();

bool active();
bool beginFrame();
void endFrame();
bool bindEye(int eye);     // Binds eye framebuffer for rendering (returns false to skip eye)
void unbindEye(int eye);   // Restores default framebuffer + viewport
void getEyePose(int eye, EyePose* out);
VrInput input();
const char* lastError();

}  // namespace slashco_vr
