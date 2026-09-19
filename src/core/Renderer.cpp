#include "core/Renderer.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

#include "core/Logger.hpp"
#include "core/Loc.hpp"
#include "game/World.hpp"

#include <vector>

#ifdef SLASHCO_RAYLIB
#include <raylib.h>
#include <rlgl.h>

#ifdef _WIN32
extern "C" const unsigned char* __stdcall glGetString(unsigned int name);
#else
extern "C" const unsigned char* glGetString(unsigned int name);
#endif
#endif

namespace {
struct GpuPreset {
    int msaa = 0;
    int segments = 24;
    int fps = 60;
    int grid = 1;
    int vhs = 1;
    bool vsync = true;
};

GpuPreset presetFor(const std::string& name) {
    GpuPreset p;
    std::string n = name;
    for (auto& c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (n == "high" || n == "nvidia" || n == "amd" || n == "radeon") {
        p.msaa = 4;
        p.segments = 48;
        p.fps = 0;
        p.grid = 1;
        p.vhs = 1;
    } else if (n == "low" || n == "potato") {
        p.msaa = 0;
        p.segments = 12;
        p.fps = 30;
        p.grid = 0;
        p.vhs = 0;
    } else if (n == "intel" || n == "balanced") {
        p.msaa = 0;
        p.segments = 24;
        p.fps = 60;
        p.grid = 1;
        p.vhs = 1;
    } else {
        p.msaa = 0;
        p.segments = 24;
        p.fps = 60;
        p.grid = 1;
        p.vhs = 1;
    }
    return p;
}
}

Renderer::Renderer() : screenW(1280), screenH(720), ready(false) {}

Renderer::~Renderer() {
    shutdown();
}

void Renderer::init(const RendererSettings& settings) {
    screenW = settings.width;
    screenH = settings.height;
    preset = settings.gpuPreset;

    GpuPreset p = presetFor(settings.gpuPreset);
    if (settings.msaa >= 0) p.msaa = settings.msaa;
    if (settings.maxFPS >= 0) p.fps = settings.maxFPS;
    if (settings.grid >= 0) p.grid = settings.grid;
    if (settings.vhs >= 0) p.vhs = settings.vhs;
    if (settings.mode3d >= 0) mode3d = settings.mode3d != 0;

    segments = p.segments;
    maxFPS = p.fps;
    msaaOn = p.msaa >= 4;
    drawGrid = p.grid != 0;
    vsync = p.vsync;
    vhsOn = p.vhs != 0;

#ifdef SLASHCO_RAYLIB
    unsigned int flags = FLAG_VSYNC_HINT;
    if (msaaOn) flags |= FLAG_MSAA_4X_HINT;
    SetConfigFlags(flags);

    InitWindow(screenW, screenH, settings.title.c_str());
    SetTargetFPS(maxFPS > 0 ? maxFPS : 0);
    ready = true;

    detectVendor();

    if (settings.gpuPreset == "auto" || settings.gpuPreset.empty()) {
        if (vendor.find("intel") != std::string::npos) {
            preset = "intel";
            if (settings.msaa < 0) msaaOn = false;
            if (settings.grid < 0) drawGrid = true;
            if (settings.maxFPS < 0) maxFPS = 60;
            if (maxFPS > 0) SetTargetFPS(maxFPS);
        } else if (vendor.find("nvidia") != std::string::npos ||
                   vendor.find("amd") != std::string::npos ||
                   vendor.find("ati") != std::string::npos) {
            preset = "high";
        }
    }

    if (vhsOn) {
        RenderTexture2D* rt = new RenderTexture2D(LoadRenderTexture(screenW, screenH));
        SetTextureFilter(rt->texture, TEXTURE_FILTER_POINT);
        sceneTarget = rt;
    }

    {
        std::vector<int> cps = core::Loc::instance().zhCodepoints();
        const char* candidates[] = {"C:/Windows/Fonts/msyh.ttc",
                                    "C:/Windows/Fonts/simhei.ttf",
                                    "C:/Windows/Fonts/simsun.ttc",
                                    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
                                    "/System/Library/Fonts/PingFang.ttc"};
        for (const char* path : candidates) {
            if (!FileExists(path)) continue;
            Font f = LoadFontEx(path, 28, cps.data(), static_cast<int>(cps.size()));
            if (f.texture.id != 0) {
                SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
                uiFont = new Font(f);
                unicodeFont = true;
                break;
            }
        }
        core::Logger::info(unicodeFont ? "Unicode font loaded"
                                       : "Unicode font not found (ASCII only)");
    }

    core::Logger::info("GPU vendor=" + vendor + " preset=" + preset + " segments=" +
                       std::to_string(segments) + " fpsCap=" + std::to_string(maxFPS) +
                       (vhsOn ? " vhs=on" : " vhs=off"));
#else
    ready = false;
#endif
}

void Renderer::detectVendor() {
#ifdef SLASHCO_RAYLIB
    const char* raw = reinterpret_cast<const char*>(glGetString(0x1F00));
    if (raw && raw[0] != '\0') {
        std::string v = raw;
        for (auto& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        vendor = v;
    }
#endif
}

void Renderer::shutdown() {
#ifdef SLASHCO_RAYLIB
    if (uiFont) {
        UnloadFont(*static_cast<Font*>(uiFont));
        delete static_cast<Font*>(uiFont);
        uiFont = nullptr;
        unicodeFont = false;
    }
    if (sceneTarget) {
        RenderTexture2D* rt = static_cast<RenderTexture2D*>(sceneTarget);
        UnloadRenderTexture(*rt);
        delete rt;
        sceneTarget = nullptr;
    }
    if (ready) {
        CloseWindow();
        ready = false;
    }
#endif
}

bool Renderer::shouldClose() const {
#ifdef SLASHCO_RAYLIB
    return ready && WindowShouldClose();
#else
    return false;
#endif
}

void Renderer::setVhs(bool on) {
#ifdef SLASHCO_RAYLIB
    vhsOn = on;
    if (on && !sceneTarget && ready) {
        RenderTexture2D* rt = new RenderTexture2D(LoadRenderTexture(screenW, screenH));
        SetTextureFilter(rt->texture, TEXTURE_FILTER_POINT);
        sceneTarget = rt;
    }
#else
    vhsOn = on;
#endif
}

void Renderer::setGrid(bool on) {
    drawGrid = on;
}

void Renderer::resize(int width, int height) {
#ifdef SLASHCO_RAYLIB
    if (!ready) return;
    screenW = width;
    screenH = height;
    SetWindowSize(width, height);
    if (sceneTarget) {
        RenderTexture2D* rt = static_cast<RenderTexture2D*>(sceneTarget);
        UnloadRenderTexture(*rt);
        delete rt;
        sceneTarget = nullptr;
    }
    if (vhsOn) {
        RenderTexture2D* rt = new RenderTexture2D(LoadRenderTexture(screenW, screenH));
        SetTextureFilter(rt->texture, TEXTURE_FILTER_POINT);
        sceneTarget = rt;
    }
#else
    (void)width;
    (void)height;
#endif
}

void Renderer::addShake(float strength) {
#ifdef SLASHCO_RAYLIB
    shake = std::max(shake, strength);
#else
    (void)strength;
#endif
}

void Renderer::addHitFx(double x, double z, const std::string& text, unsigned char r, unsigned char g,
                        unsigned char b) {
#ifdef SLASHCO_RAYLIB
    RenderFx fx;
    fx.x = x;
    fx.z = z;
    fx.life = 1.0;
    fx.text = text;
    fx.r = r;
    fx.g = g;
    fx.b = b;
    fxs.push_back(fx);
#else
    (void)x;
    (void)z;
    (void)text;
    (void)r;
    (void)g;
    (void)b;
#endif
}

void Renderer::setMouseCapture(bool on) {
#ifdef SLASHCO_RAYLIB
    if (on == mouseCaptured) return;
    mouseCaptured = on;
    if (on) {
        DisableCursor();
    } else {
        EnableCursor();
    }
#else
    mouseCaptured = on;
#endif
}

void Renderer::resetCamera() {
    camYaw = 0.0f;
    camPitch = 0.0f;
}

void Renderer::setTutorialMarker(bool active, double x, double z) {
    tutorActive = active;
    tutorX = x;
    tutorZ = z;
}

void Renderer::text(const char* s, int x, int y, int size, unsigned char r, unsigned char g,
                    unsigned char b, unsigned char a) {
#ifdef SLASHCO_RAYLIB
    if (unicodeFont && uiFont) {
        DrawTextEx(*static_cast<Font*>(uiFont), s, Vector2{static_cast<float>(x), static_cast<float>(y)},
                   static_cast<float>(size), 1.0f, Color{r, g, b, a});
    } else {
        DrawText(s, x, y, size, Color{r, g, b, a});
    }
#else
    (void)s;
    (void)x;
    (void)y;
    (void)size;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
#endif
}

#ifdef SLASHCO_RAYLIB
namespace {
struct Rgb {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

Color toColor(const Rgb& c, int a = 255) {
    return Color{c.r, c.g, c.b, static_cast<unsigned char>(a)};
}

const Rgb kBg{24, 24, 32};
const Rgb kGrid{40, 40, 54};
const Rgb kSurvivorCol{70, 200, 120};
const Rgb kLocalCol{90, 240, 255};
const Rgb kKillerCol{230, 60, 60};
const Rgb kGenCol{240, 200, 60};
const Rgb kExitCol{110, 160, 255};
const Rgb kTextCol{220, 220, 225};
const Rgb kAlertCol{255, 210, 90};
const Rgb kWallCol{150, 105, 60};

double clamp01(double v) {
    return std::max(0.0, std::min(1.0, v));
}

void drawOverlay(const UiFrame& ui, int w, int h, const std::string& vendor,
                 const std::string& preset, int segments, Renderer& owner) {
    auto text = [&owner](const char* s, int x, int y, int size, unsigned char r, unsigned char g,
                         unsigned char b, unsigned char a = 255) {
        owner.text(s, x, y, size, r, g, b, a);
    };
    if (ui.mode == "none") return;

    if (ui.mode == "tutorial") {
        DrawRectangle(0, 0, w, 92, Color{0, 0, 0, 175});
        text(ui.title.c_str(), 12, 8, 22, kAlertCol.r, kAlertCol.g, kAlertCol.b);
        int ly = 38;
        if (!ui.subtitle.empty()) {
            text(ui.subtitle.c_str(), 12, ly, 16, kTextCol.r, kTextCol.g, kTextCol.b);
            ly += 22;
        }
        for (const auto& line : ui.lines) {
            text(line.c_str(), 12, ly, 16, kTextCol.r, kTextCol.g, kTextCol.b);
            ly += 20;
        }
        return;
    }

    DrawRectangle(0, 0, w, h, Color{0, 0, 0, 190});

    int titleSize = 48;
    int tw = MeasureText(ui.title.c_str(), titleSize);
    text(ui.title.c_str(), (w - tw) / 2, h / 3 - 70, titleSize, kAlertCol.r, kAlertCol.g,
         kAlertCol.b);

    if (!ui.subtitle.empty()) {
        int sw2 = MeasureText(ui.subtitle.c_str(), 20);
        text(ui.subtitle.c_str(), (w - sw2) / 2, h / 3 + 0, 20, kTextCol.r, kTextCol.g, kTextCol.b);
    }

    int y = h / 3 + 46;
    for (const auto& line : ui.lines) {
        int lw = MeasureText(line.c_str(), 18);
        text(line.c_str(), (w - lw) / 2, y, 18, kTextCol.r, kTextCol.g, kTextCol.b);
        y += 26;
    }

    if (!ui.options.empty()) {
        int oy = h / 3 + 66;
        for (size_t i = 0; i < ui.options.size(); ++i) {
            bool sel = (static_cast<int>(i) == ui.selected);
            std::string txt = (sel ? "> " : "  ") + ui.options[i];
            Color c = sel ? toColor(kAlertCol) : toColor(kTextCol);
            int ow = MeasureText(txt.c_str(), 20);
            text(txt.c_str(), (w - ow) / 2, oy, 20, c.r, c.g, c.b, c.a);
            oy += 30;
        }
    }

    std::string gpu = "GPU:" + vendor + " preset=" + preset + " seg=" + std::to_string(segments);
    text(gpu.c_str(), 12, h - 18, 10, 150, 150, 160);
}

void drawVhsFx(RenderTexture2D& rt, int w, int h, float shakeX, float shakeY) {
    Rectangle src{0.0f, 0.0f, static_cast<float>(rt.texture.width),
                  static_cast<float>(rt.texture.height)};
    Rectangle dst{shakeX, shakeY, static_cast<float>(w), static_cast<float>(h)};

    DrawTexturePro(rt.texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, WHITE);

    BeginBlendMode(BLEND_ADDITIVE);
    Rectangle dstR = dst;
    dstR.x += 1.5f;
    DrawTexturePro(rt.texture, src, dstR, Vector2{0.0f, 0.0f}, 0.0f, Color{255, 60, 60, 70});
    Rectangle dstB = dst;
    dstB.x -= 1.5f;
    DrawTexturePro(rt.texture, src, dstB, Vector2{0.0f, 0.0f}, 0.0f, Color{60, 255, 255, 70});
    EndBlendMode();

    for (int y = 0; y < h; y += 3) {
        DrawRectangle(0, y, w, 1, Color{0, 0, 0, 38});
    }

    static unsigned long long vhsState = 0x9E3779B97F4A7C15ULL;
    auto rnd = []() -> unsigned long long {
        vhsState += 0x9E3779B97F4A7C15ULL;
        unsigned long long z = vhsState;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    };
    for (int i = 0; i < 48; ++i) {
        int nx = static_cast<int>(rnd() % static_cast<unsigned>(w));
        int ny = static_cast<int>(rnd() % static_cast<unsigned>(h));
        int nw = 1 + static_cast<int>(rnd() % 3);
        int alpha = 8 + static_cast<int>(rnd() % 18);
        DrawRectangle(nx, ny, nw, 1, Color{255, 255, 255, static_cast<unsigned char>(alpha)});
    }

    if ((rnd() % 90) == 0) {
        DrawRectangle(0, 0, w, h, Color{255, 255, 255, 10});
    }

    DrawRectangleGradientV(0, 0, w, static_cast<int>(h * 0.15), Color{0, 0, 0, 120},
                           Color{0, 0, 0, 0});
    DrawRectangleGradientV(0, static_cast<int>(h * 0.85), w, static_cast<int>(h * 0.15),
                           Color{0, 0, 0, 0}, Color{0, 0, 0, 120});
    DrawRectangleGradientH(0, 0, static_cast<int>(w * 0.12), h, Color{0, 0, 0, 110},
                           Color{0, 0, 0, 0});
    DrawRectangleGradientH(static_cast<int>(w * 0.88), 0, static_cast<int>(w * 0.12), h,
                           Color{0, 0, 0, 0}, Color{0, 0, 0, 110});
}
}

void Renderer::drawSceneImpl(const game::World& world) {
    if (mode3d) {
        drawScene3DImpl(world);
    } else {
        drawScene2DImpl(world);
    }
}

void Renderer::drawScene2DImpl(const game::World& world) {
    const double margin = 40.0;
    const double m = std::min(screenW - margin * 2.0, screenH - margin * 2.0 - 60.0);
    const double s = m / 22.0;
    const double ox = screenW / 2.0;
    const double oy = screenH / 2.0 + 15.0;

    auto px = [&](double v) { return ox + v * s; };
    auto pz = [&](double v) { return oy - v * s; };

    if (drawGrid) {
        rlBegin(RL_LINES);
        rlColor4ub(kGrid.r, kGrid.g, kGrid.b, 255);
        for (int i = -11; i <= 11; ++i) {
            rlVertex2f(static_cast<float>(px(static_cast<double>(i))), static_cast<float>(oy - 11 * s));
            rlVertex2f(static_cast<float>(px(static_cast<double>(i))), static_cast<float>(oy + 11 * s));
            rlVertex2f(static_cast<float>(ox - 11 * s), static_cast<float>(pz(static_cast<double>(i))));
            rlVertex2f(static_cast<float>(ox + 11 * s), static_cast<float>(pz(static_cast<double>(i))));
        }
        rlEnd();
    }

    rlBegin(RL_QUADS);
    rlColor4ub(kWallCol.r, kWallCol.g, kWallCol.b, 255);
    for (int gx = 0; gx < game::NavGrid::Size; ++gx) {
        for (int gz = 0; gz < game::NavGrid::Size; ++gz) {
            if (!world.nav().blocked(gx, gz)) continue;
            double cx = px(static_cast<double>(gx - 11));
            double cz = pz(static_cast<double>(gz - 11));
            float hh = static_cast<float>(s * 0.48);
            float x0 = static_cast<float>(cx - hh);
            float y0 = static_cast<float>(cz - hh);
            float x1 = static_cast<float>(cx + hh);
            float y1 = static_cast<float>(cz + hh);
            rlVertex2f(x0, y0);
            rlVertex2f(x0, y1);
            rlVertex2f(x1, y1);
            rlVertex2f(x1, y0);
        }
    }
    rlEnd();

    for (const auto& e : world.exits()) {
        double r = s * 0.9;
        Color c = e.open ? toColor(kExitCol) : toColor(kTextCol, 90);
        Vector2 ctr{static_cast<float>(px(e.position.x)), static_cast<float>(pz(e.position.z))};
        DrawRectangle(static_cast<int>(ctr.x - r), static_cast<int>(ctr.y - r),
                      static_cast<int>(r * 2), static_cast<int>(r * 2), c);
        DrawText(e.open ? "EXIT" : "LOCKED", static_cast<int>(ctr.x - 18),
                 static_cast<int>(ctr.y - 4), 12, toColor(kBg));
    }

    {
        Vector2 ctr{static_cast<float>(px(world.vaultPos().x)), static_cast<float>(pz(world.vaultPos().z))};
        float vh = static_cast<float>(s * 0.7);
        int variant = world.vaultVariant();
        Rgb base = variant == 1 ? Rgb{90, 220, 140}
                                : (variant == 2 ? Rgb{230, 90, 90} : Rgb{190, 120, 255});
        Color vc = world.vaultOpened() ? toColor(base, 110) : toColor(base);
        DrawRectangle(static_cast<int>(ctr.x - vh), static_cast<int>(ctr.y - vh),
                      static_cast<int>(vh * 2), static_cast<int>(vh * 2), vc);
        const char* label = world.vaultOpened() ? "open"
                                                : (variant == 1 ? "CACHE" : (variant == 2 ? "TRAP" : "VAULT"));
        DrawText(label, static_cast<int>(ctr.x - 20), static_cast<int>(ctr.y - 6), 12, toColor(kBg));
    }

    if (world.modeName() == "Blackout") {
        Vector2 ctr{static_cast<float>(px(world.powerSwitchPos().x)),
                    static_cast<float>(pz(world.powerSwitchPos().z))};
        float r = static_cast<float>(s * 0.5);
        Color pc = world.powerSwitchActivated() ? toColor(Rgb{80, 230, 120}) : toColor(Rgb{90, 200, 255});
        DrawCircleSector(ctr, r, 0.0f, 360.0f, segments, pc);
        DrawText(world.powerSwitchActivated() ? "ON" : "P", static_cast<int>(ctr.x - 6),
                 static_cast<int>(ctr.y - 8), 16, toColor(kBg));
    }

    for (const auto& g : world.generators()) {
        Vector2 ctr{static_cast<float>(px(g.position.x)), static_cast<float>(pz(g.position.z))};
        float r = static_cast<float>(s * 0.55);
        DrawCircleSector(ctr, r, 0.0f, 360.0f, segments, toColor(kGenCol, 70));
        DrawCircleSectorLines(ctr, r, 0.0f, 360.0f, segments, toColor(kGenCol));
        float frac = g.activated ? 1.0f : static_cast<float>(clamp01(g.work / g.fuelMax));
        DrawRing(ctr, r - 4.0f, r, 0.0f, 360.0f * frac, segments,
                 toColor(g.activated ? Rgb{80, 230, 120} : Rgb{255, 180, 60}));
        DrawText("G", static_cast<int>(ctr.x - 6), static_cast<int>(ctr.y - 8), 16, toColor(kGenCol));
    }

    for (const auto& sv : world.survivors()) {
        if (sv.eliminated || sv.escaped) continue;
        Vector2 ctr{static_cast<float>(px(sv.position.x)), static_cast<float>(pz(sv.position.z))};
        float r = static_cast<float>(s * 0.38);
        Color c = toColor(kSurvivorCol);
        if (sv.downed) {
            c = toColor(Rgb{160, 160, 170});
        }
        DrawCircleSector(ctr, r, 0.0f, 360.0f, segments, c);
        DrawCircleSectorLines(ctr, r, 0.0f, 360.0f, segments, toColor(Rgb{255, 255, 255}, 140));
        if (sv.id == spectateId) {
            DrawRing(ctr, r + 3.0f, r + 8.0f, 0.0f, 360.0f, segments, toColor(Rgb{255, 220, 90}));
        }
        std::string tag = std::to_string(sv.id);
        if (sv.downed) tag = "D";
        DrawText(tag.c_str(), static_cast<int>(ctr.x - 4), static_cast<int>(ctr.y - 8), 16, toColor(kBg));
        if (sv.downed) {
            DrawText(("revive " + std::to_string(std::max(0, static_cast<int>(sv.downTimer)))).c_str(),
                     static_cast<int>(ctr.x - 22), static_cast<int>(ctr.y - r - 18), 10, toColor(kTextCol));
        }
    }

    if (!world.over()) {
        for (const auto& k : world.killers()) {
            Vector2 ctr{static_cast<float>(px(k.position().x)), static_cast<float>(pz(k.position().z))};
            float r = static_cast<float>(s * 0.5);
            if (k.kind() == game::KillerAI::Kind::Whisper) {
                DrawCircleSector(ctr, static_cast<float>(s * 6.0), 0.0f, 360.0f, segments,
                                 toColor(Rgb{90, 160, 255}, 18));
                DrawCircleSectorLines(ctr, static_cast<float>(s * 6.0), 0.0f, 360.0f, segments,
                                      toColor(Rgb{120, 200, 255}, 70));
            } else if (k.kind() == game::KillerAI::Kind::Warden) {
                DrawCircleSector(ctr, static_cast<float>(s * 8.0), 0.0f, 360.0f, segments,
                                 toColor(Rgb{80, 220, 200}, 14));
                DrawCircleSectorLines(ctr, static_cast<float>(s * 8.0), 0.0f, 360.0f, segments,
                                      toColor(Rgb{90, 230, 205}, 70));
            }
            Color kc = k.isTemporaryKiller() ? toColor(Rgb{255, 120, 60}) : toColor(kKillerCol);
            if (k.kind() == game::KillerAI::Kind::Butcher) {
                r = static_cast<float>(s * 0.65);
                kc = toColor(Rgb{200, 80, 220});
            }
            DrawCircleSector(ctr, r, 0.0f, 360.0f, segments, kc);
            DrawRing(ctr, r, r + 4.0f, 0.0f, 360.0f, segments, toColor(kAlertCol));
            DrawText(k.kind() == game::KillerAI::Kind::Butcher ? "B" : "K",
                     static_cast<int>(ctr.x - 7), static_cast<int>(ctr.y - 10), 20, toColor(kBg));
        }
    }

    if (tutorActive) {
        Vector2 ctr{static_cast<float>(px(tutorX)), static_cast<float>(pz(tutorZ))};
        float pulse = 0.6f + 0.15f * std::sin(static_cast<float>(GetTime()) * 5.0f);
        DrawCircleSector(ctr, static_cast<float>(s * pulse), 0.0f, 360.0f, segments,
                         toColor(Rgb{90, 240, 255}, 40));
        DrawCircleSectorLines(ctr, static_cast<float>(s * pulse), 0.0f, 360.0f, segments,
                              toColor(Rgb{90, 240, 255}, 200));
        DrawText("HERE", static_cast<int>(ctr.x - 16), static_cast<int>(ctr.y - 6), 12,
                 toColor(Rgb{90, 240, 255}));
    }

    for (const auto& fx : fxs) {
        double life = std::max(0.0, std::min(1.0, fx.life));
        int alpha = static_cast<int>(life * 255.0);
        float rise = static_cast<float>((1.0 - life) * 18.0);
        DrawText(fx.text.c_str(), static_cast<int>(px(fx.x) - 10),
                 static_cast<int>(pz(fx.z) - 22 - rise), 16,
                 Color{fx.r, fx.g, fx.b, static_cast<unsigned char>(alpha)});
    }

    if (world.modeName() == "Blackout" && !world.over()) {
        if (const game::Survivor* lp = world.localSurvivor()) {
            Vector2 ctr{static_cast<float>(px(lp->position.x)), static_cast<float>(pz(lp->position.z))};
            DrawRectangle(0, 0, screenW, screenH, Color{0, 0, 0, 70});
            DrawRing(ctr, static_cast<float>(s * 6.0), static_cast<float>(s * 60.0), 0.0f, 360.0f,
                     segments, Color{0, 0, 0, 205});
        }
    }
}

void Renderer::drawScene3DImpl(const game::World& world) {
    const game::Survivor* focus = world.localSurvivor();
    if (!focus) {
        for (const auto& s : world.survivors()) {
            if (!s.eliminated && !s.escaped) {
                focus = &s;
                break;
            }
        }
    }

    Vector3 eye{0.0f, 7.0f, 7.0f};
    Vector3 target{0.0f, 0.0f, 0.0f};
    if (focus) {
        eye = Vector3{static_cast<float>(focus->position.x), 1.65f,
                      static_cast<float>(focus->position.z)};
    }

    if (mouseCaptured) {
        Vector2 d = GetMouseDelta();
        camYaw -= d.x * 0.003f;
        camPitch -= d.y * 0.003f;
        if (camPitch > 1.45f) camPitch = 1.45f;
        if (camPitch < -1.45f) camPitch = -1.45f;
    }

    float cp = std::cos(camPitch);
    Vector3 fwd{std::sin(camYaw) * cp, std::sin(camPitch), std::cos(camYaw) * cp};
    if (focus) {
        target = Vector3{eye.x + fwd.x, eye.y + fwd.y, eye.z + fwd.z};
    }

    Camera3D cam{};
    cam.position = eye;
    cam.target = target;
    cam.up = Vector3{0.0f, 1.0f, 0.0f};
    cam.fovy = 72.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    BeginMode3D(cam);

    DrawPlane(Vector3{0.0f, 0.0f, 0.0f}, Vector2{22.0f, 22.0f}, toColor(Rgb{30, 30, 38}));
    DrawGrid(22, 1.0f);

    for (int gx = 0; gx < game::NavGrid::Size; ++gx) {
        for (int gz = 0; gz < game::NavGrid::Size; ++gz) {
            if (!world.nav().blocked(gx, gz)) continue;
            Vector3 c{static_cast<float>(gx - 11), 1.5f, static_cast<float>(gz - 11)};
            DrawCube(c, 0.98f, 3.0f, 0.98f, toColor(kWallCol));
            DrawCubeWires(c, 0.98f, 3.0f, 0.98f, toColor(Rgb{90, 62, 36}));
        }
    }

    for (const auto& e : world.exits()) {
        Vector3 c{static_cast<float>(e.position.x), 1.5f, static_cast<float>(e.position.z)};
        DrawCube(c, 2.0f, 3.0f, 0.5f, e.open ? toColor(kExitCol) : toColor(Rgb{90, 90, 100}));
    }

    for (const auto& g : world.generators()) {
        Vector3 c{static_cast<float>(g.position.x), 0.6f, static_cast<float>(g.position.z)};
        Color gc = g.activated ? toColor(Rgb{80, 230, 120}) : toColor(kGenCol);
        DrawCube(c, 1.0f, 1.2f, 1.0f, gc);
        DrawCubeWires(c, 1.0f, 1.2f, 1.0f, toColor(kBg));
    }

    {
        Vector3 c{static_cast<float>(world.vaultPos().x), 0.7f,
                  static_cast<float>(world.vaultPos().z)};
        int variant = world.vaultVariant();
        Rgb base = variant == 1 ? Rgb{90, 220, 140}
                                : (variant == 2 ? Rgb{230, 90, 90} : Rgb{190, 120, 255});
        DrawCube(c, 1.4f, 1.4f, 1.4f, world.vaultOpened() ? toColor(base, 90) : toColor(base));
    }

    if (world.modeName() == "Blackout") {
        Vector3 c{static_cast<float>(world.powerSwitchPos().x), 0.8f,
                  static_cast<float>(world.powerSwitchPos().z)};
        DrawCube(c, 0.8f, 1.6f, 0.8f,
                 world.powerSwitchActivated() ? toColor(Rgb{80, 230, 120}) : toColor(Rgb{90, 200, 255}));
    }

    for (const auto& s : world.survivors()) {
        if (s.eliminated || s.escaped) continue;
        Vector3 a{static_cast<float>(s.position.x), 0.45f, static_cast<float>(s.position.z)};
        Vector3 b{static_cast<float>(s.position.x), 1.25f, static_cast<float>(s.position.z)};
        Color sc = s.downed ? toColor(Rgb{160, 160, 170}) : toColor(kSurvivorCol);
        DrawCapsule(a, b, s.downed ? 0.45f : 0.35f, 8, 8, sc);
    }

    for (const auto& k : world.killers()) {
        Rgb base = kKillerCol;
        if (k.kind() == game::KillerAI::Kind::Whisper) {
            base = Rgb{90, 160, 255};
        } else if (k.kind() == game::KillerAI::Kind::Warden) {
            base = Rgb{80, 220, 200};
        } else if (k.kind() == game::KillerAI::Kind::Butcher) {
            base = Rgb{200, 80, 220};
        } else if (k.isTemporaryKiller()) {
            base = Rgb{255, 120, 60};
        }
        float r = (k.kind() == game::KillerAI::Kind::Butcher) ? 0.6f : 0.45f;
        Vector3 a{static_cast<float>(k.position().x), 0.5f, static_cast<float>(k.position().z)};
        Vector3 b{static_cast<float>(k.position().x), 1.6f, static_cast<float>(k.position().z)};
        DrawCapsule(a, b, r, 8, 8, toColor(base));
    }

    EndMode3D();

    if (world.modeName() == "Blackout" && !world.over() && world.localSurvivor()) {
        Vector2 center{static_cast<float>(screenW) / 2.0f, static_cast<float>(screenH) / 2.0f};
        float radius = static_cast<float>(screenH) * 0.42f;
        DrawRectangle(0, 0, screenW, screenH, Color{0, 0, 0, 60});
        DrawRing(center, radius, radius + 2200.0f, 0.0f, 360.0f, segments, Color{0, 0, 0, 205});
    }
}

void Renderer::drawHudImpl(const game::World& world, const std::string& status, const UiFrame& ui) {
    DrawText("WASD move  SHIFT sprint  E repair / escape  ESC quit", 12, screenH - 28, 16, toColor(kTextCol));
    DrawText("1/2/3 use item   B buy MapCharm(100)   G buy GoldCharm(1000, once/match)", 12,
             screenH - 46, 14, toColor(kAlertCol));

    std::string head = status;
    DrawText(head.c_str(), 12, 10, 16, toColor(kTextCol));

    std::string kinfo = "t=" + std::to_string(static_cast<int>(world.clock())) + "s  map=" +
                        world.mapName() + "  killers=" +
                        std::to_string(static_cast<int>(world.killers().size()));
    int maxAnger = 0;
    for (const auto& k : world.killers()) {
        kinfo += " [" + k.typeName() + ":" + k.stateName() + " " + std::to_string(k.anger()) + "]";
        if (k.anger() > maxAnger) maxAnger = k.anger();
    }
    if (world.killers().empty()) kinfo += " [none]";
    text(kinfo.c_str(), 12, 32, 14, kTextCol.r, kTextCol.g, kTextCol.b);

    if (spectateId >= 0) {
        std::string sp = "SPECTATING Survivor#" + std::to_string(spectateId) + "   [ / ] switch";
        int sw = MeasureText(sp.c_str(), 14);
        text(sp.c_str(), (screenW - sw) / 2, 10, 14, kAlertCol.r, kAlertCol.g, kAlertCol.b);
    }

    std::string gpuInfo = "GPU:" + vendor + " preset=" + preset + " seg=" + std::to_string(segments) +
                          (msaaOn ? " MSAA4x" : " MSAA off");
    int gw = MeasureText(gpuInfo.c_str(), 10);
    DrawText(gpuInfo.c_str(), screenW - gw - 12, screenH - 18, 10, toColor(Rgb{150, 150, 160}));

    float bx = 12.0f;
    float by = static_cast<float>(screenH - 60);
    float bw = static_cast<float>(screenW * 0.3f);
    text(core::Loc::instance().t("hud.anger").c_str(), static_cast<int>(bx), static_cast<int>(by - 16), 12,
         kTextCol.r, kTextCol.g, kTextCol.b);
    DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bw), 8, toColor(kBg));
    DrawRectangle(static_cast<int>(bx), static_cast<int>(by),
                  static_cast<int>(bw * static_cast<float>(clamp01(maxAnger / 100.0))), 8,
                  toColor(kKillerCol));

    if (const game::Survivor* lp = world.localSurvivor()) {
        float ubw = static_cast<float>(screenW * 0.22f);
        float ubx = 12.0f;
        float uby = static_cast<float>(screenH - 98);
        float hpF = static_cast<float>(lp->health.hp()) / 100.0f;
        Rgb hpCol = hpF > 0.6f ? Rgb{80, 230, 120} : (hpF > 0.3f ? Rgb{255, 190, 60} : Rgb{230, 60, 60});
        text(core::Loc::instance().t("hud.hp").c_str(), static_cast<int>(ubx), static_cast<int>(uby - 16), 12,
             kTextCol.r, kTextCol.g, kTextCol.b);
        DrawRectangle(static_cast<int>(ubx), static_cast<int>(uby), static_cast<int>(ubw), 6, toColor(kBg));
        if (hpF > 0.0f) {
            DrawRectangle(static_cast<int>(ubx), static_cast<int>(uby),
                          static_cast<int>(ubw * hpF), 6, toColor(hpCol));
        }
        if (lp->downed) {
            std::string dtxt = core::Loc::instance().t("hud.downed") + " - " + core::Loc::instance().t("hud.rescueIn") +
                               ": " + std::to_string(std::max(0, static_cast<int>(lp->downTimer))) + "s";
            text(dtxt.c_str(), static_cast<int>(ubx + ubw + 10), static_cast<int>(uby - 8), 12, 230,
                 60, 60);
        } else {
            float stF = static_cast<float>(lp->stamina) / 100.0f;
            text(core::Loc::instance().t("hud.stamina").c_str(), static_cast<int>(ubx), static_cast<int>(uby + 10),
                 12, kTextCol.r, kTextCol.g, kTextCol.b);
            DrawRectangle(static_cast<int>(ubx), static_cast<int>(uby + 26), static_cast<int>(ubw), 6, toColor(kBg));
            DrawRectangle(static_cast<int>(ubx), static_cast<int>(uby + 26),
                          static_cast<int>(ubw * stF), 6, toColor(kLocalCol));
            if (!lp->effect.empty()) {
                std::string buff = core::Loc::instance().t("hud.buff") + ": " + lp->effect + " " +
                                   std::to_string(std::max(0, static_cast<int>(lp->effectTimer))) + "s";
                text(buff.c_str(), static_cast<int>(ubx + ubw + 10), static_cast<int>(uby + 10), 12,
                     kAlertCol.r, kAlertCol.g, kAlertCol.b);
            }
        }
        if (!lp->items.empty() || lp->coins > 0) {
            std::string inv = core::Loc::instance().t("hud.coins") + ": " + std::to_string(lp->coins) + "  |  " +
                              core::Loc::instance().t("hud.inventory") + ":";
            for (size_t i = 0; i < lp->items.size(); ++i) {
                inv += "  [" + std::to_string(i + 1) + "]" + lp->items[i].name;
            }
            int tw = MeasureText(inv.c_str(), 14);
            text(inv.c_str(), screenW - tw - 12, 10, 14, kTextCol.r, kTextCol.g, kTextCol.b);
        }
    }

    drawOverlay(ui, screenW, screenH, vendor, preset, segments, *this);
}

void Renderer::present(const game::World& world, const std::string& status, const UiFrame& ui) {
    if (!ready) return;

    float frameDt = GetFrameTime();
    if (shake > 0.001f) {
        static unsigned long long shakeState = 0x123456789ABCDEFULL;
        shakeState = shakeState * 6364136223846793005ULL + 1442695040888963407ULL;
        float rx = static_cast<float>((shakeState >> 33) % 2000) / 1000.0f - 1.0f;
        shakeState = shakeState * 6364136223846793005ULL + 1442695040888963407ULL;
        float ry = static_cast<float>((shakeState >> 33) % 2000) / 1000.0f - 1.0f;
        float amp = shake * 7.0f;
        shakeX = rx * amp;
        shakeY = ry * amp;
        shake *= std::exp(-6.0f * frameDt);
    } else {
        shakeX = 0.0f;
        shakeY = 0.0f;
        shake = 0.0f;
    }

    for (auto& fx : fxs) {
        fx.life -= frameDt * 1.2;
    }
    fxs.erase(std::remove_if(fxs.begin(), fxs.end(),
                             [](const RenderFx& fx) { return fx.life <= 0.0; }),
              fxs.end());

    if (vhsOn && sceneTarget) {
        RenderTexture2D* rt = static_cast<RenderTexture2D*>(sceneTarget);
        BeginTextureMode(*rt);
        ClearBackground(toColor(kBg));
        drawSceneImpl(world);
        EndTextureMode();

        BeginDrawing();
        ClearBackground(toColor(kBg));
        drawVhsFx(*rt, screenW, screenH, shakeX, shakeY);
        drawHudImpl(world, status, ui);
        EndDrawing();
    } else {
        BeginDrawing();
        ClearBackground(toColor(kBg));
        Camera2D cam{};
        cam.offset = Vector2{shakeX, shakeY};
        BeginMode2D(cam);
        drawSceneImpl(world);
        EndMode2D();
        drawHudImpl(world, status, ui);
        EndDrawing();
    }
}
#else
void Renderer::drawSceneImpl(const game::World& world) {
    (void)world;
}

void Renderer::drawHudImpl(const game::World& world, const std::string& status, const UiFrame& ui) {
    (void)world;
    (void)status;
    (void)ui;
}

void Renderer::present(const game::World& world, const std::string& status, const UiFrame& ui) {
    (void)world;
    (void)status;
    (void)ui;
}
#endif
