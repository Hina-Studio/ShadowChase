openxr / quest milestone
========================

SLASHCO_ENABLE_OPENXR: FetchContent(OpenXR-SDK-Source, release-1.0.34) builds the
Khronos loader statically and links it into both desktop and android client targets.

phase 1 (shipped): runtime detection
    --no-vr forces the flat renderer; otherwise the game tries a real session.
    Fallback is automatic: if the runtime can't give an HMD system
    (XR_ERROR_FORM_FACTOR_UNAVAILABLE), the UI stays in the normal flat renderer.

phase 2 (shipped, desktop GL): session renderer
    src\client\vr\OpenXRBackend.cpp implements the full pipeline behind a small API:
    initSession / beginFrame / endFrame / bindEye / unbindEye / getEyePose / input.

    instance        : XR_KHR_opengl_enable
    system          : HMD display form factor (flat fallback otherwise)
    session         : XrGraphicsBindingOpenGLWin32KHR with wglGetCurrentDC/Context
    reference space : XR_REFERENCE_SPACE_TYPE_LOCAL
    swapchains      : one per eye, 2048x2048, GL_RGBA8 or first supported format
    framebuffers    : per swapchain image - rlLoadFramebuffer + attached external
                      color texture + rlLoadTextureDepth, validated with
                      rlFramebufferComplete (see src\client\vr\rlGlue.cpp, a
                      raylib-only TU so the OpenXR TU never pulls wingdi/GDI
                      names into the same translation unit as raylib)
    frame loop      : pollEvents (auto-begins session on SESSION_STATE_READY),
                      xrWaitFrame -> xrBeginFrame -> syncActions -> xrLocateViews,
                      then per eye: acquire/wait image -> bindEye -> drawScene ->
                      unbindEye, then endFrame submits an
                      XrCompositionLayerProjection with both views
    input           : action set "slashco" - left/right thumbstick (vector2f) +
                      trigger/squeeze (bool); suggested bindings for oculus touch,
                      ms motion and valve index profiles; merged into ClientApp
                      sendInput/updateLocal (left thumb = move, right thumb = look,
                      trigger = interact, squeeze = drop)
    monitor view    : while the session renders the eyes, the desktop window keeps
                      mirroring the flat camera + drawHud2D for spectators

    verified on windows (no headset present):
        [VR] no HMD available: XR_ERROR_FORM_FACTOR_UNAVAILABLE -> flat mode
        flat renderer, embedded server, selftests and e2e join unchanged.

phase 3 (shipped, code): quest native session (needs on-device test)
    The android TU shares the same session implementation (one file, platform
    branches): XR_KHR_opengles_enable + XrGraphicsBindingOpenGLESAndroidKHR with
    EGL display/context pulled from the live raylib context (eglGetCurrentDisplay/
    Context, config resolved via EGL_CONFIG_ID), XR_KHR_loader_init_android
    binding the Java VM (GetAndroidApp) before xrCreateInstance, swapchain sizes
    from xrEnumerateViewConfigurationViews, and depth renderbuffer attachments
    (rlgl falls back automatically on GLES). While a session renders the eyes,
    the hidden android window surface is not drawn (mirror block skipped on
    __ANDROID__).

    verified on windows (no headset present):
        flat renderer, embedded server, selftests and e2e join unchanged.
    verified on android arm64-v8a (build only so far):
        cmake+Ninja compile OK, apk re-signed (apksigner verify OK), manifest
        contains com.samsung.android.vr.application.mode=vr_only and
        com.oculus.intent.category.VR so Quest's launcher treats the app as
        immersive.

phase 4 (pending): quest device test + pcvr
    Side-load the apk via adb, grant runtime perms, launch: session should spawn
    into OpenXR (com.oculus.* loader) and the eye render path executes in-device.
    PCVR/Link runs the same desktop GL session code on windows - just need a
    headset once available.
