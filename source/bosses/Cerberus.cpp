#include "Cerberus.h"

/**
 * Reads all per-boss configuration values from _customData and initializes head thresholds.
 * Called by both init() overloads immediately after the base Enemy is fully initialized.
 */
void Cerberus::loadCustomData() {
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
        CULog("[Cerberus] knockThresh=%.1f knockDur=%.1f regen=%.1f franticRate=%.2f",
              _maxKnockedThreshold, _knockedDuration, _knockedThresholdRegen, _franticRate);
    }
}

/**
 * Initializes Cerberus without asset manager support.
 * Delegates to Enemy::init and then loads boss-specific configuration from customData.
 *
 * @param enemyId   The unique enemy ID (should be "cerberus").
 * @param jsonPath  Path to enemies.json.
 * @return          True if initialization succeeds, false on error.
 */
bool Cerberus::init(const std::string& enemyId, const std::string& jsonPath) {
    // True if the base Enemy initialized successfully; propagated to the caller.
    bool success = Enemy::init("cerberus", jsonPath);
    loadCustomData();
    return success;
}

/**
 * Initializes Cerberus with animation metadata from the AssetManager.
 * Delegates to Enemy::init and then loads boss-specific configuration from customData.
 *
 * @param enemyId   The unique enemy ID (should be "cerberus").
 * @param jsonPath  Path to enemies.json.
 * @param assets    AssetManager containing enemyAnimations.json.
 * @return          True if initialization succeeds, false on error.
 */
bool Cerberus::init(const std::string& enemyId, const std::string& jsonPath, const std::shared_ptr<cugl::AssetManager>& assets) {
    // True if the base Enemy initialized successfully; propagated to the caller.
    bool success = Enemy::init("cerberus", jsonPath, assets);
    loadCustomData();
    return success;
}

/**
 * Per-frame update. Ticks knocked timers (recovering heads when expired), regenerates
 * knock thresholds for active heads, triggers corrosive on spit attack entry, ticks the
 * corrosive debuff, and accelerates the IDLE state cooldown when frantic thresholds are crossed.
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


    Enemy::update(dt);

    // Shorten IDLE cooldowns when frantic — attack and defense durations are unaffected
    if (getCurrentState() == EnemyLoader::State::IDLE) {
        advanceStateTime(franticExtraTime);
    }
}

/**
 * Handles incoming player damage. Reduces the struck head's knock threshold and delegates
 * to Enemy::takeDamage for side-multiplier application and health reduction.
 *
 * Special case — all-heads-knocked window: if all three heads are simultaneously knocked
 * and the attacker hits any non-back position, the 5x side multiplier (set by knockHead)
 * applies naturally, then all heads are woken and the boss returns to idle.
 *
 * Special case — drain-shield defense: the negative side multiplier set by the defense
 * state's entryEvents converts incoming damage into healing inside Enemy::takeDamage.
 *
 * @param damage       Raw damage before side multipliers are applied.
 * @param playerIndex  Slot index of the attacking player.
 */
void Cerberus::takeDamage(float damage, int playerIndex) {
    // Map the attacking player's absolute slot to a relative head position.
    // relativeSlot 2 is the back position, which has no physical head.
    int attackerRelativeSlot = (playerIndex - getTargetIndex() + 4) % 4;

    if (attackerRelativeSlot != 2 && allHeadsKnocked()) {
        // All-heads-knocked window: the 5x side multiplier was already set by knockHead,
        // so Enemy::takeDamage applies it naturally and the popup shows it automatically.
        // After the hit: reset all sides to 1.0, wake all heads, and return to idle.
        Enemy::takeDamage(damage, playerIndex);
        for (int side = 0; side < 4; side++) setSideMultiplier(side, 1.0f);
        for (int headIndex = 0; headIndex < 3; headIndex++) unKnockHead(headIndex);
        forceIdle(0.0f);
        CULog("[Cerberus] All-heads-knocked window triggered — %.1fx side multiplier applied, all heads woken", ALL_HEADS_KNOCKED_MULTIPLIER);
        return;
    }

    // The drain-shield state reverses damage to healing, so head knock thresholds
    // are not reduced — players are not actually dealing damage during this state.
    if (attackerRelativeSlot != 2 && getCurrentState() != EnemyLoader::State::DEFENSE_MOVE) {
        // Left side (relative slot 3) maps to head index 2; all other non-back slots map directly.
        int headArrayIndex = (attackerRelativeSlot == 3) ? 2 : attackerRelativeSlot;
        if (!_heads[headArrayIndex].knocked) {
            _heads[headArrayIndex].knockedThreshold -= damage;
            if (_heads[headArrayIndex].knockedThreshold < 0.0f) knockHead(headArrayIndex);
        }
    }

    Enemy::takeDamage(damage, playerIndex);
}

/**
 * Marks a one-shot corrosive drain as pending for the given player.
 * GameScene polls shouldDrainItem() once per frame; the flag is consumed on the first true return.
 */
void Cerberus::markCorrosive(int playerIndex, float fadeDuration, float fadeVariance, int maxAffected) {
    _corrosiveTarget       = playerIndex;
    _corrosiveFadeDuration = (fadeDuration > 0.0f) ? fadeDuration : 0.9f;
    _corrosiveFadeVariance = (fadeVariance > 0.0f) ? fadeVariance : 0.3f;
    _corrosiveMaxAffected  = maxAffected;
    _shouldDrain           = true;
    if (_debug) CULog("[Cerberus] markCorrosive: player=%d, fade=%.2f±%.0f%%, max=%d",
                      playerIndex, _corrosiveFadeDuration, _corrosiveFadeVariance * 100.0f, _corrosiveMaxAffected);
}

/**
 * Returns true (and clears the flag) if a corrosive drain is pending this frame.
 */
bool Cerberus::shouldDrainItem() {
    if (_shouldDrain) {
        _shouldDrain = false;
        return true;
    }
    return false;
}

/**
 * Marks the head at the given array index as knocked and starts its recovery timer.
 * If this knock causes all three heads to be downed simultaneously, sets the 5×
 * damage multiplier on all non-back sides to signal the vulnerability window.
 *
 * @param headArrayIndex  Index into _heads[] (0=main, 1=right, 2=left).
 */
void Cerberus::knockHead(int headArrayIndex) {
    _heads[headArrayIndex].knocked = true;
    _heads[headArrayIndex].knockedTimer = _knockedDuration;
    _headKnockSoundPending = true;
    if (_debug) CULog("[Cerberus] Head %d knocked for %.1fs", headArrayIndex, _knockedDuration);

    if (allHeadsKnocked()) {
        // All three heads are now down — boost all non-back sides to signal the vulnerability window.
        setSideMultiplier(0, ALL_HEADS_KNOCKED_MULTIPLIER);
        setSideMultiplier(1, ALL_HEADS_KNOCKED_MULTIPLIER);
        setSideMultiplier(3, ALL_HEADS_KNOCKED_MULTIPLIER);
        if (_debug) CULog("[Cerberus] All heads knocked — %.1fx side multiplier active on all non-back sides", ALL_HEADS_KNOCKED_MULTIPLIER);
    }
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
    if (_debug) CULog("[Cerberus] Head %d recovered", headArrayIndex);
}
