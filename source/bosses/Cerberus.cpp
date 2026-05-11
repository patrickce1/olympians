#include "Cerberus.h"

/**
 * Initializes  Cerberus without asset manager support.
 * Reads lifeStealPercent parameter from customData in enemies.json.
 *
 * @param enemyId   The unique enemy ID (should be "cerberus")
 * @param jsonPath  Path to enemies.json
 * @return true if initialization succeeds, false on error
 */
bool Cerberus::init(const std::string& enemyId, const std::string& jsonPath) {
    bool success = Enemy::init("cerberus", jsonPath);
    float headHp = _customData -> getFloat("headHealth");
    for (int i = 0; i < 3; i++){
        _headMaxHealth[i] = headHp;
        _headHealth[i]    = headHp;
        _headStunned[i]   = false;
        _headStunTimer[i] = 0.0f;
    }
    
    _isFullyStunned      = false;
    _corrosiveActive     = false;
    _corrosiveTimer      = 0.0f;
    _corrosiveTarget     = -1;
    _corrosiveDrainAccum = CORROSIVE_DRAIN_INTERVAL;
    _shouldDrain         = false;
    _previousState       = EnemyLoader::State::IDLE;

    return success;
}


/**
 * Initializes Cerberus with animation metadata from the AssetManager.
 * Reads lifeStealPercent parameter from customData in enemies.json.
 *
 * @param enemyId   The unique enemy ID (should be "cerberus")
 * @param jsonPath  Path to enemies.json
 * @param assets    AssetManager containing enemyAnimations.json
 * @return true if initialization succeeds, false on error
 */
bool Cerberus::init(const std::string& enemyId, const std::string& jsonPath, const std::shared_ptr<cugl::AssetManager>& assets) {
    bool success = Enemy::init("cerberus", jsonPath, assets);
    float headHp = _customData -> getFloat("headHealth");
    for (int i = 0; i < 3; i++){
        _headMaxHealth[i] = headHp;
        _headHealth[i]    = headHp;
        _headStunned[i]   = false;
        _headStunTimer[i] = 0.0f;
    }
    
    _isFullyStunned      = false;
    _corrosiveActive     = false;
    _corrosiveTimer      = 0.0f;
    _corrosiveTarget     = -1;
    _corrosiveDrainAccum = CORROSIVE_DRAIN_INTERVAL;
    _shouldDrain         = false;
    _previousState       = EnemyLoader::State::IDLE;

    return success;
}


/**
 * Updates Cerberus each frame, continuously applying corrosion if active.
 *
 * If corrosion is active, the affected player loses inventory items until the timer for the affected player reaches zero.
 *
 * @param dt  Elapsed time in seconds since the last update
 */
void Cerberus::update(float dt) {
    // Store previous state to detect state changes
    EnemyLoader::State currentState = getCurrentState();

    // Trigger corrosive when entering ATTACK_3 state
    if (currentState == EnemyLoader::State::ATTACK_3 && _previousState != EnemyLoader::State::ATTACK_3) {
        // Use the enemy's current target as the corrosive victim
        if (_targetIndex >= 0){
            startCorrosive(_targetIndex, CORROSIVE_DURATION);
        }
        if (_debug) CULog("Cerberus: ATTACK_3 started, triggering corrosive on player %d", _targetIndex);
    }

    _previousState = currentState;

    //Tick the head stun timers
    for (int i = 0; i < 3; i++){
        if (_headStunned[i]){
            _headStunTimer[i] -= dt;
            if (_headStunTimer[i] <= 0) {
                unstunHead(i);
            }
        }
    }
    
    //Handle corrosion if it is active on the scene
    if (_corrosiveActive) {
        if (_debug) CULog("Cerberus: Corrosive active, target=%d, timer=%.2f, accum=%.2f", _corrosiveTarget, _corrosiveTimer, _corrosiveDrainAccum);
        _corrosiveTimer -= dt;
        if (_corrosiveTimer <= 0){
            _corrosiveTimer = 0;
            _corrosiveActive = false;
            _corrosiveTarget = -1;
            _corrosiveDrainAccum = 0;
            if (_debug) CULog("Cerberus: Corrosive ended naturally (timer expired)");
        }
        else { //Accumulator for draining
            _corrosiveDrainAccum -= dt;
            if (_corrosiveDrainAccum <= 0) {
                _shouldDrain = true;
                _corrosiveDrainAccum = CORROSIVE_DRAIN_INTERVAL;
                if (_debug) CULog("Cerberus: Drain ready! shouldDrain=true, target=%d", _corrosiveTarget);
            }
        }
    }
    Enemy::update(dt);
}

/**
* Applies damage with life steal. A defined percent of the raw damage value is
* added back as healing before side multipliers are applied.
*
* @param damage       Raw damage before side multipliers
* @param playerIndex  Slot index of the attacking player
*/
void Cerberus::takeDamage(float damage, int playerIndex){
    Enemy::takeDamage(damage, playerIndex);
}


/**
 * Registers a stun on one of Cerberus's heads. If all 3 heads become
 * stunned simultaneously, triggers a full stun and plays the
 * heads-lowered animation.
 *
 * @param headIndex  Which head was stunned (0, 1, or 2)
 */
void Cerberus::stunHead(int headIndex) {
    if (headIndex < 0 || headIndex >= 3) {
        if (_debug) CULog("Cerberus: Invalid headIndex %d in stunHead()", headIndex);
        return;
    }

    _headStunned[headIndex] = true;
    _headStunTimer[headIndex] = HEAD_STUN_DURATION;

    if (_headStunned[0] && _headStunned[1] && _headStunned[2]) {
        _isFullyStunned = true;
    }
}

/**
 * Clears the stun state on a single head, called when the stun
 * duration on that head expires.
 *
 * @param headIndex  Which head to unstun (0, 1, or 2)
 */
void Cerberus::unstunHead(int headIndex) {
    if (headIndex < 0 || headIndex >= 3) {
        if (_debug) CULog("Cerberus: Invalid headIndex %d in unstunHead()", headIndex);
        return;
    }

    _headStunned[headIndex] = false;
    _headStunTimer[headIndex] = 0;
    _isFullyStunned = false;
}

/**Determines if corrosive should drain an item for the player.
 * Makes sure there are items, and that time has passed.
 */
bool Cerberus::shouldDrainItem() {
    if (_debug) CULog("Cerberus: shouldDrainItem called - _shouldDrain=%d, _corrosiveActive=%d", _shouldDrain, _corrosiveActive);
    if (_shouldDrain){
        _shouldDrain = false;
        if (_debug) CULog("Cerberus: Returning TRUE, consuming drain flag");
        return true;
    }
    return false;
}

/**
 * Applies the corrosive debuff to _corrosiveTarget for _corrosiveTimer seconds.
 * Notifies the game scene to fade tokens, show the corrosive outline,
 * and block item passing/receiving for the debuff duration.
 *
 * @param playerIndex  Slot index of the player to afflict
 * @param duration     How long the debuff lasts in seconds
 */
void Cerberus::startCorrosive(int playerIndex, float duration) {
    _corrosiveTarget = playerIndex;
    _corrosiveTimer = duration;
    _corrosiveActive = true;
    _corrosiveDrainAccum = CORROSIVE_DRAIN_INTERVAL;
    _shouldDrain = false;
    if (_debug) CULog("Cerberus: startCorrosive called - player=%d, duration=%.1f", playerIndex, duration);
}

/** Ends the corrosive effect early (e.g., when player runs out of items) */
void Cerberus::endCorrosive() {
    _corrosiveActive = false;
    _corrosiveTimer = 0.0f;
    _corrosiveTarget = -1;
    _corrosiveDrainAccum = 0.0f;
    _shouldDrain = false;
    if (_debug) CULog("Cerberus: endCorrosive called");
}
