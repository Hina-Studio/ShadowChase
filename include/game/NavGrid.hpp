#pragma once
#include <vector>

#include "Math.hpp"

namespace game {
class NavGrid {
public:
    static constexpr int Size = 23;

    void rebuild();
    void clear();
    void addBlock(int gx, int gz);
    void addRect(int x0, int z0, int x1, int z1);
    bool inBounds(int gx, int gz) const;
    bool blocked(int gx, int gz) const;
    bool blockedAt(const Vec3& p) const;
    void toTile(const Vec3& p, int& gx, int& gz) const;
    Vec3 tileCenter(int gx, int gz) const;
    std::vector<Vec3> findPath(const Vec3& from, const Vec3& to, int maxNodes = 128) const;
    Vec3 slideMove(const Vec3& pos, const Vec3& delta) const;

private:
    bool cells[Size * Size] = {};

    void setBlocked(int gx, int gz);
};
}
