#pragma once
#include <cstdint>

namespace core {
class Random {
public:
    static Random& instance();
    void seed(std::uint64_t s);
    std::uint64_t next();
    int range(int min, int maxExclusive);
private:
    Random();
    std::uint64_t state;
};
}
