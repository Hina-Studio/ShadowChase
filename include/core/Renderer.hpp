#pragma once
#include <string>
#include <vector>

namespace game {
class World;
}

struct RendererSettings {
    int width = 1280;
    int height = 720;
    std::string title = "SlashCo";
    std::string gpuPreset = "auto";
    int msaa = -1;
    int maxFPS = -1;
    int grid = -1;
    int vhs = -1;
};

struct UiFrame {
    std::string mode = "none";
    std::string title;
    std::string subtitle;
    std::vector<std::string> lines;
    std::vector<std::string> options;
    int selected = -1;
};

struct RenderFx {
    double x = 0.0;
    double z = 0.0;
    double life = 1.0;
    std::string text;
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
};

class Renderer {
public:
    Renderer();
    ~Renderer();
    void init(const RendererSettings& settings);
    void shutdown();
    bool shouldClose() const;
    void present(const game::World& world, const std::string& status, const UiFrame& ui);
    void setVhs(bool on);
    void setGrid(bool on);
    void resize(int width, int height);
    void addShake(float strength);
    void addHitFx(double x, double z, const std::string& text, unsigned char r, unsigned char g,
                  unsigned char b);
    void setTutorialMarker(bool active, double x, double z);
    void setSpectateId(int id) { spectateId = id; }
    void text(const char* s, int x, int y, int size, unsigned char r, unsigned char g,
              unsigned char b, unsigned char a = 255);
    bool hasUnicodeFont() const { return unicodeFont; }
    bool vhsEnabled() const { return vhsOn; }
    bool gridEnabled() const { return drawGrid; }

    const std::string& gpuVendor() const { return vendor; }
    const std::string& presetName() const { return preset; }

private:
    int screenW;
    int screenH;
    bool ready;

    std::string vendor = "unknown";
    std::string preset = "balanced";
    int segments = 24;
    int maxFPS = 60;
    bool msaaOn = false;
    bool drawGrid = true;
    bool vsync = true;
    bool vhsOn = true;
    void* sceneTarget = nullptr;
    std::vector<RenderFx> fxs;
    float shake = 0.0f;
    float shakeX = 0.0f;
    float shakeY = 0.0f;
    bool tutorActive = false;
    double tutorX = 0.0;
    double tutorZ = 0.0;
    void* uiFont = nullptr;
    bool unicodeFont = false;
    int spectateId = -1;

    void detectVendor();
    void drawSceneImpl(const game::World& world);
    void drawHudImpl(const game::World& world, const std::string& status, const UiFrame& ui);
};
