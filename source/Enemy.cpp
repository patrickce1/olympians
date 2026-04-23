// Enemy.cpp
#include "Enemy.h"
#include "GameScene.h"
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
 * Checks if the enemy is currently in an attack phase (post-buildup) for its current animation.
 * Returns true if we've elapsed past the buildup phase duration.
 * 
 * @param animationRegistry  Map of animation IDs to animation metadata entries
 * @return true if in attack phase, false if in buildup phase or animation has no attack phase
 */
bool Enemy::isInAttackPhase(const std::unordered_map<std::string, class AnimationEntry>& animationRegistry) const {
    const EnemyLoader::StateDef* stateDef = getCurrentStateDef();
    if (!stateDef || stateDef->animationKey.empty()) {
        return false;  // No animation metadata
    }
    
    // Look up animation in registry
    auto registryEntry = animationRegistry.find(stateDef->animationKey);
    if (registryEntry == animationRegistry.end()) {
        return false;  // Animation not found in registry
    }
    
    const auto& animEntry = registryEntry->second;
    
    // Calculate buildup duration in seconds
    float buildupDuration = animEntry.buildupFrameCount * animEntry.frameDuration;
    
    // In attack phase if we've elapsed past the buildup phase
    // (If buildupFrameCount == frameCount, this will never be true since animation completes before it)
    return _stateTime >= buildupDuration;
}

/** Returns true if successfully enters requested state. False and idle otherwise. */
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

/** Immediately enters the state and resets timers. */
void Enemy::enterState(EnemyLoader::State state) {
    // Only reset stateTime if actually changing states
    // If staying in the same state (like IDLE -> IDLE), keep accumulating time
    if (_currentState != state) {
        _stateTime = 0.0f;
        _eventsFiredThisState = false;
    }
    _currentState = state;
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

/** Updates timers.*/
void Enemy::tick(float dt) {
    if (dt <= 0.0f) return;

    const float previousStunDuration = _stunDuration;
    const float previousLoveDuration = _loveDuration;

    if (_stunDuration > 0.0f) {
        _stunDuration = std::max(0.0f, _stunDuration - dt);
        if (previousStunDuration > 0.0f && _stunDuration <= 0.0f) {
            CULog("Enemy stun expired: enemy='%s'", _enemyId.c_str());
        }
    }

    if (_loveDuration > 0.0f) {
        _loveDuration = std::max(0.0f, _loveDuration - dt);
        if (previousLoveDuration > 0.0f && _loveDuration <= 0.0f) {
            CULog("Enemy love expired: enemy='%s'", _enemyId.c_str());
        }
    }

    const float frozenDuration = std::max(previousStunDuration, previousLoveDuration);
    const float activeCombatDt = std::max(0.0f, dt - frozenDuration);
    if (activeCombatDt > 0.0f) {
        _stateTime += activeCombatDt;
        _attackLockout = std::max(0.0f, _attackLockout - activeCombatDt);
    }

    for (int side = 0; side < NUM_PLAYERS; side++) {
        if (_vulnerableDurations[side] <= 0.0f) {
            continue;
        }

        const float previousDuration = _vulnerableDurations[side];
        _vulnerableDurations[side] = std::max(0.0f, _vulnerableDurations[side] - dt);
        if (previousDuration > 0.0f && _vulnerableDurations[side] == 0.0f) {
            _vulnerableSideMultipliers[side] = 1.0f;
            setSideMultiplier(side, _baseSideMultipliers[side]);
            CULog("Enemy vulnerability ended: enemy='%s' side=%d", _enemyId.c_str(), side);
        }
    }
}

/** Returns true when the animation has fully completed and events have not yet fired.
 * 
 * For animated states: Returns true when currentAnimationFrame reaches frameCount-1 (the last frame).
 * For non-animated states: Returns true when buildUpTime elapses.
 * 
 * Once true, determines when state should transition and events should fire.
 * 
 * @return true if animation/duration complete and events not yet fired
 */
bool Enemy::readyToFire() const {
    const EnemyLoader::StateDef* stateDef = getCurrentStateDef();
    if (!stateDef) return false;
    if (_eventsFiredThisState) return false;
    
    if (stateDef->frameCount <= 0) {
        // Non-animated states use buildUpTime
        return _stateTime >= stateDef->buildUpTime;
    }
    
    // Animated states: fire when reaching final frame (frameCount - 1, since 0-indexed)
    return _currentAnimationFrame >= (stateDef->frameCount - 1);
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

/**
 * Applies or refreshes a stun without changing the enemy's current state.
 *
 * @param duration  The stun time to apply, in seconds.
 */
void Enemy::applyStun(float duration) {
    if (duration <= 0.0f) {
        return;
    }

    const bool wasStunned = isStunned();
    _stunDuration = std::max(_stunDuration, duration);

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

    if (!wasStunned && willBeStunned) {
        CULog("Enemy stun applied: enemy='%s' duration=%.3f", _enemyId.c_str(), _stunDuration);
    } else if (wasStunned && !willBeStunned) {
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

    if (!wasLoved && willBeLoved) {
        CULog("Enemy love applied: enemy='%s' duration=%.3f", _enemyId.c_str(), _loveDuration);
    } else if (wasLoved && !willBeLoved) {
        CULog("Enemy love ended: enemy='%s'", _enemyId.c_str());
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

    if (!wasVulnerable && willBeVulnerable) {
        CULog("Enemy vulnerable: enemy='%s'", _enemyId.c_str());
    } else if (wasVulnerable && !willBeVulnerable) {
        CULog("Enemy vulnerability ended: enemy='%s'", _enemyId.c_str());
    }
}

/** Clears runtime-only combat effects so a reset round starts from a clean enemy state. */
void Enemy::clearRuntimeEffects() {
    _stunDuration = 0.0f;
    _loveDuration = 0.0f;

    for (int side = 0; side < NUM_PLAYERS; side++) {
        _vulnerableDurations[side] = 0.0f;
        _vulnerableSideMultipliers[side] = 1.0f;
        setSideMultiplier(side, _baseSideMultipliers[side]);
    }
}

/** Handles taking damage and applying the side modifiers
 * Use this method instead of updateHealth() for appropriate damage multiplication
 *
 * @param damage is the amount of damage being done to the boss
 * @param playerIndex is the index that was assigned to the player by the host
 */
void Enemy::takeDamage(float damage, int playerIndex) {
    const int relativeIndex = relativeSideForPlayer(playerIndex, _targetIndex);
    updateHealth(-(damage * _sideMultipliers[relativeIndex]));
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
