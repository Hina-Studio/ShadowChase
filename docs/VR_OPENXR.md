openxr / quest milestone
========================

SLASHCO_ENABLE_OPENXR: FetchContent(OpenXR-SDK-Source, release-1.0.34) builds the
Khronos loader statically and links it into both desktop and android client targets.
At start the game calls slashco_vr::probeRuntime() (src/client/vr/OpenXRBackend.cpp):

    Windows desktop, no headset  -> [VR] OpenXR xrCreateInstance failed (flat mode)
    Windows desktop w/ runtime   -> [VR] runtime=VirtualDesktopXR / OVR / OpenXR … flat mode
    Quest (same APK)             -> uses the system OpenXR loader; runtime auto-bound.

Fallback behaviour is automatic: if a runtime can't give us an HMD system
(XR_ERROR_…), the UI stays in the normal flat renderer.

VI packing/build is already done:

    packaging/android/build_apk.ps1 -OpenXR      # Android (arm64-v8a) with loader

The next VR milestone is the actual OpenXR renderer:

1) xrGetVulkanGraphicsRequirementsKHR / OpenGLES Android binding (Quest native),
2) swapchain creation (two-view FBO in raylib offscreen RenderTextures),
3) game-loop: xrWaitFrame/xrBeginFrame/xrLocateViews → render StereoCamera3D per eye
   with ClientApp's raylib renderer + controllers via xrSuggestInteractionProfileBindings,
4) Quest: need meta's extra Android runtime checks (com.oculus.* mania) + PCVR link.
Hands-on device testing still required.
