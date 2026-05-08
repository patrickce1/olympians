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
    _lifeStealPercent        =  _customData->getFloat("lifeStealPercent");
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
    _lifeStealPercent        =  _customData->getFloat("lifeStealPercent");
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
    
    return success;
}


/**
 * Updates Cerberus each frame, continously applying corrosion if active..
 *
 * If corrosion is active, the affected player loses inventory items until the timer for the affected player reaches zero.
 *
 * @param dt  Elapsed time in seconds since the last update
 */
void Cerberus::update(float dt) {
    // Store previous state to detect state changes
    static EnemyLoader::State previousState = EnemyLoader::State::IDLE;
    EnemyLoader::State currentState = getCurrentState();

    // Trigger corrosive when entering ATTACK_3 state
    if (currentState == EnemyLoader::State::ATTACK_3 && previousState != EnemyLoader::State::ATTACK_3) {
        // Always target player 0 (local player) for testing
        startCorrosive(0, CORROSIVE_DURATION);
        CULog("Cerberus: ATTACK_3 started, triggering corrosive on player 0 (YOU)");
    }
    previousState = currentState;

    //Tick the head stun timers
    for (int i = 0; i < 3; i++){
        if (_headStunned[i]){
            _headStunTimer[i] -= dt;
            if (_headStunTimer[i] <= 0) {
                unstunHead(i);
            }
        }
    }
    if (_corrosiveActive) {
        CULog("Cerberus: Corrosive active, timer=%.2f, accum=%.2f", _corrosiveTimer, _corrosiveDrainAccum);
        _corrosiveTimer -= dt;
        if (_corrosiveTimer <= 0){
            _corrosiveTimer = 0;
            _corrosiveActive = false;
            _corrosiveTarget = -1;
            _corrosiveDrainAccum = 0;
            CULog("Cerberus: Corrosive ended");
        }
        else {
            _corrosiveDrainAccum -= dt;
            CULog("Cerberus: Accum ticking: %.3f (will drain at 0)", _corrosiveDrainAccum);
            if (_corrosiveDrainAccum <= 0) {
                _shouldDrain = true;
                _corrosiveDrainAccum += CORROSIVE_DRAIN_INTERVAL; // reset by adding interval (more accurate)
                CULog("Cerberus: Setting _shouldDrain to TRUE, resetting accum to %.3f", _corrosiveDrainAccum);
            }
        }
    }
    Enemy::update(dt);
}

/**
 * Registers a stun on one of Cerberus's heads. If all 3 heads become
 * stunned simultaneously, triggers a full stun and plays the
 * heads-lowered animation.
 *
 * @param headIndex  Which head was stunned (0, 1, or 2)
 */
void Cerberus::stunHead(int headIndex) {
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
    _headStunned[headIndex] = false;
    _headStunTimer[headIndex] = 0;
    _isFullyStunned = false;
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
    CULog("Cerberus: startCorrosive called - player=%d, duration=%.1f", playerIndex, duration);
}

bool Cerberus::shouldDrainItem() {
    CULog("Cerberus: shouldDrainItem called - _shouldDrain=%d, _corrosiveActive=%d", _shouldDrain, _corrosiveActive);
    if (_shouldDrain){
        _shouldDrain = false;
        CULog("Cerberus: Returning TRUE, consuming drain flag");
        return true;
    }
    return false;
}

void Cerberus::takeDamage(float damage, int playerIndex){
    // TODO: Implement life steal logic (80% of damage converted to healing)
    Enemy::takeDamage(damage, playerIndex);
}

void Cerberus::endCorrosive() {
    _corrosiveActive = false;
    _corrosiveTimer = 0.0f;
    _corrosiveTarget = -1;
    _corrosiveDrainAccum = 0.0f;
    _shouldDrain = false;
    CULog("Cerberus: endCorrosive called - corrosive terminated early");
}
