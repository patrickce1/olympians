// Enemy.cpp
#include "Enemy.h"
#include <algorithm>
#include <cugl/cugl.h>

using namespace cugl;

bool Enemy::init(const std::string& enemyId, const std::string& jsonPath) {
    static EnemyLoader sLoader;
    static bool sLoaded = false;
    static std::string sLoadedPath;

    if (!sLoaded) {
        if (!sLoader.loadFromFile(jsonPath)) {
            CULog("Failed to load enemy JSON: %s", jsonPath.c_str());
            return false;
        }
        sLoaded = true;
        sLoadedPath = jsonPath;
    }

    if (sLoadedPath != jsonPath) {
        CULog("Enemy JSON already loaded from different path");
        return false;
    }

    if (!sLoader.has(enemyId)) {
        CULog("Enemy ID not found: %s", enemyId.c_str());
        return false;
    }

    const EnemyLoader::EnemyDef& def = sLoader.get(enemyId);

    _enemyId = def.id;
    _name = def.name;
    _spritesheetPath = def.spritesheetPath;

    _maxHealth = def.maxHealth;
    _currentHealth = def.maxHealth;

    _states = def.states;

    if (_states.count("idle") == 0) {
        CULog("Enemy '%s' missing idle state", enemyId.c_str());
        return false;
    }

    enterState("idle");
    _attackLockout = 0.0f;

    return true;
}

const EnemyLoader::StateDef* Enemy::getCurrentStateDef() const {
    auto it = _states.find(_currentState);
    if (it == _states.end()) return nullptr;
    return &it->second;
}

bool Enemy::requestState(const std::string& stateName) {
    if (_states.count(stateName) == 0) return false;

    // While lockout is active, only allow idle.
    if (_attackLockout > 0.0f && stateName != "idle") return false;

    enterState(stateName);
    return true;
}

void Enemy::enterState(const std::string& stateName) {
    _currentState = stateName;
    _stateTime = 0.0f;
    _eventsFiredThisState = false;
}

void Enemy::tick(float dt) {
    if (dt <= 0.0f) return;
    _stateTime += dt;
    _attackLockout = clampMinZero(_attackLockout - dt);
}

bool Enemy::readyToFire() const {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return false;
    if (_eventsFiredThisState) return false;
    return _stateTime >= st->buildUpTime;
}

void Enemy::fireEvents() {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return;

    for (const auto& ev : st->events) {
        FiredEvent fe;
        fe.def = ev;
        fe.stateName = st->name;
        _firedEvents.push_back(fe);
    }

    _eventsFiredThisState = true;
}

void Enemy::applyCooldown() {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return;
    _attackLockout = std::max(_attackLockout, st->cooldownTime);
}

std::string Enemy::getNextStateOrIdle() const {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return "idle";

    if (!st->nextState.empty() && _states.count(st->nextState) > 0) {
        return st->nextState;
    }
    return "idle";
}

void Enemy::update(float dt) {
    tick(dt);

    if (readyToFire()) {
        fireEvents();
        applyCooldown();
        enterState(getNextStateOrIdle());
    }
}

std::vector<Enemy::FiredEvent> Enemy::takeFiredEvents() {
    std::vector<FiredEvent> out;
    out.swap(_firedEvents);
    return out;
}

void Enemy::updateHealth(float delta) {
    _currentHealth += delta;
    if (_currentHealth > _maxHealth) _currentHealth = _maxHealth;
    if (_currentHealth < 0.0f) _currentHealth = 0.0f;
}
