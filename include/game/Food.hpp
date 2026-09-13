#pragma once
#include <string>
#include <vector>

namespace game {
struct FoodItem {
    std::string name;
    int heal = 0;
    std::string effect;
    double duration = 0.0;
    int rarity = 1;
    int stamina = 0;
};

inline FoodItem cannedMeal() {
    return {"CannedMeal", 20, "", 0.0, 1};
}

inline FoodItem cocoa() {
    return {"Cocoa", 15, "", 0.0, 1};
}

inline FoodItem chickenHead() {
    return {"ChickenHead", 45, "", 0.0, 2};
}

inline FoodItem candy() {
    return {"Candy", 0, "Shield", 120.0, 2};
}

inline FoodItem coldWater() {
    return {"ColdWater", 0, "Speed", 200.0, 2};
}

inline FoodItem emergencyRation() {
    return {"EmergencyRation", 60, "", 0.0, 3};
}

inline FoodItem bloodBag() {
    return {"BloodBag", 0, "Reveal", 8.0, 3};
}

inline FoodItem breadWorm() {
    return {"BreadWorm", 0, "NightVision", 5.0, 2};
}

inline FoodItem stimulant() {
    return {"Stimulant", 0, "Speed", 120.0, 4};
}

inline FoodItem energyBar() {
    return {"EnergyBar", 10, "", 0.0, 1, 40};
}

inline FoodItem painkillers() {
    return {"Painkillers", 30, "Shield", 20.0, 3};
}

inline FoodItem energyDrink() {
    return {"EnergyDrink", 0, "Speed", 20.0, 2, 60};
}

inline FoodItem bandage() {
    return {"Bandage", 25, "", 0.0, 1};
}

inline FoodItem adrenaline() {
    return {"Adrenaline", 0, "Speed", 8.0, 3, 100};
}

inline FoodItem mapCharm() {
    return {"MapCharm", 0, "SaveLife", 0.0, 4};
}

inline FoodItem goldCharm() {
    return {"GoldCharm", 0, "SaveLifeGold", 0.0, 5};
}

inline bool isMapCharm(const FoodItem& f) {
    return f.effect == "SaveLife" || f.effect == "SaveLifeGold";
}

inline bool isGoldCharm(const FoodItem& f) {
    return f.effect == "SaveLifeGold";
}

inline std::vector<FoodItem> startingKit() {
    return {cannedMeal(), energyBar()};
}
}
