#include "Cyclops.h"

/**
 * Initializes the Cyclops without asset manager support.
 * Reads frantic hit thresholds and boulder-toss parameters from customData in enemies.json.
 *
 * @param enemyId   The unique enemy ID (should be "cyclops")
 * @param jsonPath  Path to enemies.json
 * @return true if initialization succeeds, false on error
 */
bool Cyclops::init(const std::string& enemyId, const std::string& jsonPath) {
	bool success = Enemy::init("cyclops", jsonPath);
	_frantic1Threshold        = _customData->getInt("frantic1Threshold", 0);
	_frantic2Threshold        = _customData->getInt("frantic2Threshold", 0);
	_franticRate              = _customData->getFloat("franticRate", 1.0f);
	_boulderTossReductionAmount = _customData->getFloat("boulderTossReductionAmount", 1.0f);
	if (_debug) CULog("[Cyclops]: franticRate=%.2f frantic1=%d frantic2=%d",
	                  _franticRate, _frantic1Threshold, _frantic2Threshold);
	return success;
}

/**
 * Initializes the Cyclops with animation metadata from the AssetManager.
 * Reads frantic hit thresholds and boulder-toss parameters from customData in enemies.json.
 *
 * @param enemyId   The unique enemy ID (should be "cyclops")
 * @param jsonPath  Path to enemies.json
 * @param assets    AssetManager containing enemyAnimations.json
 * @return true if initialization succeeds, false on error
 */
bool Cyclops::init(const std::string& enemyId, const std::string& jsonPath, const std::shared_ptr<cugl::AssetManager>& assets) {
	bool success = Enemy::init("cyclops", jsonPath, assets);
	_frantic1Threshold        = _customData->getInt("frantic1Threshold", 0);
	_frantic2Threshold        = _customData->getInt("frantic2Threshold", 0);
	_franticRate              = _customData->getFloat("franticRate", 1.0f);
	_boulderTossReductionAmount = _customData->getFloat("boulderTossReductionAmount", 1.0f);
	if (_debug) CULog("[Cyclops]: franticRate=%.2f frantic1=%d frantic2=%d",
	                  _franticRate, _frantic1Threshold, _frantic2Threshold);
	return success;
}

/**
 * Updates the Cyclops each frame, applying frantic behavior when hit-count thresholds are reached.
 *
 * Each frantic threshold adds one extra tick of time during IDLE so cooldowns between
 * attacks shorten — but attack wind-ups and defense duration are unaffected.
 * Crossing both thresholds stacks the effect.
 *
 * @param dt  Elapsed time in seconds since the last update
 */
void Cyclops::update(float dt) {
	float franticDt = 0.0f;
	if (_frantic1Threshold > 0 && Enemy::getHitCount() >= _frantic1Threshold) {
		franticDt += dt * _franticRate;
	}
	if (_frantic2Threshold > 0 && Enemy::getHitCount() >= _frantic2Threshold) {
		franticDt += dt * _franticRate;
	}

	Enemy::update(dt);

	// Only accelerate IDLE so cooldowns shorten, not attack or defense states
	if (Enemy::getCurrentState() == EnemyLoader::State::IDLE) {
		Enemy::advanceStateTime(franticDt);
	}
}

/**
 * Handles incoming damage with boulder-toss interaction logic.
 *
 * During a boulder-toss wind-up (ATTACK_3 buildup phase), each hit:
 *   - Retargets the Cyclops toward the attacker
 *   - Advances the wind-up timer by boulderTossReductionAmount (capped so that
 *     at least one full buildup loop always remains)
 *   - If less than one buildup loop remains, resets the wind-up to frame 0 instead
 *
 * @param damage       Raw damage amount before side multipliers
 * @param playerIndex  Slot index of the attacking player
 */
void Cyclops::takeDamage(float damage, int playerIndex) {
	if (Enemy::getCurrentState() == EnemyLoader::State::ATTACK_3) {
		if (!Enemy::isInAttackAnimationPhase()) {
			Enemy::setTargetIndex(playerIndex);
		}

		const auto* stateDef = Enemy::getCurrentStateDef();
		if (stateDef && _stateTime < stateDef->buildUpTime) {
			int loopFrameCount    = stateDef->loopEndFrame - stateDef->loopStartFrame + 1;
			float oneLoopDuration = loopFrameCount * stateDef->frameDuration;
			float safeZoneEnd     = stateDef->buildUpTime - oneLoopDuration;

			if (safeZoneEnd <= 0.0f || _stateTime >= safeZoneEnd) {
				// Less than one buildup loop remains — restart loop from the first loop frame
				Enemy::setStateTime(stateDef->loopStartFrame * stateDef->frameDuration);
			} else {
				float timeToAdvance = std::min(_stateTime + _boulderTossReductionAmount, safeZoneEnd) - _stateTime;
				Enemy::advanceStateTime(timeToAdvance);
			}
		}
		if (_debug) CULog("[Cyclops]: Hit during boulder toss — retargeting to player %d", playerIndex);
	}

	Enemy::takeDamage(damage, playerIndex);
}
