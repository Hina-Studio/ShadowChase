#include "core/Random.hpp"

namespace core {
Random::Random() : state(0x9E3779B97F4A7C15ULL) {}

Random& Random::instance() {
    static Random inst;
    return inst;
}

void Random::seed(std::uint64_t s) {
    state = s != 0 ? s : 0x9E3779B97F4A7C15ULL;
}

std::uint64_t Random::next() {
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

int Random::range(int min, int maxExclusive) {
    if (maxExclusive <= min) return min;
    return static_cast<int>(min + next() % static_cast<std::uint64_t>(maxExclusive - min));
}
}
