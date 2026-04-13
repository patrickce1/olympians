// Enemy.cpp
#include "Enemy.h"
#include <algorithm>
#include <cugl/cugl.h>
#include <cmath>

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
    // Clear any previous stun state when reinitializing the enemy instance.
    _stunDuration = 0.0f;
    _vulnerableDuration = 0.0f;
    _vulnerableMultiplier = 1.0f;

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
    if (isStunned() && stateName != "idle") return false; // Stunned enemies cannot choose attacks
    if (_attackLockout > 0.0f && stateName != "idle") return false; // Lockout is active, only allow idle

    enterState(stateName);
    return true;
}

/** Sets the enemy retarget likelihood. Only used for testing, should normally be defined in enemies JSON. */
void Enemy::setRetargetLikelihood(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    _retargetLikelihood = v;
}

/** Immediately enters the state and resets timers. */
void Enemy::enterState(const std::string& stateName) {
    _currentState = stateName;
    _stateTime = 0.0f;
    _eventsFiredThisState = false;
}

/** Forces the enemy into idle and clears progress on the interrupted state. */
void Enemy::forceIdle() {
    if (_currentState != "idle") {
        enterState("idle");
    } else {
        _stateTime = 0.0f;
        _eventsFiredThisState = false;
    }
}

/** Updates timers.*/
void Enemy::tick(float dt) {
    if (dt <= 0.0f) return;

    if (!isStunned()) {
        _stateTime += dt;
    }

    _attackLockout = (_attackLockout - dt < 0.0f) ? 0.0f : _attackLockout - dt;

    if (_stunDuration > 0.0f) {
        const float previousDuration = _stunDuration;
        _stunDuration = std::max(0.0f, _stunDuration - dt);
        if (previousDuration > 0.0f && _stunDuration == 0.0f) {
            CULog("Enemy stun ended: enemy='%s'", _enemyId.c_str());
        }
    }

    if (_vulnerableDuration > 0.0f) {
        const float previousDuration = _vulnerableDuration;
        _vulnerableDuration = std::max(0.0f, _vulnerableDuration - dt);
        if (previousDuration > 0.0f && _vulnerableDuration == 0.0f) {
            _vulnerableMultiplier = 1.0f;
            CULog("Enemy vulnerability ended: enemy='%s'", _enemyId.c_str());
        }
    }
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

    if (isStunned()) {
        forceIdle();
        return;
    }

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
    if (delta < 0.0f && isVulnerable()) {
        delta *= _vulnerableMultiplier;
    }
    _currentHealth += delta;
    if (_currentHealth > _maxHealth) _currentHealth = _maxHealth;
    if (_currentHealth < 0.0f) _currentHealth = 0.0f;
}

/**
 * Applies or refreshes a stun, forcing the enemy idle and extending the remaining duration.
 *
 * @param duration  The stun time to apply, in seconds.
 */
void Enemy::applyStun(float duration) {
    if (duration <= 0.0f) {
        return;
    }

    const bool wasStunned = isStunned();
    _stunDuration = std::max(_stunDuration, duration);
    forceIdle();

    if (!wasStunned) {
        CULog("Enemy stunned: enemy='%s' duration=%.3f", _enemyId.c_str(), _stunDuration);
    } else {
        CULog("Enemy stun refreshed: enemy='%s' duration=%.3f", _enemyId.c_str(), _stunDuration);
    }
}

/**
 * Overwrites local stun time from the host snapshot so remote clients mirror the authoritative state.
 *
 * @param duration  The authoritative remaining stun time, in seconds.
 */
void Enemy::syncStunDuration(float duration) {
    duration = std::max(0.0f, duration);
    const bool wasStunned = isStunned();
    const bool willBeStunned = duration > 0.0f;
    _stunDuration = duration;

    if (willBeStunned) {
        forceIdle();
    }

    if (!wasStunned && willBeStunned) {
        CULog("Enemy stunned: enemy='%s' duration=%.3f", _enemyId.c_str(), _stunDuration);
    } else if (wasStunned && !willBeStunned) {
        CULog("Enemy stun ended: enemy='%s'", _enemyId.c_str());
    }
}

/**
 * Applies a local authoritative vulnerability, extending the current timer and preserving the strongest multiplier.
 *
 * @param multiplier  Damage multiplier for incoming damage
 * @param duration      Time this state will last
 */
void Enemy::applyVulnerable(float multiplier, float duration) {
    if (duration <= 0.0f) {
        return;
    }

    const bool wasVulnerable = isVulnerable();
    _vulnerableDuration = std::max(0.0f, duration);
    _vulnerableMultiplier = std::max(1.0f, multiplier);

    if (!wasVulnerable) {
        CULog("Enemy vulnerable: enemy='%s' multiplier=%.3f duration=%.3f",
              _enemyId.c_str(),
              _vulnerableMultiplier,
              _vulnerableDuration);
    } else {
        CULog("Enemy vulnerability refreshed: enemy='%s' multiplier=%.3f duration=%.3f",
              _enemyId.c_str(),
              _vulnerableMultiplier,
              _vulnerableDuration);
    }
}

/**
 * Overwrites local vulnerable state from the host snapshot so remote clients mirror the authoritative state.
 *
 * @param multiplier  The authoritative damage multiplier to apply while vulnerable.
 * @param duration    The authoritative remaining vulnerable time, in seconds.
 */
void Enemy::syncVulnerable(float multiplier, float duration) {
    duration = std::max(0.0f, duration);
    multiplier = (duration > 0.0f) ? std::max(1.0f, multiplier) : 1.0f;
    const bool wasVulnerable = isVulnerable();
    const bool willBeVulnerable = duration > 0.0f;
    _vulnerableDuration = duration;
    _vulnerableMultiplier = willBeVulnerable ? multiplier : 1.0f;

    if (!wasVulnerable && willBeVulnerable) {
        CULog("Enemy vulnerable: enemy='%s' multiplier=%.3f duration=%.3f",
              _enemyId.c_str(),
              _vulnerableMultiplier,
              _vulnerableDuration);
    } else if (wasVulnerable && !willBeVulnerable) {
        CULog("Enemy vulnerability ended: enemy='%s'", _enemyId.c_str());
    }
}

/** Clears runtime-only combat effects so a reset round starts from a clean enemy state. */
void Enemy::clearRuntimeEffects() {
    _stunDuration = 0.0f;
    _vulnerableDuration = 0.0f;
    _vulnerableMultiplier = 1.0f;
}
