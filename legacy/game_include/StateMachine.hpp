#pragma once
#include <functional>
#include <string>
#include <unordered_map>

namespace game {
class StateMachine {
public:
    using Handler = std::function<void(double)>;

    void add(const std::string& name, Handler onEnter, Handler onUpdate, Handler onExit);
    void change(const std::string& name);
    void update(double dt);
    const std::string& current() const;
private:
    struct State {
        Handler enter;
        Handler update;
        Handler exit;
    };
    std::unordered_map<std::string, State> states;
    std::string currentState;
    std::string nextState;
    void applyChange();
};
}
