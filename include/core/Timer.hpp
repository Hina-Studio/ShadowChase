#pragma once
#include <chrono>

namespace core {
class Timer {
public:
    Timer();
    void tick();
    double deltaSeconds() const;
    double elapsedSeconds() const;
    int fps() const;
private:
    std::chrono::steady_clock::time_point last;
    std::chrono::steady_clock::time_point start;
    double dt;
    int frameCount;
    double fpsTimer;
    int fpsValue;
};
}
