// Enemy.cpp
#include "Enemy.h"
#include <algorithm>
#include <cugl/cugl.h>

using namespace cugl;

/** Returns true if the enemy initializes successfully. */
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
    _retargetLikelihood = def.ai.retargetLikelihood;
    
    
    return true;
}

/** Returns the current state that the enemy is in. */
const EnemyLoader::StateDef* Enemy::getCurrentStateDef() const {
    auto cur = _states.find(_currentState);
    if (cur == _states.end()) return nullptr;
    return &cur->second;
}

/** Returns true if successfully enters requested state. False and idle otherwise. */
bool Enemy::requestState(const std::string& stateName) {
    if (_states.count(stateName) == 0) return false;    // State doesn't exist
    if (_attackLockout > 0.0f && stateName != "idle") return false; // Lockout is active, only allow idle

    enterState(stateName);
    return true;
}

/** Immediately enters the state and resets timers. */
void Enemy::enterState(const std::string& stateName) {
    _currentState = stateName;
    _stateTime = 0.0f;
    _eventsFiredThisState = false;
}

/** Updates timers.*/
void Enemy::tick(float dt) {
    if (dt <= 0.0f) return;
    _stateTime += dt;
    _attackLockout = clampMinZero(_attackLockout - dt);
}

/** Returns true if buildUp time has passed and events have not yet fired in this state. */
bool Enemy::readyToFire() const {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return false;
    if (_eventsFiredThisState) return false;
    return _stateTime >= st->buildUpTime;
}

/** Fires events from this state, adding them to the events buffer. */
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

/** Sets the cooldown timer based on the current state of the enemy. */
void Enemy::applyCooldown() {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return;
    _attackLockout = std::max(_attackLockout, st->cooldownTime);
}

/** Returns the next state if defined by current state or "idle" by default. */
std::string Enemy::getNextStateOrIdle() const {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return "idle";

    if (!st->nextState.empty() && _states.count(st->nextState) > 0) {
        return st->nextState;
    }
    return "idle";
}

/** Main update loop for enemy. Handles firing events, applying cooldown, transition to next state. */
void Enemy::update(float dt) {
    tick(dt);

    if (readyToFire()) {
        fireEvents();
        applyCooldown();
        enterState(getNextStateOrIdle());
    }
}

/** Return contents of current event buffer and clears it.*/
std::vector<Enemy::FiredEvent> Enemy::takeFiredEvents() {
    std::vector<FiredEvent> out;
    out.swap(_firedEvents);
    return out;
}

/** Updates the enemy's health. Positive delta heals, negative damages. */
void Enemy::updateHealth(float delta) {
    _currentHealth += delta;
    if (_currentHealth > _maxHealth) _currentHealth = _maxHealth;
    if (_currentHealth < 0.0f) _currentHealth = 0.0f;
}
