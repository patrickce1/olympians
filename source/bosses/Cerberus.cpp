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
    
    if (_debug) CULog("[Cerberus]: LifeStealPercent=%.2f,
                      _lifeStealPercent);
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
    
    if (_debug) CULog("[Cerberus]: LifeStealPercent=%.2f,
                      _lifeStealPercent);
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
            _corrosiveTimer -= dt;
            if (_corrosiveTimer <= 0){
                _corrosiveTimer = 0;
                _corrosiveActive = false;
                _corrosiveTarget = -1;
            }
            else {
                _corrosiveDrainAccum -= dt;
                if (_corrosiveDrainAccum <= 0) {
                    _corrosiveDrainAccum = CORROSIVE_DRAIN_INTERVAL; // reset
                    applyCorrosion();
            }
        }
        Enemy::update(dt);
        
}


void Cerberus::stunHead(int headIndex) {
    _headStunned[headIndex] = true;
    _headStunTimer[headIndex] = HEAD_STUN_DURATION;

    if (_headStunned[0] && _headStunned[1] && _headStunned[2]) {
        _isFullyStunned = true;
    }
}

void Cerberus::unstunHead(int headIndex) {
    _headStunned[headIndex] = false;
    _headStunTimer[headIndex] = 0;
    _isFullyStunned = false;
}
