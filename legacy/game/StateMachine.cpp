#include "game/StateMachine.hpp"

namespace game {
void StateMachine::add(const std::string& name, Handler onEnter, Handler onUpdate, Handler onExit) {
    states[name] = State{std::move(onEnter), std::move(onUpdate), std::move(onExit)};
}

void StateMachine::applyChange() {
    if (nextState.empty() || nextState == currentState) {
        nextState.clear();
        return;
    }
    auto it = states.find(currentState);
    if (it != states.end() && it->second.exit) {
        it->second.exit(0.0);
    }
    currentState = nextState;
    nextState.clear();
    it = states.find(currentState);
    if (it != states.end() && it->second.enter) {
        it->second.enter(0.0);
    }
}

void StateMachine::change(const std::string& name) {
    if (states.count(name) != 0) {
        nextState = name;
        applyChange();
    }
}

void StateMachine::update(double dt) {
    auto it = states.find(currentState);
    if (it != states.end() && it->second.update) {
        it->second.update(dt);
    }
}

const std::string& StateMachine::current() const {
    return currentState;
}
}
