#include "OpenXRBackend.hpp"

#if defined(SLASHCO_OPENXR) && defined(_WIN32)

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <unknwn.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "rlGlue.hpp"
#include <core/Logger.hpp>

namespace slashco_vr {

namespace {

constexpr uint64_t GL_RGBA8_FMT = 0x8058ull;
constexpr int RL_ATTACH_COLOR0 = 0;
constexpr int RL_ATTACH_DEPTH = 100;
constexpr int RL_TEX_TYPE_2D = 100;

struct SwapImage {
    unsigned int color = 0;
    unsigned int depth = 0;
    unsigned int fbo = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct Engine {
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;
    XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
    XrSpace localSpace = XR_NULL_HANDLE;
    XrSwapchain swapchain[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
    std::vector<SwapImage> images[2];
    uint32_t width[2] = {0, 0};
    uint32_t height[2] = {0, 0};
    uint32_t acquired[2] = {0, 0};
    bool acquiredValid[2] = {false, false};
    bool began = false;
    bool frameActive = false;
    XrTime displayTime = 0;
    XrPosef pose[2]{};
    XrFovf fov[2]{};
    std::string lastError = "not initialized";

    Engine() {
        for (int i = 0; i < 2; ++i) {
            pose[i].orientation.w = 1.0f;
            fov[i].angleUp = 0.9f;
            fov[i].angleDown = -0.9f;
            fov[i].angleLeft = -0.9f;
            fov[i].angleRight = 0.9f;
        }
    }
};

Engine g_engine;
VrInput g_input;
bool g_activeFlag = false;
bool g_submitWarned = false;

struct ActionRefs {
    XrActionSet set = XR_NULL_HANDLE;
    XrAction thumbLeft = XR_NULL_HANDLE;
    XrAction thumbRight = XR_NULL_HANDLE;
    XrAction trigger = XR_NULL_HANDLE;
    XrAction squeeze = XR_NULL_HANDLE;
    XrPath leftHand = XR_NULL_PATH;
    XrPath rightHand = XR_NULL_PATH;
};

ActionRefs g_actions;

void setErr(std::string what, XrResult r = XR_SUCCESS) {
    if (r != XR_SUCCESS && g_engine.instance != XR_NULL_HANDLE) {
        char buf[XR_MAX_RESULT_STRING_SIZE] = {};
        xrResultToString(g_engine.instance, r, buf);
        what += ": ";
        what += buf;
    }
    g_engine.lastError = std::move(what);
    core::Logger::warn("[VR] " + g_engine.lastError);
}

void destroyAll() {
    for (int eye = 0; eye < 2; ++eye) {
        for (auto& im : g_engine.images[eye]) {
            if (im.fbo) glueUnloadFramebuffer(im.fbo);
        }
        g_engine.images[eye].clear();
        if (g_engine.swapchain[eye] != XR_NULL_HANDLE)
            xrDestroySwapchain(g_engine.swapchain[eye]);
        g_engine.swapchain[eye] = XR_NULL_HANDLE;
    }
    if (g_actions.set != XR_NULL_HANDLE) {
        xrDestroyActionSet(g_actions.set);
        g_actions = ActionRefs{};
    }
    if (g_engine.localSpace != XR_NULL_HANDLE)
        xrDestroySpace(g_engine.localSpace);
    g_engine.localSpace = XR_NULL_HANDLE;
    if (g_engine.session != XR_NULL_HANDLE)
        xrDestroySession(g_engine.session);
    g_engine.session = XR_NULL_HANDLE;
    if (g_engine.instance != XR_NULL_HANDLE)
        xrDestroyInstance(g_engine.instance);
    g_engine.instance = XR_NULL_HANDLE;
    g_engine.began = false;
    g_engine.frameActive = false;
}

bool createInstance() {
    uint32_t count = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr);
    std::vector<XrExtensionProperties> props(count > 0 ? count : 1);
    for (auto& p : props) p.type = XR_TYPE_EXTENSION_PROPERTIES;
    if (count > 0 &&
        xrEnumerateInstanceExtensionProperties(nullptr, count, &count, props.data()) != XR_SUCCESS) {
        setErr("extension enumeration failed");
        return false;
    }
    bool hasGl = false;
    for (uint32_t i = 0; i < count; ++i) {
        if (std::strcmp(props[i].extensionName, "XR_KHR_opengl_enable") == 0) hasGl = true;
    }
    if (!hasGl) {
        setErr("runtime lacks XR_KHR_opengl_enable");
        return false;
    }

    XrInstanceCreateInfo info{};
    info.type = XR_TYPE_INSTANCE_CREATE_INFO;
    std::snprintf(info.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE, "SlashCo");
    std::snprintf(info.applicationInfo.engineName, XR_MAX_ENGINE_NAME_SIZE, "SlashCo");
    info.applicationInfo.applicationVersion = 1;
    info.applicationInfo.engineVersion = 1;
    info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    const char* enabled[1] = {"XR_KHR_opengl_enable"};
    info.enabledExtensionNames = enabled;
    info.enabledExtensionCount = 1;
    XrResult r = xrCreateInstance(&info, &g_engine.instance);
    if (r != XR_SUCCESS) {
        setErr("no OpenXR loader found", r);
        return false;
    }
    return true;
}

bool createSystemAndSession() {
    XrSystemGetInfo sysInfo{};
    sysInfo.type = XR_TYPE_SYSTEM_GET_INFO;
    sysInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrResult r = xrGetSystem(g_engine.instance, &sysInfo, &g_engine.system);
    if (r != XR_SUCCESS) {
        setErr("no HMD available", r);
        return false;
    }
    XrSystemProperties sysProps{};
    sysProps.type = XR_TYPE_SYSTEM_PROPERTIES;
    xrGetSystemProperties(g_engine.instance, g_engine.system, &sysProps);
    core::Logger::info(std::string("[VR] HMD: ") + sysProps.systemName);

    PFN_xrGetOpenGLGraphicsRequirementsKHR glReq = nullptr;
    xrGetInstanceProcAddr(g_engine.instance, "xrGetOpenGLGraphicsRequirementsKHR",
                          reinterpret_cast<PFN_xrVoidFunction*>(&glReq));
    if (glReq) {
        XrGraphicsRequirementsOpenGLKHR req{};
        req.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
        glReq(g_engine.instance, g_engine.system, &req);
        core::Logger::info("[VR] GL min version ok");
    }

    XrGraphicsBindingOpenGLWin32KHR binding{};
    binding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
    binding.hDC = wglGetCurrentDC();
    binding.hGLRC = wglGetCurrentContext();
    if (!binding.hDC || !binding.hGLRC) {
        setErr("GL context unavailable");
        return false;
    }
    XrSessionCreateInfo createInfo{};
    createInfo.type = XR_TYPE_SESSION_CREATE_INFO;
    createInfo.next = &binding;
    createInfo.systemId = g_engine.system;
    r = xrCreateSession(g_engine.instance, &createInfo, &g_engine.session);
    if (r != XR_SUCCESS) {
        setErr("xrCreateSession failed", r);
        return false;
    }
    return true;
}

bool createSpaceAndSwapchains() {
    XrReferenceSpaceCreateInfo refInfo{};
    refInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
    refInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    refInfo.poseInReferenceSpace.orientation.w = 1.0f;
    XrResult r = xrCreateReferenceSpace(g_engine.session, &refInfo, &g_engine.localSpace);
    if (r != XR_SUCCESS) {
        setErr("xrCreateReferenceSpace failed", r);
        return false;
    }

    uint32_t fmtCount = 0;
    xrEnumerateSwapchainFormats(g_engine.session, 0, &fmtCount, nullptr);
    int64_t chosen = 0;
    std::vector<int64_t> formats(fmtCount > 0 ? fmtCount : 1, 0);
    if (fmtCount > 0 &&
        xrEnumerateSwapchainFormats(g_engine.session, fmtCount, &fmtCount, formats.data()) ==
            XR_SUCCESS) {
        for (uint32_t i = 0; i < fmtCount; ++i) {
            if (static_cast<uint64_t>(formats[i]) == GL_RGBA8_FMT) chosen = formats[i];
        }
        if (chosen == 0 && fmtCount > 0) chosen = formats[0];
    }
    if (chosen == 0) {
        setErr("no compatible swapchain format");
        return false;
    }

    for (int eye = 0; eye < 2; ++eye) {
        XrSwapchainCreateInfo swInfo{};
        swInfo.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
        swInfo.format = chosen;
        swInfo.width = 2048;
        swInfo.height = 2048;
        swInfo.arraySize = 1;
        swInfo.mipCount = 1;
        swInfo.sampleCount = 1;
        swInfo.faceCount = 1;
        swInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        r = xrCreateSwapchain(g_engine.session, &swInfo, &g_engine.swapchain[eye]);
        if (r != XR_SUCCESS) {
            setErr("xrCreateSwapchain failed", r);
            return false;
        }
        g_engine.width[eye] = swInfo.width;
        g_engine.height[eye] = swInfo.height;

        uint32_t imgCount = 0;
        xrEnumerateSwapchainImages(g_engine.swapchain[eye], 0, &imgCount, nullptr);
        std::vector<XrSwapchainImageOpenGLKHR> imgs(imgCount > 0 ? imgCount : 1);
        for (auto& im : imgs) im.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
        r = xrEnumerateSwapchainImages(g_engine.swapchain[eye], imgCount, &imgCount,
                                       reinterpret_cast<XrSwapchainImageBaseHeader*>(imgs.data()));
        if (r != XR_SUCCESS || imgCount == 0) {
            setErr("xrEnumerateSwapchainImages failed", r);
            return false;
        }
        for (uint32_t i = 0; i < imgCount; ++i) {
            SwapImage si;
            si.color = static_cast<unsigned int>(imgs[i].image);
            si.width = g_engine.width[eye];
            si.height = g_engine.height[eye];
            si.fbo = glueLoadFramebuffer();
            if (si.fbo == 0) continue;
            si.depth = glueLoadDepthTexture(static_cast<int>(si.width),
                                            static_cast<int>(si.height));
            glueBindFramebuffer(si.fbo);
            glueFramebufferAttach(si.fbo, si.color, RL_ATTACH_COLOR0, RL_TEX_TYPE_2D, 0);
            glueFramebufferAttach(si.fbo, si.depth, RL_ATTACH_DEPTH, RL_TEX_TYPE_2D, 0);
            bool ok = glueFramebufferComplete(si.fbo);
            glueBindFramebuffer(0);
            if (!ok) {
                glueUnloadFramebuffer(si.fbo);
                si.fbo = 0;
                continue;
            }
            g_engine.images[eye].push_back(si);
        }
        if (g_engine.images[eye].empty()) {
            setErr("eye framebuffer setup failed");
            return false;
        }
        core::Logger::info("[VR] eye " + std::to_string(eye) + ": " +
                           std::to_string(imgCount) + " images " +
                           std::to_string(g_engine.width[eye]) + "x" +
                           std::to_string(g_engine.height[eye]));
    }
    return true;
}

bool createActions() {
    XrPath hands[2] = {XR_NULL_PATH, XR_NULL_PATH};
    xrStringToPath(g_engine.instance, "/user/hand/left", &hands[0]);
    xrStringToPath(g_engine.instance, "/user/hand/right", &hands[1]);
    g_actions.leftHand = hands[0];
    g_actions.rightHand = hands[1];

    XrActionSetCreateInfo setInfo{};
    setInfo.type = XR_TYPE_ACTION_SET_CREATE_INFO;
    std::snprintf(setInfo.actionSetName, XR_MAX_ACTION_SET_NAME_SIZE, "slashco");
    std::snprintf(setInfo.localizedActionSetName, XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE,
                  "SlashCo");
    if (xrCreateActionSet(g_engine.instance, &setInfo, &g_actions.set) != XR_SUCCESS) {
        setErr("xrCreateActionSet failed");
        return false;
    }

    auto createAction = [&](XrActionType ty, const char* name, const char* local,
                            XrAction* out) {
        XrActionCreateInfo ai{};
        ai.type = XR_TYPE_ACTION_CREATE_INFO;
        ai.actionType = ty;
        std::snprintf(ai.actionName, XR_MAX_ACTION_NAME_SIZE, "%s", name);
        std::snprintf(ai.localizedActionName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE, "%s", local);
        ai.subactionPaths = hands;
        ai.countSubactionPaths = 2;
        if (xrCreateAction(g_actions.set, &ai, out) != XR_SUCCESS) *out = XR_NULL_HANDLE;
    };
    createAction(XR_ACTION_TYPE_VECTOR2F_INPUT, "thumbstick_left", "Left Thumbstick",
                 &g_actions.thumbLeft);
    createAction(XR_ACTION_TYPE_VECTOR2F_INPUT, "thumbstick_right", "Right Thumbstick",
                 &g_actions.thumbRight);
    createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "trigger", "Trigger", &g_actions.trigger);
    createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "squeeze", "Squeeze", &g_actions.squeeze);
    if (!g_actions.thumbLeft || !g_actions.thumbRight || !g_actions.trigger ||
        !g_actions.squeeze) {
        setErr("action creation failed");
        return false;
    }

    XrSessionActionSetsAttachInfo attach{};
    attach.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
    attach.countActionSets = 1;
    attach.actionSets = &g_actions.set;
    if (xrAttachSessionActionSets(g_engine.session, &attach) != XR_SUCCESS) {
        setErr("xrAttachSessionActionSets failed");
        return false;
    }

    using BindList = std::vector<std::pair<XrAction, const char*>>;
    struct Profile {
        const char* path;
        BindList binds;
    };
    std::vector<Profile> profiles;
    profiles.push_back({"/interaction_profiles/oculus/touch_controller",
                        {{g_actions.thumbLeft, "/user/hand/left/input/thumbstick"},
                         {g_actions.thumbRight, "/user/hand/right/input/thumbstick"},
                         {g_actions.trigger, "/user/hand/left/input/trigger"},
                         {g_actions.trigger, "/user/hand/right/input/trigger"},
                         {g_actions.squeeze, "/user/hand/left/input/squeeze/value"},
                         {g_actions.squeeze, "/user/hand/right/input/squeeze/value"}}});
    profiles.push_back({"/interaction_profiles/microsoft/motion_controller",
                        {{g_actions.thumbLeft, "/user/hand/left/input/thumbstick"},
                         {g_actions.thumbRight, "/user/hand/right/input/thumbstick"},
                         {g_actions.trigger, "/user/hand/left/input/trigger"},
                         {g_actions.trigger, "/user/hand/right/input/trigger"},
                         {g_actions.squeeze, "/user/hand/left/input/squeeze"},
                         {g_actions.squeeze, "/user/hand/right/input/squeeze"}}});
    profiles.push_back({"/interaction_profiles/valve/index_controller",
                        {{g_actions.thumbLeft, "/user/hand/left/input/thumbstick"},
                         {g_actions.thumbRight, "/user/hand/right/input/thumbstick"},
                         {g_actions.trigger, "/user/hand/left/input/trigger"},
                         {g_actions.trigger, "/user/hand/right/input/trigger"},
                         {g_actions.squeeze, "/user/hand/left/input/squeeze"},
                         {g_actions.squeeze, "/user/hand/right/input/squeeze"}}});

    for (auto& prof : profiles) {
        std::vector<XrActionSuggestedBinding> sugs;
        for (auto& b : prof.binds) {
            XrActionSuggestedBinding sb{};
            sb.action = b.first;
            if (xrStringToPath(g_engine.instance, b.second, &sb.binding) == XR_SUCCESS)
                sugs.push_back(sb);
        }
        XrInteractionProfileSuggestedBinding sug{};
        sug.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
        xrStringToPath(g_engine.instance, prof.path, &sug.interactionProfile);
        sug.suggestedBindings = sugs.data();
        sug.countSuggestedBindings = static_cast<uint32_t>(sugs.size());
        XrResult r = xrSuggestInteractionProfileBindings(g_engine.instance, &sug);
        if (r != XR_SUCCESS)
            core::Logger::warn(std::string("[VR] bindings rejected for ") + prof.path);
    }
    return true;
}

void pollEvents() {
    while (true) {
        XrEventDataBuffer ev{};
        ev.type = XR_TYPE_EVENT_DATA_BUFFER;
        XrResult r = xrPollEvent(g_engine.instance, &ev);
        if (r != XR_SUCCESS) break;
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto* s = reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);
            g_engine.sessionState = s->state;
            if (s->state == XR_SESSION_STATE_READY && !g_engine.began) {
                XrSessionBeginInfo bi{};
                bi.type = XR_TYPE_SESSION_BEGIN_INFO;
                bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                r = xrBeginSession(g_engine.session, &bi);
                if (r == XR_SUCCESS) {
                    g_engine.began = true;
                    core::Logger::info("[VR] session began");
                } else {
                    setErr("xrBeginSession failed", r);
                }
            } else if (s->state == XR_SESSION_STATE_SYNCHRONIZED) {
                core::Logger::info("[VR] session synchronized");
            }
        }
    }
}

void syncActions() {
    XrActionsSyncInfo sync{};
    sync.type = XR_TYPE_ACTIONS_SYNC_INFO;
    XrActiveActionSet actset{};
    actset.actionSet = g_actions.set;
    actset.subactionPath = XR_NULL_PATH;
    sync.activeActionSets = &actset;
    sync.countActiveActionSets = 1;
    if (xrSyncActions(g_engine.session, &sync) != XR_SUCCESS) return;

    XrActionStateGetInfo gi{};
    gi.type = XR_TYPE_ACTION_STATE_GET_INFO;
    gi.action = g_actions.thumbLeft;
    gi.subactionPath = g_actions.leftHand;
    XrActionStateVector2f v2{};
    v2.type = XR_TYPE_ACTION_STATE_VECTOR2F;
    if (xrGetActionStateVector2f(g_engine.session, &gi, &v2) == XR_SUCCESS && v2.isActive) {
        g_input.moveX = v2.currentState.x;
        g_input.moveY = v2.currentState.y;
    }
    gi.action = g_actions.thumbRight;
    v2 = {};
    v2.type = XR_TYPE_ACTION_STATE_VECTOR2F;
    if (xrGetActionStateVector2f(g_engine.session, &gi, &v2) == XR_SUCCESS && v2.isActive) {
        g_input.lookX = v2.currentState.x;
        g_input.lookY = v2.currentState.y;
    }
    XrActionStateBoolean b{};
    bool trig = false, sqz = false;
    for (XrPath hand : {g_actions.leftHand, g_actions.rightHand}) {
        gi.action = g_actions.trigger;
        gi.subactionPath = hand;
        b = {};
        b.type = XR_TYPE_ACTION_STATE_BOOLEAN;
        if (xrGetActionStateBoolean(g_engine.session, &gi, &b) == XR_SUCCESS && b.currentState)
            trig = true;
        gi.action = g_actions.squeeze;
        b = {};
        b.type = XR_TYPE_ACTION_STATE_BOOLEAN;
        if (xrGetActionStateBoolean(g_engine.session, &gi, &b) == XR_SUCCESS && b.currentState)
            sqz = true;
    }
    g_input.triggerPressed = trig;
    g_input.squeezePressed = sqz;
    g_input.hmdValid = true;
}

}  // namespace

VrMode initSession() {
    if (!createInstance()) return VrMode::Flat;
    if (!createSystemAndSession()) {
        destroyAll();
        return VrMode::Flat;
    }
    if (!createSpaceAndSwapchains()) {
        destroyAll();
        return VrMode::Flat;
    }
    if (!createActions()) {
        destroyAll();
        return VrMode::Flat;
    }
    g_activeFlag = true;
    g_engine.lastError.clear();
    core::Logger::info("[VR] OpenXR session created (waiting for headset begin)");
    return VrMode::Ready;
}

bool active() {
    return g_activeFlag;
}

bool beginFrame() {
    if (!g_activeFlag) return false;
    pollEvents();

    if (!g_engine.began || (g_engine.sessionState != XR_SESSION_STATE_FOCUSED &&
                            g_engine.sessionState != XR_SESSION_STATE_VISIBLE)) {
        XrFrameState fs{};
        fs.type = XR_TYPE_FRAME_STATE;
        if (xrWaitFrame(g_engine.session, nullptr, &fs) != XR_SUCCESS) return false;
        XrResult r = xrBeginFrame(g_engine.session, nullptr);
        if (r == XR_SUCCESS) {
            XrFrameEndInfo endInfo{};
            endInfo.type = XR_TYPE_FRAME_END_INFO;
            endInfo.displayTime = fs.predictedDisplayTime;
            endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            xrEndFrame(g_engine.session, &endInfo);
        }
        return false;
    }

    XrFrameState fs{};
    fs.type = XR_TYPE_FRAME_STATE;
    if (xrWaitFrame(g_engine.session, nullptr, &fs) != XR_SUCCESS) return false;
    if (xrBeginFrame(g_engine.session, nullptr) != XR_SUCCESS) return false;
    g_engine.displayTime = fs.predictedDisplayTime;
    g_engine.frameActive = true;

    if (g_engine.sessionState == XR_SESSION_STATE_FOCUSED) syncActions();

    XrViewLocateInfo locate{};
    locate.type = XR_TYPE_VIEW_LOCATE_INFO;
    locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    locate.displayTime = g_engine.displayTime;
    locate.space = g_engine.localSpace;
    XrView located[2]{};
    located[0].type = XR_TYPE_VIEW;
    located[1].type = XR_TYPE_VIEW;
    uint32_t viewCount = 0;
    XrViewState state{};
    state.type = XR_TYPE_VIEW_STATE;
    XrResult r = xrLocateViews(g_engine.session, &locate, &state, 2, &viewCount, located);
    if (r != XR_SUCCESS || viewCount < 2) {
        g_engine.frameActive = false;
        XrFrameEndInfo endInfo{};
        endInfo.type = XR_TYPE_FRAME_END_INFO;
        endInfo.displayTime = g_engine.displayTime;
        xrEndFrame(g_engine.session, &endInfo);
        return false;
    }
    for (int i = 0; i < 2 && i < static_cast<int>(viewCount); ++i) {
        g_engine.pose[i] = located[i].pose;
        g_engine.fov[i] = located[i].fov;
    }

    for (int eye = 0; eye < 2; ++eye) {
        XrSwapchainImageAcquireInfo ai{};
        ai.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
        r = xrAcquireSwapchainImage(g_engine.swapchain[eye], &ai, &g_engine.acquired[eye]);
        if (r != XR_SUCCESS) {
            g_engine.acquiredValid[eye] = false;
            continue;
        }
        XrSwapchainImageWaitInfo wi{};
        wi.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
        wi.timeout = 1000000000ull;
        if (xrWaitSwapchainImage(g_engine.swapchain[eye], &wi) != XR_SUCCESS) {
            g_engine.acquiredValid[eye] = false;
            continue;
        }
        g_engine.acquiredValid[eye] = true;
    }
    return g_engine.acquiredValid[0] && g_engine.acquiredValid[1];
}

bool bindEye(int eye) {
    if (!g_engine.frameActive || !g_engine.acquiredValid[eye] ||
        g_engine.images[eye].empty())
        return false;
    const SwapImage& img =
        g_engine.images[eye][g_engine.acquired[eye] % g_engine.images[eye].size()];
    glueFlushBatch();
    glueBindFramebuffer(img.fbo);
    glueViewport(0, 0, static_cast<int>(img.width), static_cast<int>(img.height));
    glueClearColor(18, 18, 24, 255);
    glueClearBuffers();
    return true;
}

void unbindEye(int eye) {
    (void)eye;
    glueFlushBatch();
    glueBindFramebuffer(0);
    glueViewport(0, 0, glueScreenWidth(), glueScreenHeight());
}

void getEyePose(int eye, EyePose* out) {
    if (!out || eye < 0 || eye > 1) return;
    const XrPosef& p = g_engine.pose[eye];
    out->px = p.position.x;
    out->py = p.position.y;
    out->pz = p.position.z;
    out->qx = p.orientation.x;
    out->qy = p.orientation.y;
    out->qz = p.orientation.z;
    out->qw = p.orientation.w;
    out->fovUp = g_engine.fov[eye].angleUp;
    out->fovDown = g_engine.fov[eye].angleDown;
    out->fovLeft = g_engine.fov[eye].angleLeft;
    out->fovRight = g_engine.fov[eye].angleRight;
}

VrInput input() {
    return g_input;
}

const char* lastError() {
    return g_engine.lastError.c_str();
}

void endFrame() {
    if (!g_engine.frameActive) return;
    g_engine.frameActive = false;

    if (g_engine.acquiredValid[0] && g_engine.acquiredValid[1]) {
        XrCompositionLayerProjectionView layerViews[2]{};
        for (int eye = 0; eye < 2; ++eye) {
            const SwapImage& img =
                g_engine.images[eye][g_engine.acquired[eye] % g_engine.images[eye].size()];
            layerViews[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
            layerViews[eye].pose = g_engine.pose[eye];
            layerViews[eye].fov = g_engine.fov[eye];
            layerViews[eye].subImage.swapchain = g_engine.swapchain[eye];
            layerViews[eye].subImage.imageArrayIndex = 0;
            layerViews[eye].subImage.imageRect.offset.x = 0;
            layerViews[eye].subImage.imageRect.offset.y = 0;
            layerViews[eye].subImage.imageRect.extent.width = img.width;
            layerViews[eye].subImage.imageRect.extent.height = img.height;
        }
        XrCompositionLayerProjection proj{};
        proj.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
        proj.space = g_engine.localSpace;
        proj.viewCount = 2;
        proj.views = layerViews;
        const XrCompositionLayerBaseHeader* layers[1] = {
            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&proj)};
        XrFrameEndInfo endInfo{};
        endInfo.type = XR_TYPE_FRAME_END_INFO;
        endInfo.displayTime = g_engine.displayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endInfo.layerCount = 1;
        endInfo.layers = layers;
        XrResult r = xrEndFrame(g_engine.session, &endInfo);
        if (r != XR_SUCCESS && !g_submitWarned) {
            g_submitWarned = true;
            setErr("xrEndFrame failed", r);
        }
    } else {
        XrFrameEndInfo endInfo{};
        endInfo.type = XR_TYPE_FRAME_END_INFO;
        endInfo.displayTime = g_engine.displayTime;
        xrEndFrame(g_engine.session, &endInfo);
    }

    for (int eye = 0; eye < 2; ++eye) {
        if (g_engine.acquiredValid[eye]) {
            XrSwapchainImageReleaseInfo ri{};
            ri.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
            xrReleaseSwapchainImage(g_engine.swapchain[eye], &ri);
        }
        g_engine.acquiredValid[eye] = false;
    }
}

}  // namespace slashco_vr

#else

#include "OpenXRBackend.hpp"

namespace slashco_vr {

VrMode initSession() {
    return VrMode::Flat;
}

bool active() {
    return false;
}

bool beginFrame() {
    return false;
}

void endFrame() {
}

bool bindEye(int) {
    return false;
}

void unbindEye(int) {
}

void getEyePose(int, EyePose*) {
}

VrInput input() {
    return VrInput{};
}

const char* lastError() {
    return "OpenXR not compiled in";
}

}  // namespace slashco_vr

#endif
