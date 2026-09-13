#include "core/Timer.hpp"

namespace core {
Timer::Timer()
    : last(std::chrono::steady_clock::now()),
      start(last),
      dt(0.0),
      frameCount(0),
      fpsTimer(0.0),
      fpsValue(0) {}

void Timer::tick() {
    auto now = std::chrono::steady_clock::now();
    dt = std::chrono::duration<double>(now - last).count();
    last = now;
    ++frameCount;
    fpsTimer += dt;
    if (fpsTimer >= 1.0) {
        fpsValue = frameCount;
        frameCount = 0;
        fpsTimer -= 1.0;
    }
}

double Timer::deltaSeconds() const {
    return dt;
}

double Timer::elapsedSeconds() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

int Timer::fps() const {
    return fpsValue;
}
}
