#pragma once
#include <string>
#include <vector>

namespace sc {
constexpr double kCellSize = 2.0;
constexpr double kWallHeight = 3.0;
constexpr double kGrassSpeedMul = 0.6;
constexpr double kGrassExposureMul = 0.5;
constexpr double kTrapDamage = 10.0;
constexpr double kTrapRootTime = 1.5;
constexpr double kTrapTriggerRange = 0.8;
constexpr int kGeneratorCandidates = 12;
constexpr int kActiveGenerators = 2;
constexpr int kExitCandidates = 4;
constexpr int kExitActive = 1;
constexpr int kSpawnCandidates = 3;

inline const std::vector<std::string>& maloneFarmMap() {
    static const std::vector<std::string> rows = {
        "#################E##################",
        "#S.................................#",
        "#.####....#D##....####.............#",
        "#.#G.#....#G.#....#G.#.............#",
        "#.#.L#....#.L#....#.L#.............#",
        "#.#D##....####....#D##.............#",
        "#............................~~~~~.#",
        "#.....G.....T...........G....~~~~~.#",
        "#.........L...............L..~~~~~.#",
        "#............###D####..............#",
        "#...WW..G....#......#.......T......#",
        "#..LWW.......#.G....#..............#",
        "E............D......D..............E",
        "#............#..L.G.#..............#",
        "#............#......#..............#",
        "#............########..............#",
        "#.......G.....T.....L.......G......#",
        "#...........L.................L....#",
        "#..######...............#D##.......#",
        "#..#....#.~~~...........#G.#.......#",
        "#..#.G..#.~~~...T.......#.L#.......#",
        "#..#..L.#.~~~...........####.......#",
        "#..##D###.~~~......................#",
        "#..................................#",
        "#S................................S#",
        "##########E#########################",
    };
    return rows;
}
}
