#include "Enemy.h"
#include <algorithm>

/** Clamps a float to be at least zero. */
static float clampMinZero(float v) { return v < 0.0f ? 0.0f : v; }

/** Constructs an Enemy instance from loader data and initializes runtime state. */
Enemy::Enemy(const std::string& enemyId, const EnemyLoader& loader) {
    CUAssertLog(loader.has(enemyId), "Enemy ID not found: %s", enemyId.c_str());
    const EnemyLoader::EnemyDef& def = loader.get(enemyId);

    _enemyId = def.id;
    _name = def.name;
    _spritesheetPath = def.spritesheetPath;

    _maxHealth = def.maxHealth;
    _currentHealth = def.maxHealth;

    _shields = def.defaultShields;
    _states = def.states;

    CUAssertLog(_states.count("idle") > 0, "Enemy '%s' must define 'idle'", _enemyId.c_str()); // assert there is an idle state

    // always start idle
    enterState("idle");
    _attackLockout = 0.0f;
    _facing = Facing::DOWN;
}

/** Returns the definition of the currently active state, or nullptr if missing. */
const EnemyLoader::StateDef* Enemy::getCurrentStateDef() const {
    auto stateDef = _states.find(_currentState);
    if (stateDef == _states.end()) return nullptr;
    return &stateDef->second;
}

/**
 * Attempts to enter a new state, respecting cooldown lockout rules.
 * Returns TRUE if successfully enters new state.
 */
bool Enemy::requestState(const std::string& stateName) {
    if (_states.count(stateName) == 0) return false;

    // While lockout is active, only allow idle.
    if (_attackLockout > 0.0f && stateName != "idle") return false;

    enterState(stateName);
    return true;
}

/** Immediately switches to the given state and resets runtime state timers. */
void Enemy::enterState(const std::string& stateName) {
    _currentState = stateName;
    _stateTime = 0.0f;
    _eventsFiredThisState = false;
}

/** Advances internal timers and decreases attack cooldown lockout. */
void Enemy::tick(float dt) {
    if (dt <= 0.0f) return;

    _stateTime += dt;
    _attackLockout = clampMinZero(_attackLockout - dt);
}

/** Returns TRUE if the current state has reached its build-up time and can fire events. */
bool Enemy::readyToFire() const {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return false;
    if (_eventsFiredThisState) return false;
    return _stateTime >= st->buildUpTime;
}

/** Fires all events defined by the current state and queues them for external systems. */
void Enemy::fireEvents() {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return;

    for (const auto& ev : st->events) {
        FiredEvent fe;
        fe.def = ev;
        fe.target = ev.target;
        fe.facingAtFire = _facing;
        fe.stateName = st->name;
        _firedEvents.push_back(fe);
    }

    _eventsFiredThisState = true;
}

/** Applies the current state's cooldown to prevent immediate re-entry into attack states. */
void Enemy::applyCooldown() {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return;

    _attackLockout = std::max(_attackLockout, st->cooldownTime);
}

/** Returns the state's configured next state, or falls back to "idle" if invalid/missing. */
std::string Enemy::getNextStateOrIdle() const {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return "idle";

    if (!st->nextState.empty() && _states.count(st->nextState) > 0)
        return st->nextState;

    return "idle";
}

/** Transitions to the current state's configured next state without additional checks. */
void Enemy::transitionToNextState() {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return;
    enterState(st->nextState);
}

/** Updates the enemy's state execution, firing events and transitioning when ready. */
void Enemy::update(float dt) {

    tick(dt);

    if (readyToFire()) {
        fireEvents();
        applyCooldown();
        enterState(getNextStateOrIdle());
    }
}

/** Returns all fired events since last call and clears the internal event buffer. */
std::vector<Enemy::FiredEvent> Enemy::takeFiredEvents() {
    std::vector<FiredEvent> out;
    out.swap(_firedEvents);
    return out;
}

/** Increases/decreases the enemy health, clamping health to zero or max health. */
void Enemy::updateHealth(float amount) {
    _currentHealth += amount;
    if (_currentHealth > _maxHealth) _currentHealth = _maxHealth;
    if (_currentHealth < 0.0f) _currentHealth = 0.0f;
}


