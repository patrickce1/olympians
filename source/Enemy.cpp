// Enemy.cpp
#include "Enemy.h"
#include "GameScene.h"
#include <algorithm>
#include <cmath>
#include <cugl/cugl.h>

using namespace cugl;

/** Returns true if the enemy initializes successfully. */
bool Enemy::init(const std::string& enemyId, const std::string& jsonPath) {
    static EnemyLoader sLoader;
    static bool sLoaded = false;
    static std::string sLoadedPath;

    CULog("[ENEMY INIT] Called WITHOUT assets for enemyId='%s'", enemyId.c_str());
    
    if (!sLoaded) {
        CULog("[ENEMY INIT] Loading enemy definitions from '%s'", jsonPath.c_str());
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

/** Initializes the enemy with animation metadata loaded from AssetManager.
 * Uses smart caching: definitions load once, registry loads only when needed (not empty). */
bool Enemy::init(const std::string& enemyId, const std::string& jsonPath, 
                const std::shared_ptr<cugl::AssetManager>& assets) {
    static EnemyLoader sLoader;
    static bool sLoaded = false;
    static std::string sLoadedPath;

    CULog("[ENEMY INIT] Called WITH assets for enemyId='%s'", enemyId.c_str());
    
    // Smart registry loading: only load if provided AND not already loaded
    if (assets && !sLoader.isAnimationRegistryLoaded()) {
        CULog("[ENEMY INIT] Loading animation registry from assets...");
        if (!sLoader.loadAnimationRegistry(assets)) {
            CULog("[ENEMY INIT] WARNING: Failed to load animation registry, continuing anyway");
            // Non-fatal - continue
        }
    }

    // Load enemy definitions once (static pattern same as non-assets version)
    if (!sLoaded) {
        CULog("[ENEMY INIT] Loading enemy definitions from '%s'", jsonPath.c_str());
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
    
    // Log animation metadata for debugging
    for (const auto& [state, stateDef] : def.states) {
        if (stateDef.frameCount > 0) {
            CULog("[ENEMY INIT] State '%s' has %d frames (buildup: %d), duration: %.2f sec per frame",
                  stateDef.name.c_str(), stateDef.frameCount, stateDef.buildupFrameCount, stateDef.frameDuration);
        } else {
            CULog("[ENEMY INIT] State '%s' has NO frame data (frameCount=0)", stateDef.name.c_str());
        }
    }
    
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
    auto it = animationRegistry.find(stateDef->animationKey);
    if (it == animationRegistry.end()) {
        return false;  // Animation not found in registry
    }
    
    const auto& animEntry = it->second;
    
    // Calculate buildup duration in seconds
    float buildupDuration = animEntry.buildupFrameCount * animEntry.frameDuration;
    
    // In attack phase if we've elapsed past the buildup phase
    // (If buildupFrameCount == frameCount, this will never be true since animation completes before it)
    return _stateTime >= buildupDuration;
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
    // Only reset stateTime if actually changing states
    // If staying in the same state (like IDLE -> IDLE), keep accumulating time
    if (_currentState != state) {
        _stateTime = 0.0f;
        _eventsFiredThisState = false;
    }
    _currentState = state;
}

/** Updates timers.*/
void Enemy::tick(float dt) {
    if (dt <= 0.0f) return;
    _stateTime += dt;
    _attackLockout = (_attackLockout - dt < 0.0f) ? 0.0f : _attackLockout - dt;
}

/** Returns true when the animation has fully completed and events have not yet fired.
 * Checks if the current animation frame has reached the final frame.
 * For non-animated states, fires at buildUpTime immediately. */
bool Enemy::readyToFire() const {
    const EnemyLoader::StateDef* st = getCurrentStateDef();
    if (!st) return false;
    if (_eventsFiredThisState) return false;
    
    // For states without animation metadata or with no attack phase, use buildUpTime
    if (st->frameCount <= 0) {
        return _stateTime >= st->buildUpTime;
    }
    
    // For animated states, check if we've reached the final frame
    // Frames are 0-indexed, so last frame is at frameCount - 1
    // Fire when we reach or pass the final frame index
    bool shouldFire = _currentAnimationFrame >= (st->frameCount - 1);
    
    static int logCounter = 0;
    if (logCounter++ % 5 == 0) {  // Log every 5 calls
        CULog("[FIRE DEBUG] currentFrame=%d frameCount=%d lastFrameIdx=%d shouldFire=%d", 
              _currentAnimationFrame, st->frameCount, (st->frameCount - 1), shouldFire ? 1 : 0);
    }
    
    return shouldFire;
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
    CULog(
        "[Enemy]: Damage Calculation. PlayerIndex: %d | TargetIndex: %d | RelativeIndex: %d | "
        "BaseDamage: %f | Multiplier: %f | FinalDamage: %f",
        playerIndex,
        _targetIndex,
        relativeIndex,
        damage,
        multiplier,
        damage * multiplier
    );


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