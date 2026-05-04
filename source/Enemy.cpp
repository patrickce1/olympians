// Enemy.cpp
#include "Enemy.h"
#include "scenes/GameScene.h"
#include <algorithm>
#include <cmath>
#include <cugl/cugl.h>
#include <cmath>

using namespace cugl;

/** Singleton loader instance */
static EnemyLoader staticEnemyLoader;
/** Flag to ensure loader is initialized only once */
static bool staticEnemyLoaderInitialized = false; 
/** Path where the loader was initialized (for error checking if multiple paths are used) */
static std::string staticEnemyLoaderPath; 

/**
 * Ensures the animation registry is loaded from AssetManager (if provided).
 * Should be called before ensuring JSON definitions are loaded.
 * 
 * @param assets The AssetManager containing animation metadata, or nullptr if not available
 * @return true if successful (or no assets provided), false on error
 */
static bool ensureAnimationRegistryLoaded(const std::shared_ptr<cugl::AssetManager>& assets) {
    if (!assets) {
        return true;  // No assets provided, continue without animations
    }
    
    // Only load if not already loaded
    if (staticEnemyLoader.isAnimationRegistryLoaded()) {
        return true;  // Already loaded
    }
    
    if (!staticEnemyLoader.loadAnimationRegistry(assets)) {
        CULog("WARNING: Failed to load animation registry, continuing without animation metadata");
        return false;  // Non-fatal error
    }
    
    return true;
}

/**
 * Initializes the static enemy loader with definitions from a JSON file.
 * Uses static initialization pattern to load enemy definitions once per application run.
 * Must call ensureAnimationRegistryLoaded() first if animation metadata is needed.
 * 
 * @param jsonPath Path to enemies.json file
 * @return true if loader is ready, false on error
 */
static bool ensureEnemyLoaderInitialized(const std::string& jsonPath) {
    if (!staticEnemyLoaderInitialized) {
        if (!staticEnemyLoader.loadFromFile(jsonPath)) {
            CULog("ERROR: Failed to load enemy JSON from %s", jsonPath.c_str());
            return false;
        }
        staticEnemyLoaderInitialized = true;
        staticEnemyLoaderPath = jsonPath;
    }

    if (staticEnemyLoaderPath != jsonPath) {
        CULog("ERROR: Enemy JSON already loaded from different path: %s vs %s", 
              staticEnemyLoaderPath.c_str(), jsonPath.c_str());
        return false;
    }

    return true;
}

/**
 * Gets the static enemy loader singleton.
 * Must call ensureEnemyLoaderInitialized() first.
 * 
 * @return Reference to the static enemy loader
 */
static EnemyLoader& getEnemyLoader() {
    return staticEnemyLoader;
}

/**
 * Wraps a side index into the valid relative-side range [0, NUM_PLAYERS).
 *
 * @param index The raw side index to normalize.
 * @return The wrapped relative side index.
 */
static int normalizeSideIndex(int index) {
    const int wrapped = index % Enemy::NUM_PLAYERS;
    return (wrapped < 0) ? wrapped + Enemy::NUM_PLAYERS : wrapped;
}

/**
 * Converts an attacking player's slot into the side they occupy relative to the enemy's current facing.
 *
 * @param playerIndex The attacking player's slot index.
 * @param targetIndex The player slot the enemy is currently facing.
 * @return The relative side index, where 0 is the enemy's current facing side.
 */
static int relativeSideForPlayer(int playerIndex, int targetIndex) {
    return normalizeSideIndex(playerIndex - targetIndex);
}

/**
 * Initializes this enemy instance from the given enemy definition.
 * Sets up state machine, health, side multipliers, and AI parameters.
 * 
 * @param def The enemy definition to initialize from
 * @return true if initialization succeeds, false if required state missing
 */
bool Enemy::initializeFromDef(const EnemyLoader::EnemyDef& def) {
    _enemyId = def.id;
    _spritesheetPath = def.spritesheetPath;
    _maxHealth = def.maxHealth;
    _currentHealth = def.maxHealth;
    _states = def.states;
    _customData = def.customData;
    
    // Initialize all side damage multipliers to default (1.0 = no modification)
    for (int i = 0; i < NUM_PLAYERS; i++) {
        _sideMultipliers[i] = 1.0f;
        _baseSideMultipliers[i] = 1.0f;
        _vulnerableDurations[i] = 0.0f;
        _vulnerableSideMultipliers[i] = 1.0f;
    }

    // Verify required idle state exists
    if (_states.count(EnemyLoader::State::IDLE) == 0) {
        CULog("ERROR: Enemy '%s' missing required idle state", def.id.c_str());
        return false;
    }
    
    enterState(EnemyLoader::State::IDLE);
    _attackLockout = 0.0f;
    _retargetLikelihood = def.ai.retargetLikelihood;

    // Clear any previous stun/love state when reinitializing the enemy instance.
    _stunDuration = 0.0f;
    _loveDuration = 0.0f;
    _slowDuration = 0.0f;
    _slowMultiplier = 1.0f;

    for (int i = 0; i < NUM_PLAYERS; i++) {
        _vulnerableDurations[i] = 0.0f;
        _vulnerableSideMultipliers[i] = 1.0f;
    }

    _defenseLikelihood = def.ai.defenseLikelihood;

    return true;
}

/** Returns true if the enemy initializes successfully. 
 *  @param enemyId The unique ID of the enemy to load (e.g., "cyclops")
 *  @param jsonPath Path to the enemies.json configuration file
 *  @return true if initialization succeeds, false on error
*/
bool Enemy::init(const std::string& enemyId, const std::string& jsonPath) {
    if (!ensureEnemyLoaderInitialized(jsonPath)) {
        return false;
    }

    EnemyLoader& loader = getEnemyLoader();
    
    if (!loader.has(enemyId)) {
        CULog("ERROR: Enemy ID not found: %s", enemyId.c_str());
        return false;
    }

    const EnemyLoader::EnemyDef& def = loader.get(enemyId);
    return initializeFromDef(def);
}

/**
 * Initializes the enemy with animation metadata loaded from AssetManager.
 * Loads animation registry FIRST, then initializes enemy state.
 * Uses smart caching: definitions load once, registry loads only when provided and not yet loaded.
 * 
 * @param enemyId  The unique ID of the enemy to load
 * @param jsonPath Path to the enemies.json configuration file
 * @param assets   The AssetManager containing animation metadata in enemyAnimations.json
 * @return true if initialization succeeds, false on error
 */
bool Enemy::init(const std::string& enemyId, const std::string& jsonPath, 
                const std::shared_ptr<cugl::AssetManager>& assets) {
    // Load animation registry FIRST so state definitions can be populated with animation metadata
    if (!ensureAnimationRegistryLoaded(assets)) {
        // Non-fatal error - continue without animations
    }
    
    // Now load enemy definitions (populated from registry if animation metadata is available)
    if (!ensureEnemyLoaderInitialized(jsonPath)) {
        return false;
    }

    EnemyLoader& loader = getEnemyLoader();
    
    if (!loader.has(enemyId)) {
        CULog("ERROR: Enemy ID not found: %s", enemyId.c_str());
        return false;
    }

    const EnemyLoader::EnemyDef& def = loader.get(enemyId);
    return initializeFromDef(def);
}

/** Returns the current state that the enemy is in. */
const EnemyLoader::StateDef* Enemy::getCurrentStateDef() const {
    auto cur = _states.find(_currentState);
    if (cur == _states.end()) return nullptr;
    return &cur->second;
}

/**
 * Returns true when the enemy has passed the buildup phase and is actively executing
 * its attack animation (i.e. the frames after the wind-up loop).
 * Used to block stuns and retargets from interrupting an in-progress strike.
 *
 * @return true if currently in the attack phase of an animated state, false otherwise
 */
bool Enemy::isInAttackAnimationPhase() const {
    const EnemyLoader::StateDef* stateDef = getCurrentStateDef();
    if (!stateDef || stateDef->loopStartFrame < 0) return false;

    if (stateDef->damageFrame < 0) {
        // No damage frame: attack phase is simply past buildUpTime (in the outro)
        return stateDef->buildUpTime > 0.0f && _stateTime >= stateDef->buildUpTime;
    }

    if (stateDef->damageFrame > stateDef->loopEndFrame) {
        // Loop comes before damage: attack phase runs from loop end through end of animation
        return _stateTime >= stateDef->buildUpTime;
    }

    // Damage comes before loop: attack phase runs from state entry until damage fires
    return _currentAnimationFrame < stateDef->damageFrame;
}

/** Returns true if successfully enters requested state. False and idle otherwise.
 *
 * @param state   The requested state.
 * @return True if successfully enters requested state. False otherwise.
 */
bool Enemy::requestState(EnemyLoader::State state) {
    if (_states.count(state) == 0) return false;
    if (isLoved()) return false; // Loved enemies cannot choose attacks
    if (isStunned()) return false; // Stunned enemies cannot choose attacks while frozen
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

/**
 * Transitions to the given state, resetting timers and queuing entry events.
 * No-ops if the enemy is already in that state (timers continue accumulating).
 * Entry events (e.g. applying side modifiers at the start of defense) are queued
 * here so they fire on the same frame the state begins.
 *
 * @param state  The state to enter
 */
void Enemy::enterState(EnemyLoader::State state) {
    if (_currentState == state) return;

    _stateTime = 0.0f;
    _eventsFiredThisState = false;
    _currentState = state;

    const EnemyLoader::StateDef* stateDef = getCurrentStateDef();
    if (!stateDef) return;

    for (const auto& eventDef : stateDef->entryEvents) {
        FiredEvent firedEvent;
        firedEvent.def = eventDef;
        firedEvent.state = state;
        _firedEvents.push_back(firedEvent);
    }
}

/** Forces the enemy into idle and clears progress on the interrupted state. */
void Enemy::forceIdle() {
    if (_currentState != EnemyLoader::State::IDLE) {
        enterState(EnemyLoader::State::IDLE);
    } else {
        _stateTime = 0.0f;
        _eventsFiredThisState = false;
    }
}

/** Updates enemy effects timers.
 *
 * @param dt  The elapsed time since the previous frame, in seconds.
 */
void Enemy::tick(float dt) {
    if (dt <= 0.0f) return;

    const float previousStunDuration = _stunDuration;
    const float previousLoveDuration = _loveDuration;
    const float previousSlowDuration = _slowDuration;
    const float previousSlowMultiplier = _slowMultiplier;

    if (_stunDuration > 0.0f) {
        _stunDuration = std::max(0.0f, _stunDuration - dt);
        if (previousStunDuration > 0.0f && _stunDuration <= 0.0f && _debug) {
            CULog("Enemy stun expired: enemy='%s'", _enemyId.c_str());
        }
    }

    if (_loveDuration > 0.0f) {
        _loveDuration = std::max(0.0f, _loveDuration - dt);
        if (previousLoveDuration > 0.0f && _loveDuration <= 0.0f && _debug) {
            CULog("Enemy love expired: enemy='%s'", _enemyId.c_str());
        }
    }

    if (_slowDuration > 0.0f) {
        _slowDuration = std::max(0.0f, _slowDuration - dt);
        if (previousSlowDuration > 0.0f && _slowDuration <= 0.0f) {
            _slowMultiplier = 1.0f;
            if (_debug) {
                CULog("Enemy slow expired: enemy='%s'", _enemyId.c_str());
            }
        }
    }

    const float frozenDuration = std::max(previousStunDuration, previousLoveDuration);
    const float activeCombatDt = std::max(0.0f, dt - frozenDuration);
    if (activeCombatDt > 0.0f) {
        const float slowedCombatDt = std::max(0.0f, std::min(dt, previousSlowDuration) - frozenDuration);
        const float normalCombatDt = std::max(0.0f, activeCombatDt - slowedCombatDt);
        const float slowMultiplier = (previousSlowDuration > frozenDuration) ? previousSlowMultiplier : 1.0f;
        _stateTime += (slowedCombatDt * slowMultiplier) + normalCombatDt;
        _attackLockout = std::max(0.0f, _attackLockout - activeCombatDt);
    }

    for (int side = 0; side < NUM_PLAYERS; side++) {
        if (_vulnerableDurations[side] <= 0.0f) {
            continue;
        }

        const float previousDuration = _vulnerableDurations[side];
        _vulnerableDurations[side] = std::max(0.0f, _vulnerableDurations[side] - dt);
        if (previousDuration > 0.0f && _vulnerableDurations[side] <= 0.0f) {
            _vulnerableSideMultipliers[side] = 1.0f;
            setSideMultiplier(side, _baseSideMultipliers[side]);
            if (_debug) {
                CULog("Enemy vulnerability ended: enemy='%s' side=%d", _enemyId.c_str(), side);
            }
        }
    }
}

/**
 * Returns true when events should fire for the current state.
 *
 * For time-based states (frameCount == 0): fires when buildUpTime elapses.
 * For frame-based states with a damageFrame: fires when that frame is reached.
 * For frame-based states without a damageFrame: fires at the last frame.
 *
 * @return true if the event fire condition is met and events have not yet fired this state
 */
bool Enemy::readyToFire() const {
    const EnemyLoader::StateDef* stateDef = getCurrentStateDef();
    if (!stateDef) return false;
    if (_eventsFiredThisState) return false;

    // damageFrame overrides all — works for both looping and linear states
    if (stateDef->damageFrame >= 0) {
        return _currentAnimationFrame >= stateDef->damageFrame;
    }

    if (stateDef->frameCount <= 0) {
        // Looping state: fire when loop ends. buildUpTime=0 means loop forever (e.g. idle).
        if (stateDef->buildUpTime <= 0.0f) return false;
        return _stateTime >= stateDef->buildUpTime;
    }

    return _currentAnimationFrame >= (stateDef->frameCount - 1);
}

/**
 * Returns true when the current state has fully completed and should transition.
 *
 * For time-based states: completes when buildUpTime elapses (same as readyToFire).
 * For frame-based states: completes when the last frame is reached, regardless of damageFrame.
 *
 * @return true if the state is complete and should transition to the next state
 */
bool Enemy::isStateComplete() const {
    const EnemyLoader::StateDef* stateDef = getCurrentStateDef();
    if (!stateDef) return false;

    if (stateDef->frameCount <= 0) {
        if (stateDef->buildUpTime <= 0.0f) return false;
        float outroTime = stateDef->outroFrameCount * stateDef->frameDuration;
        return _stateTime >= stateDef->buildUpTime + outroTime;
    }

    return _currentAnimationFrame >= (stateDef->frameCount - 1);
}

/**
 * Forces the enemy into a specific attack state immediately.
 * Resets the attack lockout to ensure the state machine doesn't block the transition.
 * @param attackState The specific attack state to enter (e.g. ATTACK_1)
 */
void Enemy::forceAttack(EnemyLoader::State attackState) {
    // Reset lockout so the attack can definitely start immediately
    _attackLockout = 0.0f;
    // Force the state
    enterState(attackState);
}

/**
 * Forces the enemy into  defense state immediately.
 * Resets the attack lockout to ensure the state machine doesn't block the transition.
 * @param attackState The specific attack state to enter (e.g. DEFENSE_1)
 */
void Enemy::forceDefense(EnemyLoader::State defenseState) {
    // Reset lockout so the defense can start immediately
    _attackLockout = 0.0f;
    // Force the state
    enterState(defenseState);
}

/** Fires all events defined for the current state, adding them to the events buffer.
 * Called once per state when animation completes.
 */
void Enemy::fireEvents() {
    const EnemyLoader::StateDef* stateDef = getCurrentStateDef();
    if (!stateDef) return;

    for (const auto& eventDef : stateDef->events) {
        FiredEvent firedEvent;
        firedEvent.def = eventDef;
        firedEvent.state = stateDef->state;
        _firedEvents.push_back(firedEvent);
    }

    _eventsFiredThisState = true;
}

/** Sets the cooldown timer based on the current state's cooldownTime.
 * Prevents rapid consecutive state transitions.
 */
void Enemy::applyCooldown() {
    const EnemyLoader::StateDef* stateDef = getCurrentStateDef();
    if (!stateDef) return;
    _attackLockout = std::max(_attackLockout, stateDef->cooldownTime);
}

/** Returns the next state defined in the current state, or defaults to IDLE.
 * Used for state machine transitions after animation completes.
 * @return The next state to transition to
 */
EnemyLoader::State Enemy::getNextStateOrIdle() const {
    const EnemyLoader::StateDef* stateDef = getCurrentStateDef();
    if (!stateDef) return EnemyLoader::State::IDLE;

    if (_states.count(stateDef->nextState) > 0) {
        return stateDef->nextState;
    }

    return EnemyLoader::State::IDLE;
}

/** Main update loop for enemy state machine.
 * Updates internal timers, detects animation completion, fires events, applies cooldown, and transitions states.
 * 
 * @param dt Delta time in seconds since last update
 */
void Enemy::update(float dt) {
    tick(dt);

    if (isLoved()) {
        forceIdle();
        return;
    }

    if (isStunned()) {
        return;
    }

    if (readyToFire()) {
        fireEvents();
    }

    if (isStateComplete()) {
        applyCooldown();
        enterState(getNextStateOrIdle());
    }
}

/**
     * Advances the enemy's state machine and attack lockout by the given amount,
     * without affecting any effect timers (stun, love, vulnerable).
     * Use this instead of a fake dt when you want to speed up state transitions
     * while leaving effect durations intact.
     *
     * @param amount  The time to advance, in seconds.
     */
void Enemy::advanceStateTime(float amount) {
    if (isStunned() || isLoved()) return;
    _stateTime += amount;
    _attackLockout = std::max(0.0f, _attackLockout - amount);
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

/**
 * Applies or refreshes a stun without changing the enemy's current state.
 * Does not apply if is already in an attack phase of an animation 
 *
 * @param duration  The stun time to apply, in seconds.
 */
void Enemy::applyStun(float duration) {
    if (duration <= 0.0f) {
        return;
    }

    if (isInAttackAnimationPhase()) {
        if (_debug) {
            const EnemyLoader::StateDef* stateDef = getCurrentStateDef();
            CULog("Enemy stun ignored during attack phase: enemy='%s' state='%s'",
                  _enemyId.c_str(),
                  stateDef->name.c_str());
        }
        return;
    }

    const bool wasStunned = isStunned();
    _stunDuration = std::max(_stunDuration, duration);

    if (!_debug) return;
    
    if (!wasStunned) {
        CULog("Enemy stun applied: enemy='%s' duration=%.3f", _enemyId.c_str(), _stunDuration);
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

    if (!wasStunned && willBeStunned && _debug) {
        CULog("Enemy stun applied: enemy='%s' duration=%.3f", _enemyId.c_str(), _stunDuration);
    } else if (wasStunned && !willBeStunned && _debug) {
        CULog("Enemy stun ended: enemy='%s'", _enemyId.c_str());
    }
}

/**
 * Applies or refreshes a love, forcing the enemy idle and extending the remaining duration.
 *
 * @param duration  The love time to apply, in seconds.
 */
void Enemy::applyLove(float duration) {
    if (duration <= 0.0f) {
        return;
    }

    const bool wasLoved = isLoved();
    _loveDuration = std::max(_loveDuration, duration);
    forceIdle();

    if (!_debug) return;
    
    if (!wasLoved) {
        CULog("Enemy love applied: enemy='%s' duration=%.3f", _enemyId.c_str(), _loveDuration);
    } else {
        CULog("Enemy love refreshed: enemy='%s' duration=%.3f", _enemyId.c_str(), _loveDuration);
    }
}

/**
 * Overwrites local love time from the host snapshot so remote clients mirror the authoritative state.
 *
 * @param duration  The authoritative remaining love time, in seconds.
 */
void Enemy::syncLoveDuration(float duration) {
    duration = std::max(0.0f, duration);
    const bool wasLoved = isLoved();
    const bool willBeLoved = duration > 0.0f;
    _loveDuration = duration;

    if (willBeLoved) {
        forceIdle();
    }

    if (!wasLoved && willBeLoved && _debug) {
        CULog("Enemy love applied: enemy='%s' duration=%.3f", _enemyId.c_str(), _loveDuration);
    } else if (wasLoved && !willBeLoved && _debug) {
        CULog("Enemy love ended: enemy='%s'", _enemyId.c_str());
    }
}

/**
 * Applies or refreshes a slow, scaling only state-time advancement for the duration.
 *
 * @param multiplier The state-time scale to apply while slowed.
 * @param duration   The slow time to apply, in seconds.
 */
void Enemy::applySlow(float multiplier, float duration) {
    if (duration <= 0.0f) {
        return;
    }

    multiplier = std::max(0.0f, multiplier);
    const bool wasSlowed = isSlowed();
    _slowDuration = std::max(_slowDuration, duration);
    _slowMultiplier = wasSlowed ? std::min(_slowMultiplier, multiplier) : multiplier;

    if (!_debug) return;

    if (!wasSlowed) {
        CULog("Enemy slow applied: enemy='%s' multiplier=%.3f duration=%.3f",
              _enemyId.c_str(), _slowMultiplier, _slowDuration);
    } else {
        CULog("Enemy slow refreshed: enemy='%s' multiplier=%.3f duration=%.3f",
              _enemyId.c_str(), _slowMultiplier, _slowDuration);
    }
}

/**
 * Overwrites local slow state from the host snapshot so remote clients mirror the authoritative state.
 *
 * @param multiplier The authoritative state-time scale while slowed.
 * @param duration   The authoritative remaining slow time, in seconds.
 */
void Enemy::syncSlow(float multiplier, float duration) {
    duration = std::max(0.0f, duration);
    multiplier = std::max(0.0f, multiplier);
    const bool wasSlowed = isSlowed();
    const bool willBeSlowed = duration > 0.0f;
    _slowDuration = duration;
    _slowMultiplier = willBeSlowed ? multiplier : 1.0f;

    if (!wasSlowed && willBeSlowed && _debug) {
        CULog("Enemy slow applied: enemy='%s' multiplier=%.3f duration=%.3f",
              _enemyId.c_str(), _slowMultiplier, _slowDuration);
    } else if (wasSlowed && !willBeSlowed && _debug) {
        CULog("Enemy slow ended: enemy='%s'", _enemyId.c_str());
    }
}

/**
 * Returns whether any relative side of the enemy is currently vulnerable.
 *
 * @return true if at least one side has a positive vulnerable timer.
 */
bool Enemy::isVulnerable() const {
    for (float duration : _vulnerableDurations) {
        if (duration > 0.0f) {
            return true;
        }
    }
    return false;
}

/**
 * Returns the longest remaining vulnerable duration across all relative sides.
 *
 * @return The maximum remaining vulnerable time in seconds.
 */
float Enemy::getVulnerableDuration() const {
    float longestDuration = 0.0f;
    for (float duration : _vulnerableDurations) {
        longestDuration = std::max(longestDuration, duration);
    }
    return longestDuration;
}

/**
 * Returns the strongest active vulnerable multiplier across all relative sides.
 *
 * @return The highest active vulnerable multiplier, or 1.0f if none are active.
 */
float Enemy::getVulnerableMultiplier() const {
    float strongestMultiplier = 1.0f;
    for (int side = 0; side < NUM_PLAYERS; side++) {
        if (_vulnerableDurations[side] > 0.0f) {
            strongestMultiplier = std::max(strongestMultiplier, _vulnerableSideMultipliers[side]);
        }
    }
    return strongestMultiplier;
}

/**
 * Returns the remaining vulnerable duration for one relative side.
 *
 * @param relativeIndex The relative side index to query.
 * @return The remaining vulnerable time for that side in seconds.
 */
float Enemy::getVulnerableDurationForSide(int relativeIndex) const {
    return _vulnerableDurations[normalizeSideIndex(relativeIndex)];
}

/**
 * Returns the active vulnerable multiplier for one relative side.
 *
 * @param relativeIndex The relative side index to query.
 * @return The vulnerable multiplier for that side, or 1.0f if inactive.
 */
float Enemy::getVulnerableMultiplierForSide(int relativeIndex) const {
    const int side = normalizeSideIndex(relativeIndex);
    return (_vulnerableDurations[side] > 0.0f) ? _vulnerableSideMultipliers[side] : 1.0f;
}

/**
 * Applies vulnerability to the side hit by the given player.
 *
 * The side is stored relative to the enemy's current facing so it follows turns
 * until that side's timer expires.
 *
 * @param multiplier The damage multiplier to apply to the struck side.
 * @param duration   The vulnerable duration in seconds.
 * @param playerIndex The attacking player's slot index.
 */
void Enemy::applyVulnerable(float multiplier, float duration, int playerIndex) {
    if (duration <= 0.0f) {
        return;
    }

    const int relativeIndex = relativeSideForPlayer(playerIndex, _targetIndex);
    const bool wasVulnerable = _vulnerableDurations[relativeIndex] > 0.0f;
    _vulnerableDurations[relativeIndex] = std::max(_vulnerableDurations[relativeIndex], duration);
    _vulnerableSideMultipliers[relativeIndex] = std::max(_vulnerableSideMultipliers[relativeIndex], std::max(1.0f, multiplier));
    setSideMultiplier(relativeIndex, _baseSideMultipliers[relativeIndex]);

    if (!_debug) return;
    
    if (!wasVulnerable) {
        CULog("Enemy vulnerable: enemy='%s' side=%d multiplier=%.3f duration=%.3f",
              _enemyId.c_str(),
              relativeIndex,
              _vulnerableSideMultipliers[relativeIndex],
              _vulnerableDurations[relativeIndex]);
    } else {
        CULog("Enemy vulnerability refreshed: enemy='%s' side=%d multiplier=%.3f duration=%.3f",
              _enemyId.c_str(),
              relativeIndex,
              _vulnerableSideMultipliers[relativeIndex],
              _vulnerableDurations[relativeIndex]);
    }
}

/**
 * Applies the same vulnerability to all relative sides of the enemy.
 *
 * @param multiplier The damage multiplier to apply to each side.
 * @param duration   The vulnerable duration in seconds.
 * @return true if at least one side was updated, false if duration was not positive.
 */
bool Enemy::applyVulnerableToAllSides(float multiplier, float duration) {
    if (duration <= 0.0f) {
        return false;
    }

    // updatedAnySide is for potential future use
    bool updatedAnySide = false;
    const float resolvedMultiplier = std::max(1.0f, multiplier);
    for (int side = 0; side < NUM_PLAYERS; side++) {
        const bool wasVulnerable = _vulnerableDurations[side] > 0.0f;
        _vulnerableDurations[side] = std::max(_vulnerableDurations[side], duration);
        _vulnerableSideMultipliers[side] = std::max(_vulnerableSideMultipliers[side], resolvedMultiplier);
        setSideMultiplier(side, _baseSideMultipliers[side]);
        updatedAnySide = true;

        if (_debug) continue;
        
        if (!wasVulnerable) {
            CULog("Enemy vulnerable: enemy='%s' side=%d multiplier=%.3f duration=%.3f",
                  _enemyId.c_str(),
                  side,
                  _vulnerableSideMultipliers[side],
                  _vulnerableDurations[side]);
        } else {
            CULog("Enemy vulnerability refreshed: enemy='%s' side=%d multiplier=%.3f duration=%.3f",
                  _enemyId.c_str(),
                  side,
                  _vulnerableSideMultipliers[side],
                  _vulnerableDurations[side]);
        }
    }

    return updatedAnySide;
}

/**
 * Overwrites local vulnerable state from the host snapshot so remote clients mirror the authoritative state.
 *
 * @param multipliers The authoritative per-side vulnerable multipliers.
 * @param durations   The authoritative per-side vulnerable durations in seconds.
 */
void Enemy::syncVulnerable(const std::array<float, NUM_PLAYERS>& multipliers,
                           const std::array<float, NUM_PLAYERS>& durations) {
    const bool wasVulnerable = isVulnerable();
    bool willBeVulnerable = false;

    for (int side = 0; side < NUM_PLAYERS; side++) {
        _vulnerableDurations[side] = std::max(0.0f, durations[side]);
        _vulnerableSideMultipliers[side] = (_vulnerableDurations[side] > 0.0f) ? std::max(1.0f, multipliers[side]) : 1.0f;
        setSideMultiplier(side, _baseSideMultipliers[side]);
        willBeVulnerable = willBeVulnerable || (_vulnerableDurations[side] > 0.0f);
    }

    if (!wasVulnerable && willBeVulnerable && _debug) {
        CULog("Enemy vulnerable: enemy='%s'", _enemyId.c_str());
    } else if (wasVulnerable && !willBeVulnerable && _debug) {
        CULog("Enemy vulnerability ended: enemy='%s'", _enemyId.c_str());
    }
}

/** Clears runtime-only combat effects so a reset round starts from a clean enemy state. */
void Enemy::clearRuntimeEffects() {
    _stunDuration = 0.0f;
    _loveDuration = 0.0f;
    _slowDuration = 0.0f;
    _slowMultiplier = 1.0f;

    for (int side = 0; side < NUM_PLAYERS; side++) {
        _vulnerableDurations[side] = 0.0f;
        _vulnerableSideMultipliers[side] = 1.0f;
        setSideMultiplier(side, _baseSideMultipliers[side]);
    }
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
    const int side = normalizeSideIndex(relativeIndex);
    _baseSideMultipliers[side] = multiplier;
    _sideMultipliers[side] = _baseSideMultipliers[side] * _vulnerableSideMultipliers[side];
}

/** Returns the multiplier data for the given absolute side index.
 * @param absoluteIndex is the side we want to get. Index 0 corresponds to the side facing the host, regardless of the boss' direction.
 */
float Enemy::getSideMultiplier(int absoluteIndex) {
    const int relativeIndex = relativeSideForPlayer(absoluteIndex, _targetIndex);
    return _sideMultipliers[relativeIndex];
}

/** Checks if this enemy should use their defensive move
This can and should be overwritten for each boss to have custom logic on when they decide to use their defensive move */
bool Enemy::shouldDefend() {
    return false;
}

/**
 * TESTING ONLY: Resets the static enemy loader state to allow reinitializing with different parameters.
 * Used to clear cached loader state between test runs and actual game initialization.
 * Must be called after all tests complete and BEFORE the game initializes enemies with animation metadata.
 * 
 * Example usage in tests:
 *   EnemyTests::runAll("json/enemies.json", "json/houses.json");
 *   Enemy::clearStaticLoaderForTesting();  // Clear cached state
 *   // Then game can initialize properly with animation metadata
 */
void Enemy::clearStaticLoaderForTesting() {
    staticEnemyLoaderInitialized = false;
    staticEnemyLoaderPath.clear();
    // Note: We don't clear the loader contents themselves - they're reused if same path is loaded
}
