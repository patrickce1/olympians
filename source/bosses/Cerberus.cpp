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
    _lifeStealPercent       = _customData->getFloat("lifeStealPercent");
    _maxKnockedThreshold    = _customData->getFloat("knockedThreshold", 50.0f);
    _knockedDuration        = _customData->getFloat("knockedDuration", 4.0f);
    _knockedThresholdRegen  = _customData->getFloat("knockedThresholdRegen", 10.0f);
    _frantic1Threshold      = getMaxHealth() * _customData->getFloat("frantic1Threshold", 1.0f);
    _frantic2Threshold      = getMaxHealth() * _customData->getFloat("frantic2Threshold", 1.0f);
    _franticRate            = _customData->getFloat("franticRate", 0.0f);
    for (int i = 0; i < 3; i++) _heads[i].knockedThreshold = _maxKnockedThreshold;
    if (_debug) CULog("[Cerberus]: LifeStealPercent=%.2f knockedThreshold=%.1f knockedDuration=%.1f regen=%.1f franticRate=%.2f",
                      _lifeStealPercent, _maxKnockedThreshold, _knockedDuration, _knockedThresholdRegen, _franticRate);
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
    _lifeStealPercent       = _customData->getFloat("lifeStealPercent");
    _maxKnockedThreshold    = _customData->getFloat("knockedThreshold", 50.0f);
    _knockedDuration        = _customData->getFloat("knockedDuration", 4.0f);
    _knockedThresholdRegen  = _customData->getFloat("knockedThresholdRegen", 10.0f);
    for (int i = 0; i < 3; i++) _heads[i].knockedThreshold = _maxKnockedThreshold;
    if (_debug) CULog("[Cerberus]: LifeStealPercent=%.2f knockedThreshold=%.1f knockedDuration=%.1f regen=%.1f",
                      _lifeStealPercent, _maxKnockedThreshold, _knockedDuration, _knockedThresholdRegen);
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
    float franticDt = 0.0f;
    if (getCurrentHealth() < _frantic1Threshold) franticDt += dt * _franticRate;
    if (getCurrentHealth() < _frantic2Threshold) franticDt += dt * _franticRate;

    for (int i = 0; i < 3; i++) {
        if (_heads[i].knocked) {
            _heads[i].knockedTimer -= dt;
            if (_heads[i].knockedTimer <= 0) unKnockHead(i);
        } else {
            _heads[i].knockedThreshold = std::min(_maxKnockedThreshold,
                _heads[i].knockedThreshold + _knockedThresholdRegen * dt);
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

    // Shorten IDLE cooldowns when frantic — attack and defense durations are unaffected
    if (getCurrentState() == EnemyLoader::State::IDLE) {
        advanceStateTime(franticDt);
    }
}


void Cerberus::takeDamage(float damage, int playerIndex) {
    float heal = damage * _lifeStealPercent;
    updateHealth(heal);

    int relPos4 = (playerIndex - getTargetIndex() + 4) % 4;
    if (relPos4 != 2) {  // back position has no head
        int headIdx = (relPos4 == 3) ? 2 : relPos4;
        if (!_heads[headIdx].knocked) {
            _heads[headIdx].knockedThreshold -= damage;
            if (_heads[headIdx].knockedThreshold < 0) knockHead(headIdx);
        }
    }

    Enemy::takeDamage(damage, playerIndex);
}

void Cerberus::applyCorrosive() {
    // Stub: full corrosive inventory drain to be implemented with game scene integration
}

void Cerberus::knockHead(int playerSlot) {
    _heads[playerSlot].knocked = true;
    _heads[playerSlot].knockedTimer = _knockedDuration;
    CULog("[Cerberus] Head %d knocked for %.1fs", playerSlot, _knockedDuration);
}

void Cerberus::unKnockHead(int playerSlot) {
    _heads[playerSlot].knocked = false;
    _heads[playerSlot].knockedTimer = 0.0f;
    _heads[playerSlot].knockedThreshold = _maxKnockedThreshold;
    CULog("[Cerberus] Head %d recovered", playerSlot);
}
