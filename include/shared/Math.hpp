#pragma once
#include <cmath>

namespace sc {
struct Vec2 {
    double x = 0.0;
    double z = 0.0;

    Vec2() = default;
    Vec2(double px, double pz) : x(px), z(pz) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, z + o.z}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, z - o.z}; }
    Vec2 operator*(double s) const { return {x * s, z * s}; }

    double length() const { return std::sqrt(x * x + z * z); }
    double distance(const Vec2& o) const { return (*this - o).length(); }

    Vec2 normalized() const {
        double len = length();
        if (len < 1e-9) return {0.0, 0.0};
        return {x / len, z / len};
    }
};
}
