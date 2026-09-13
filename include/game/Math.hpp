#pragma once
#include <cmath>

namespace game {
struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec3() = default;
    Vec3(double px, double py, double pz) : x(px), y(py), z(pz) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }

    double length() const { return std::sqrt(x * x + y * y + z * z); }
    double distance(const Vec3& o) const { return (*this - o).length(); }
    Vec3 normalized() const {
        double len = length();
        if (len < 1e-9) return {0.0, 0.0, 0.0};
        return {x / len, y / len, z / len};
    }
};
}
