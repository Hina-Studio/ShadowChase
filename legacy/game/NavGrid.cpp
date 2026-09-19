#include "game/NavGrid.hpp"

#include <algorithm>
#include <cmath>
#include <queue>

namespace game {
namespace {
constexpr int kHalf = (NavGrid::Size - 1) / 2;
}

void NavGrid::setBlocked(int gx, int gz) {
    if (inBounds(gx, gz)) cells[gz * Size + gx] = true;
}

void NavGrid::clear() {
    std::fill(cells, cells + Size * Size, false);
}

void NavGrid::addBlock(int gx, int gz) {
    setBlocked(gx, gz);
}

void NavGrid::addRect(int x0, int z0, int x1, int z1) {
    if (x0 > x1) std::swap(x0, x1);
    if (z0 > z1) std::swap(z0, z1);
    for (int x = x0; x <= x1; ++x) {
        for (int z = z0; z <= z1; ++z) {
            setBlocked(x, z);
        }
    }
}

bool NavGrid::inBounds(int gx, int gz) const {
    return gx >= 0 && gx < Size && gz >= 0 && gz < Size;
}

bool NavGrid::blocked(int gx, int gz) const {
    return inBounds(gx, gz) && cells[gz * Size + gx];
}

void NavGrid::toTile(const Vec3& p, int& gx, int& gz) const {
    int x = static_cast<int>(std::round(p.x + kHalf));
    int z = static_cast<int>(std::round(p.z + kHalf));
    gx = std::max(0, std::min(Size - 1, x));
    gz = std::max(0, std::min(Size - 1, z));
}

Vec3 NavGrid::tileCenter(int gx, int gz) const {
    return Vec3{static_cast<double>(gx - kHalf), 0.0, static_cast<double>(gz - kHalf)};
}

bool NavGrid::blockedAt(const Vec3& p) const {
    int gx, gz;
    toTile(p, gx, gz);
    return blocked(gx, gz);
}

void NavGrid::rebuild() {
    std::fill(cells, cells + Size * Size, false);
    for (int sx = -1; sx <= 1; ++sx) {
        for (int sz = -1; sz <= 1; ++sz) {
            if (sx == 0 || sz == 0) continue;
            int signX = sx > 0 ? 1 : -1;
            int signZ = sz > 0 ? 1 : -1;
            setBlocked(signX * 2, signZ * 2);
            setBlocked(signX * 3, signZ * 2);
            setBlocked(signX * 2, signZ * 3);
            setBlocked(signX * 3, signZ * 3);
        }
    }
}

std::vector<Vec3> NavGrid::findPath(const Vec3& from, const Vec3& to, int maxNodes) const {
    std::vector<Vec3> out;
    int sx, sz, tx, tz;
    toTile(from, sx, sz);
    toTile(to, tx, tz);
    if (blocked(tx, tz) || (sx == tx && sz == tz)) {
        if (sx == tx && sz == tz) {
            out.push_back(to);
        }
        return out;
    }

    const int W = Size;
    std::vector<int> came(W * W, -1);
    std::queue<int> q;
    int startId = sz * W + sx;
    came[startId] = startId;
    q.push(startId);
    int goalId = tz * W + tx;

    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    bool found = false;
    while (!q.empty() && !found) {
        int cur = q.front();
        q.pop();
        int cx = cur % W;
        int cz = cur / W;
        for (const auto& d : dirs) {
            int nx = cx + d[0];
            int nz = cz + d[1];
            if (!inBounds(nx, nz) || blocked(nx, nz)) continue;
            int nid = nz * W + nx;
            if (came[nid] != -1) continue;
            came[nid] = cur;
            if (nid == goalId) {
                found = true;
                break;
            }
            q.push(nid);
        }
    }
    if (!found) return out;

    std::vector<int> rev;
    for (int id = goalId; id != startId; id = came[id]) {
        rev.push_back(id);
    }
    std::reverse(rev.begin(), rev.end());
    if (maxNodes > 0 && static_cast<int>(rev.size()) > maxNodes) {
        rev.resize(maxNodes);
    }
    for (int id : rev) {
        int gx = id % W;
        int gz = id / W;
        out.push_back(tileCenter(gx, gz));
    }
    return out;
}

Vec3 NavGrid::slideMove(const Vec3& pos, const Vec3& delta) const {
    Vec3 next = pos;
    Vec3 alongX{next.x + delta.x, 0.0, next.z};
    if (!blockedAt(alongX)) {
        next.x = alongX.x;
    }
    Vec3 alongZ{next.x, 0.0, next.z + delta.z};
    if (!blockedAt(alongZ)) {
        next.z = alongZ.z;
    }
    return next;
}
}
