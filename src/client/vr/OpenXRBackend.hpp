#pragma once

// OpenXR capability probe. Attempting a session is abandoned very quickly for
// builds that do not ship a headset runtime; this makes the same binary work
// on desktop (flat) and on Quest (VR) without extra build IDs.

namespace slashco_vr {

// Returns true when an OpenXR runtime was detected and an instance was created
// and destroyed successfully. Logs runtime/hardware via core::Logger.
// Never throws; any failure yields false and the client keeps running flat.
bool probeRuntime();

}
