#pragma once
#include <string>

class Audio {
public:
    void init(bool enabled, float volume);
    void shutdown();
    void setVolume(float volume);
    bool enabled() const { return on; }
    void playEvent(const std::string& event);

private:
    bool on = false;
    float vol = 0.6f;
    void* beep = nullptr;
    void* thud = nullptr;
    void* alarm = nullptr;
    void* chime = nullptr;
    void* coin = nullptr;
};
