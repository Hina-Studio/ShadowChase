#include "OpenXRBackend.hpp"

#include <cstring>
#include <string>

#include "core/Logger.hpp"

#if defined(SLASHCO_OPENXR)
#include <openxr/openxr.h>

namespace slashco_vr {

bool probeRuntime() {
    XrInstanceCreateInfo info{};
    info.type = XR_TYPE_INSTANCE_CREATE_INFO;
    info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    std::strncpy(info.applicationInfo.applicationName, "SlashCo",
                 XR_MAX_APPLICATION_NAME_SIZE - 1);
    std::strncpy(info.applicationInfo.engineName, "ShadowChase", XR_MAX_ENGINE_NAME_SIZE - 1);
    info.applicationInfo.engineVersion = 1;
    info.applicationInfo.applicationVersion = 1;
    info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    XrInstance instance = XR_NULL_HANDLE;
    XrResult result = xrCreateInstance(&info, &instance);
    if (result != XR_SUCCESS) {
        core::Logger::warn("[VR] OpenXR xrCreateInstance failed: " + std::to_string(result) +
                           " (flat mode)");
        return false;
    }

    XrInstanceProperties props{};
    props.type = XR_TYPE_INSTANCE_PROPERTIES;
    xrGetInstanceProperties(instance, &props);
    core::Logger::info(std::string("[VR] runtime=") + props.runtimeName +
                       " spec=" + std::to_string(XR_VERSION_MAJOR(props.runtimeVersion)) + "." +
                       std::to_string(XR_VERSION_MINOR(props.runtimeVersion)));

    XrSystemGetInfo sysInfo{};
    sysInfo.type = XR_TYPE_SYSTEM_GET_INFO;
    sysInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    result = xrGetSystem(instance, &sysInfo, &systemId);
    if (result == XR_SUCCESS) {
        XrSystemProperties sysProps{};
        sysProps.type = XR_TYPE_SYSTEM_PROPERTIES;
        xrGetSystemProperties(instance, systemId, &sysProps);
        core::Logger::info(std::string("[VR] hardware=") + sysProps.systemName +
                           " vendor=" +
                           std::to_string(sysProps.graphicsProperties.maxLayerCount));
        core::Logger::info("[VR] session pending (renderer milestone)");
    } else {
        core::Logger::info("[VR] no HMD found (" + std::to_string(result) + "); flat mode");
    }

    xrDestroyInstance(instance);
    return result == XR_SUCCESS;
}

}  // namespace slashco_vr

#else

namespace slashco_vr {

bool probeRuntime() { return false; }

}  // namespace slashco_vr

#endif
