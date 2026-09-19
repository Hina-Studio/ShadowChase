#pragma once

namespace game {
struct Balance {
    double moveSpeed = 3.5;
    double sprintSpeed = 5.8;
    double staminaDrain = 14.0;
    double staminaRegen = 7.0;
    double repairTime = 6.0;
    double downWindow = 60.0;
    double damageScale = 1.0;
    double angerGainScale = 1.0;
    int generatorFuel = 6;
};
}
