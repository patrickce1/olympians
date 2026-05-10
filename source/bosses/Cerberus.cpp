#include "Cerberus.h"

/**
 * Reads all per-boss configuration values from _customData and initializes head thresholds.
 * Called by both init() overloads immediately after the base Enemy is fully initialized.
 */
void Cerberus::loadCustomData() {
    _lifeStealPercent      = _customData->getFloat("lifeStealPercent");
    _maxKnockedThreshold   = _customData->getFloat("knockedThreshold", 100.0f);
    _knockedDuration       = _customData->getFloat("knockedDuration", 4.0f);
    _knockedThresholdRegen = _customData->getFloat("knockedThresholdRegen", 10.0f);
    _frantic1Threshold     = getMaxHealth() * _customData->getFloat("frantic1Threshold", 1.0f);
    _frantic2Threshold     = getMaxHealth() * _customData->getFloat("frantic2Threshold", 1.0f);
    _franticRate           = _customData->getFloat("franticRate", 0.0f);
    for (int headIndex = 0; headIndex < 3; headIndex++) {
        _heads[headIndex].knockedThreshold = _maxKnockedThreshold;
    }
    if (_debug) {
        CULog("[Cerberus] lifeSteal=%.2f knockThresh=%.1f knockDur=%.1f regen=%.1f franticRate=%.2f",
              _lifeStealPercent, _maxKnockedThreshold, _knockedDuration, _knockedThresholdRegen, _franticRate);
    }
}

bool Cerberus::init(const std::string& enemyId, const std::string& jsonPath) {
    bool success = Enemy::init("cerberus", jsonPath);
    loadCustomData();
    return success;
}

bool Cerberus::init(const std::string& enemyId, const std::string& jsonPath, const std::shared_ptr<cugl::AssetManager>& assets) {
    bool success = Enemy::init("cerberus", jsonPath, assets);
    loadCustomData();
    return success;
}

/**
 * Per-frame update. Ticks knocked timers (recovering heads when expired), regenerates
 * knock thresholds for active heads, ticks the corrosive debuff, and accelerates the
 * IDLE state cooldown when one or both frantic thresholds are crossed.
 *
 * @param dt  Elapsed time in seconds since the last update.
 */
void Cerberus::update(float dt) {
    // Accumulate extra idle time for each active frantic tier before updating base state
    float franticExtraTime = 0.0f;
    if (getCurrentHealth() < _frantic1Threshold) franticExtraTime += dt * _franticRate;
    if (getCurrentHealth() < _frantic2Threshold) franticExtraTime += dt * _franticRate;

    for (int headIndex = 0; headIndex < 3; headIndex++) {
        if (_heads[headIndex].knocked) {
            _heads[headIndex].knockedTimer -= dt;
            if (_heads[headIndex].knockedTimer <= 0.0f) unKnockHead(headIndex);
        } else {
            _heads[headIndex].knockedThreshold = std::min(
                _maxKnockedThreshold,
                _heads[headIndex].knockedThreshold + _knockedThresholdRegen * dt);
        }
    }

    if (_corrosiveActive) {
        _corrosiveTimer -= dt;
        if (_corrosiveTimer <= 0.0f) {
            _corrosiveTimer = 0.0f;
            _corrosiveActive = false;
        } else {
            _corrosiveDrainAccum -= dt;
            if (_corrosiveDrainAccum <= 0.0f) {
                _corrosiveDrainAccum = CORROSIVE_DRAIN_INTERVAL;
                applyCorrosive();
            }
        }
    }

    Enemy::update(dt);

    // Shorten IDLE cooldowns when frantic — attack and defense durations are unaffected
    if (getCurrentState() == EnemyLoader::State::IDLE) {
        advanceStateTime(franticExtraTime);
    }
}

/**
 * Handles incoming player damage. Heals Cerberus by the life-steal fraction, then
 * reduces the struck head's knock threshold. If the threshold reaches zero the head
 * is knocked. Delegates to Enemy::takeDamage for side-multiplier application and
 * health reduction.
 *
 * Special case — all-heads-knocked window: if all three heads are simultaneously
 * knocked and the attacker hits any non-back position, the hit deals
 * ALL_HEADS_KNOCKED_MULTIPLIER times the normal damage and immediately wakes all
 * heads. Hits to the back position during this window have no special effect.
 *
 * @param damage       Raw damage before side multipliers are applied.
 * @param playerIndex  Slot index of the attacking player.
 */
void Cerberus::takeDamage(float damage, int playerIndex) {
    // Map the attacking player's absolute slot to a relative head position.
    // relativeSlot 2 is the back position, which has no physical head.
    int attackerRelativeSlot = (playerIndex - getTargetIndex() + 4) % 4;

    if (attackerRelativeSlot != 2 && allHeadsKnocked()) {
        // All-heads-knocked window: apply multiplier, heal life-steal on the boosted amount,
        // wake all heads, and skip the normal threshold logic.
        float boostedDamage = damage * ALL_HEADS_KNOCKED_MULTIPLIER;
        updateHealth(boostedDamage * _lifeStealPercent);
        for (int headIndex = 0; headIndex < 3; headIndex++) unKnockHead(headIndex);
        Enemy::takeDamage(boostedDamage, playerIndex);
        CULog("[Cerberus] All-heads-knocked window triggered — %.1fx damage, all heads woken", ALL_HEADS_KNOCKED_MULTIPLIER);
        return;
    }

    updateHealth(damage * _lifeStealPercent);

    if (attackerRelativeSlot != 2) {
        int headArrayIndex = (attackerRelativeSlot == 3) ? 2 : attackerRelativeSlot;
        if (!_heads[headArrayIndex].knocked) {
            _heads[headArrayIndex].knockedThreshold -= damage;
            if (_heads[headArrayIndex].knockedThreshold < 0.0f) knockHead(headArrayIndex);
        }
    }

    Enemy::takeDamage(damage, playerIndex);
}

/** Placeholder: drains one item from the corrosive target's inventory. Not yet implemented. */
void Cerberus::applyCorrosive() {
    // Stub: full corrosive inventory drain to be implemented with game scene integration
}

/**
 * Marks the head at the given array index as knocked and starts its recovery timer.
 *
 * @param headArrayIndex  Index into _heads[] (0=main, 1=right, 2=left).
 */
void Cerberus::knockHead(int headArrayIndex) {
    _heads[headArrayIndex].knocked = true;
    _heads[headArrayIndex].knockedTimer = _knockedDuration;
    CULog("[Cerberus] Head %d knocked for %.1fs", headArrayIndex, _knockedDuration);
}

/**
 * Clears the knocked state for the head at the given array index and resets its
 * knock threshold to the maximum so the head must be worn down again.
 *
 * @param headArrayIndex  Index into _heads[] (0=main, 1=right, 2=left).
 */
void Cerberus::unKnockHead(int headArrayIndex) {
    _heads[headArrayIndex].knocked = false;
    _heads[headArrayIndex].knockedTimer = 0.0f;
    _heads[headArrayIndex].knockedThreshold = _maxKnockedThreshold;
    CULog("[Cerberus] Head %d recovered", headArrayIndex);
}
