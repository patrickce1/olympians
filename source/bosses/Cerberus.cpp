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
    if (_debug) CULog("[Cerberus]: LifeStealPercent=%.2f",
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
    if (_debug) CULog("[Cerberus]: LifeStealPercent=%.2f",
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
        if (_heads[i].stunned){
            _heads[i].stunTimer -= dt;
            if (_heads[i].stunTimer <= 0) {
                unstunHead(i);
            }
        }
    }
    if (_corrosiveActive) {
        _corrosiveTimer -= dt;
        if (_corrosiveTimer <= 0){
            _corrosiveTimer = 0;
            _corrosiveActive = false;
        }
        else {
            _corrosiveDrainAccum -= dt;
            if (_corrosiveDrainAccum <= 0) {
                _corrosiveDrainAccum = CORROSIVE_DRAIN_INTERVAL;
                applyCorrosive();
            }
        }
    }
    Enemy::update(dt);
}


void Cerberus::takeDamage(float damage, int playerIndex) {
    float heal = damage * _lifeStealPercent;
    updateHealth(heal);
    Enemy::takeDamage(damage, playerIndex);
}

void Cerberus::applyCorrosive() {
    // Stub: full corrosive inventory drain to be implemented with game scene integration
}

void Cerberus::stunHead(int headIndex) {
    _heads[headIndex].stunned = true;
    _heads[headIndex].stunTimer = HEAD_STUN_DURATION;

    if (_heads[0].stunned && _heads[1].stunned && _heads[2].stunned) {
        _isFullyStunned = true;
    }
}

void Cerberus::unstunHead(int headIndex) {
    _heads[headIndex].stunned = false;
    _heads[headIndex].stunTimer = 0;
    _isFullyStunned = false;
}
