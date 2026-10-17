# ShadowChase / SlashCo – Platforms

Build matrix for the whole project (server + client both run on every listed box).

## 1. Desktops (Windows / Linux / macOS)
Toolchain: CMake ≥3.20 with a C++17 compiler (MSVC 2022, GCC ≥11, Clang ≥13) + Ninja or Visual Studio.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release        # -A x64 on Windows optional
cmake --build build --config Release --parallel
./build/Release/SlashCoServer.exe                     # dedicated host
./build/Release/SlashCoClient.exe --host 1            # local host mode
```

Notes
* Windows needs MSVC 2022 / /utf-8 (already wired in CMake).
* Linux/macOS Qt or other backends are unnecessary; raylib 5.5 handles them, e.g.
  `sudo apt install libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libgl1-mesa-dev`.
* macOS: allow unnotarized binary in Gatekeeper (first run).
* Final ZIP for distribution: `cmake --build build --config Release --executor companion --target package`.

## 2. Android phone (arm64-v8a)
Build without gradle: pure NDK + CMake + hand-rolled aapt2.

```powershell
packaging/android/build_apk.ps1 [-Abi arm64-v8a] [-SkipBuild]
```
Output artifact lands at `SlashCo-<version>-android-<abi>.apk` (repo root).
* Built with NativeActivity (raylib PLATFORM_ANDROID) and an embedded dedicated-server
  core so a phone can self-host (`client.host=1` inside `packaging/android/assets/config.ini`).
* Controls: left half = virtual joystick, right half drag = camera, USE button = interact,
  C crouch, FLSH flashlight, THRW throw, DROP drop (tap 3 times = drop all).
* Useful debug: `adb install -r SlashCo-0.3.0-android-arm64-v8a.apk`, and the dedicated
  server can run on the phone with `SlashCoClient` on the same LAN (room code from `config.ini`).
* Also runs on Quest (Arm64 build; UI works with a controller).

## 3. Android VR / PCVR (OpenXR) — planned milestone
The raylib client renders desktop/approaches; a dedicated OpenXR renderer needs:

1. `FetchContent` OpenXR-SDK-Source (Khronos) to build `loader` (C++17, CMake).
2. `SlashCoClientVR` / `SlashCoXR` target linking the loader + runtime shaders,
   camera-to-eye transforms + action bindings (OpenXR desktop on Windows/SteamVR, and
   `android` variant calling libopenxr_loader.so from Meta SDK AAR for Quest).

Stubs: `SLASHCO_ENABLE_OPENXR` flag already present in CMake (renders a plain 2D fallback so the
program proves). True headset support is the next milestone (needs Meta Mobile SDK /
SteamVR runtime on the build/test machine).

## Roadmap
- [x] Interview client on desktop (win/linux/mac from same CMake tree).
- [x] Android (arm64-v8a) hands-on playable build.
- [ ] Android VR (Quest) via OpenXR AAR loader + runtime checks.
- [ ] PCVR (OpenXR desktop: SteamVR / Meta Link).
- [ ] CI pipelines (linux/macos build tools run on the free GitHub Linux runner) — keep
      `.github/workflows/` local since this token is `workflow`-less.
