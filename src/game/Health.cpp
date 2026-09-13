#include "game/Health.hpp"

#include <algorithm>

namespace game {
void Health::setHp(int v) {
    hp_ = std::max(0, std::min(MaxHP, v));
}

void Health::damage(int amount) {
    setHp(hp_ - amount);
}

void Health::heal(int amount) {
    setHp(hp_ + amount);
}

std::string Health::band() const {
    if (hp_ == MaxHP) return "Healthy";
    if (hp_ > 80) return "Scratched";
    if (hp_ > 60) return "Light";
    if (hp_ > 40) return "Moderate";
    if (hp_ > 20) return "Heavy";
    if (hp_ > 0) return "Dying";
    return "Downed";
}
}
