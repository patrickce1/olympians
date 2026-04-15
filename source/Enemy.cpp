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
    _spritesheetPath = def.spritesheetPath;
    _maxHealth = def.maxHealth;
    _currentHealth = def.maxHealth;
    _states = def.states;
    _customData = def.customData;
    
    for (int i = 0; i < NUM_PLAYERS; i++) {
        _sideMultipliers[i] = 1.0f;
    }

    if (_states.count(EnemyLoader::State::IDLE) == 0) {
        CULog("Enemy '%s' missing idle state", enemyId.c_str());
        return false;
    }
    enterState(EnemyLoader::State::IDLE);

    _attackLockout = 0.0f;
    _retargetLikelihood = def.ai.retargetLikelihood;
    _defenseLikelihood = def.ai.defenseLikelihood;
    
    return true;
}

/** Returns the current state that the enemy is in. */
const EnemyLoader::StateDef* Enemy::getCurrentStateDef() const {
    auto cur = _states.find(_currentState);
    if (cur == _states.end()) return nullptr;
    return &cur->second;
}

/** Returns true if successfully enters requested state. False and idle otherwise. */
bool Enemy::requestState(EnemyLoader::State state) {
    if (_states.count(state) == 0) return false;    // State doesn't exist
    if (_attackLockout > 0.0f && state != EnemyLoader::State::IDLE) return false; // Lockout is active, only allow idle

    enterState(state);
    return true;
}

/** Sets the enemy retarget likelihood. Only used for testing, should normally be defined in enemies JSON. */
void Enemy::setRetargetLikelihood(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    _retargetLikelihood = v;
}

/** Immediately enters the state and resets timers. */
void Enemy::enterState(EnemyLoader::State state) {
    _currentState = state;
    _stateTime = 0.0f;
    _eventsFiredThisState = false;
}

/** Updates timers.*/
void Enemy::tick(float dt) {
    if (dt <= 0.0f) return;
    _stateTime += dt;
    _attackLockout = (_attackLockout - dt < 0.0f) ? 0.0f : _attackLockout - dt;
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
        fe.state = st->state;
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
EnemyLoader::State Enemy::getNextStateOrIdle() const {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return EnemyLoader::State::IDLE;

    if (_states.count(st->nextState) > 0) {
        return st->nextState;
    }

    return EnemyLoader::State::IDLE;
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

/* Handles taking damage and applying the side modifiers
 * Use this method instead of updateHealth() for appropriate damage multiplication
 * @param damage is the amount of damage being done to the boss
 * @param playerIndex is the index that was assigned to the player by the host
*/
void Enemy::takeDamage(float damage, int playerIndex) {
    //get relative index based on which side of the boss the player is on
    int relativeIndex = (playerIndex - _targetIndex + NUM_PLAYERS) % NUM_PLAYERS;

    float multiplier = 1.0f;
    if (relativeIndex < _sideMultipliers.size()) {
        multiplier = _sideMultipliers[relativeIndex];
    }

    // Debug logging for damage calculation
    if (_debug) {
        CULog(
            "[Enemy]: Damage Calculation. State %s | PlayerIndex: %d | TargetIndex: %d | RelativeIndex: %d | "
            "BaseDamage: %f | Multiplier: %f | FinalDamage: %f",
            _states.at(_currentState).name.c_str(),
            playerIndex,
            _targetIndex,
            relativeIndex,
            damage,
            multiplier,
            damage * multiplier
        );
    }

    updateHealth(-(damage * multiplier));
}

/** Lets you change the multipler value on the side equal to relativeIndex
 * @param relativeIndex is the side we want to change the multiplier for. 0 is the direction the boss is facing
 * @param multiplier the damage multiplier we want to apply to relativeIndex
 */
void Enemy::setSideMultiplier(int relativeIndex, float multiplier) {
    _sideMultipliers[relativeIndex] = multiplier;
}

/** Returns the multiplier data for the given absolute side index.
 * @param absoluteIndex is the side we want to get. Index 0 corresponds to the side facing the host, regardless of the boss' direction.
 */
float Enemy::getSideMultiplier(int absoluteIndex) {
    int relativeIndex = (absoluteIndex - _targetIndex + NUM_PLAYERS) % NUM_PLAYERS;
    if (relativeIndex < _sideMultipliers.size()) {
        return _sideMultipliers[relativeIndex];
    }
    return 1.0f;
}

/** Checks if this enemy should use their defensive move
This can and should be overwritten for each boss to have custom logic on when they decide to use their defensive move */
bool Enemy::shouldDefend() {
    return false;
}

/** If the boss is currently in cooldown, it skips the cooldown */
void Enemy::skipCooldown() {
    _attackLockout = 0.0;
}