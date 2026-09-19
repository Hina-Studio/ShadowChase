#pragma once
#include <string>

namespace game {
class Health {
public:
    static constexpr int MaxHP = 100;

    void setHp(int v);
    void damage(int amount);
    void heal(int amount);

    int hp() const { return hp_; }
    bool isDowned() const { return hp_ <= 0; }
    std::string band() const;
private:
    int hp_ = MaxHP;
};
}
