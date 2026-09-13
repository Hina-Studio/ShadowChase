#include "core/Audio.hpp"

#include <algorithm>

#include "core/Logger.hpp"

#ifdef SLASHCO_RAYLIB
#include <raylib.h>

#include <cmath>
#endif

#ifdef SLASHCO_RAYLIB
namespace {
Sound makeTone(float freq, float seconds, float amp, bool square) {
    const int sr = 44100;
    int n = static_cast<int>(sr * seconds);
    Wave w{};
    w.frameCount = static_cast<unsigned int>(n);
    w.sampleRate = sr;
    w.sampleSize = 16;
    w.channels = 1;
    short* data = static_cast<short*>(MemAlloc(static_cast<unsigned int>(n) * sizeof(short)));
    for (int i = 0; i < n; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(sr);
        float env = 1.0f - static_cast<float>(i) / static_cast<float>(n);
        float s = std::sin(6.2831853f * freq * t);
        if (square) s = s >= 0.0f ? 1.0f : -1.0f;
        data[i] = static_cast<short>(s * env * amp * 32767.0f);
    }
    w.data = data;
    Sound snd = LoadSoundFromWave(w);
    MemFree(data);
    return snd;
}
}
#endif

void Audio::init(bool enabled, float volume) {
    on = enabled;
    vol = std::max(0.0f, std::min(1.0f, volume));

#ifdef SLASHCO_RAYLIB
    if (!on) return;
    InitAudioDevice();

    Sound* b = new Sound(makeTone(880.0f, 0.12f, 0.5f, false));
    Sound* t = new Sound(makeTone(140.0f, 0.22f, 0.7f, true));
    Sound* a = new Sound(makeTone(520.0f, 0.5f, 0.5f, false));
    Sound* c = new Sound(makeTone(660.0f, 0.25f, 0.5f, false));
    Sound* k = new Sound(makeTone(1320.0f, 0.10f, 0.4f, false));
    beep = b;
    thud = t;
    alarm = a;
    chime = c;
    coin = k;
    SetSoundVolume(*b, vol * 0.8f);
    SetSoundVolume(*t, vol);
    SetSoundVolume(*a, vol * 0.9f);
    SetSoundVolume(*c, vol * 0.9f);
    SetSoundVolume(*k, vol * 0.7f);
    core::Logger::info("Audio initialized (procedural tones)");
#endif
}

void Audio::setVolume(float volume) {
    vol = std::max(0.0f, std::min(1.0f, volume));
#ifdef SLASHCO_RAYLIB
    if (!on) return;
    if (beep) SetSoundVolume(*static_cast<Sound*>(beep), vol * 0.8f);
    if (thud) SetSoundVolume(*static_cast<Sound*>(thud), vol);
    if (alarm) SetSoundVolume(*static_cast<Sound*>(alarm), vol * 0.9f);
    if (chime) SetSoundVolume(*static_cast<Sound*>(chime), vol * 0.9f);
    if (coin) SetSoundVolume(*static_cast<Sound*>(coin), vol * 0.7f);
#endif
}

void Audio::shutdown() {
#ifdef SLASHCO_RAYLIB
    if (on) {
        if (beep) { UnloadSound(*static_cast<Sound*>(beep)); delete static_cast<Sound*>(beep); beep = nullptr; }
        if (thud) { UnloadSound(*static_cast<Sound*>(thud)); delete static_cast<Sound*>(thud); thud = nullptr; }
        if (alarm) { UnloadSound(*static_cast<Sound*>(alarm)); delete static_cast<Sound*>(alarm); alarm = nullptr; }
        if (chime) { UnloadSound(*static_cast<Sound*>(chime)); delete static_cast<Sound*>(chime); chime = nullptr; }
        if (coin) { UnloadSound(*static_cast<Sound*>(coin)); delete static_cast<Sound*>(coin); coin = nullptr; }
        CloseAudioDevice();
    }
#endif
    on = false;
}

void Audio::playEvent(const std::string& event) {
#ifdef SLASHCO_RAYLIB
    if (!on) return;
    auto has = [&](const char* s) { return event.find(s) != std::string::npos; };
    if (has("bought") || has("ui")) {
        if (coin) PlaySound(*static_cast<Sound*>(coin));
    } else if (has("activated") || has("escaped") || has("Doors opened") || has("outcome")) {
        if (chime) PlaySound(*static_cast<Sound*>(chime));
    } else if (has("eliminated") || has("died while") || has("went down")) {
        if (alarm) PlaySound(*static_cast<Sound*>(alarm));
    } else if (has("hit") || has("blocked") || has("heard noise") || has("rescue")) {
        if (thud) PlaySound(*static_cast<Sound*>(thud));
    } else if (has("ate") || has("used item")) {
        if (beep) PlaySound(*static_cast<Sound*>(beep));
    }
#else
    (void)event;
#endif
}
