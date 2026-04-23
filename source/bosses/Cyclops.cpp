#include "Cyclops.h"

/** Override version of Enemy's init method, where custom data can be initialized
 * @param enemyID represents the name/id of the boss we are trying to get the data for
 * @param jsonPath is the path to the enemies.json file
 * 
 */
bool Cyclops::init(const std::string& enemyId, const std::string& jsonPath) {
	bool success = Enemy::init("cyclops", jsonPath);
	//Extract and assign the defense thresholds
	_frantic1Threshold = Enemy::getMaxHealth() * _customData->getFloat("frantic1Threshold", 1.0f);
	_frantic2Threshold = Enemy::getMaxHealth() * _customData->getFloat("frantic2Threshold", 1.0f);
	_franticRate = _customData->getFloat("franticRate", 1.0f);
	_boulderTossReductionAmount = _customData->getFloat("boulderTossReductionAmount", 1.0f);
	_boulderHigherBound = _customData->getFloat("boulderHigherBound", 1.0f);
	if(_debug) CULog("[Cyclops]: Initialized with frantic rate %f, frantic threshold %f, frantic threshold 2 %f", _franticRate, _frantic1Threshold, _frantic2Threshold);
	return success;
}

/** Initializes the cyclops with animation metadata from AssetManager.
 * This version uses smart caching to load animation registry only when needed.
 * Prefers this method when assets are available to ensure proper animation setup.
 * This also initializes all custom data that the cyclops uses
 *
 * @param enemyId The unique ID of the enemy to load (e.g., "cyclops")
 * @param jsonPath Path to enemies.json configuration file
 * @param assets AssetManager containing enemyAnimations.json and other asset definitions
 * @return true if initialization succeeds, false on error
 */
bool Cyclops::init(const std::string& enemyId, const std::string& jsonPath, const std::shared_ptr<cugl::AssetManager>& assets) {
	bool success = Enemy::init("cyclops", jsonPath, assets);
	//Extract and assign the defense thresholds
	_frantic1Threshold = Enemy::getMaxHealth() * _customData->getFloat("frantic1Threshold", 1.0f);
	_frantic2Threshold = Enemy::getMaxHealth() * _customData->getFloat("frantic2Threshold", 1.0f);
	_franticRate = _customData->getFloat("franticRate", 1.0f);
	_boulderTossReductionAmount = _customData->getFloat("boulderTossReductionAmount", 1.0f);
	_boulderHigherBound = _customData->getFloat("boulderHigherBound", 1.0f);
	if(_debug) CULog("[Cyclops]: Initialized with frantic rate %f, frantic threshold %f, frantic threshold 2 %f", _franticRate, _frantic1Threshold, _frantic2Threshold);
	return success;
}

/** Override of the enemy update method for custom logic
 * @param dt is the time that passed from the last time update was called
 */
void Cyclops::update(float dt) {
	//Cyclops becomes more frantic as his defense thresholds are met
	//His build up times are shortened
	float updatedDt = dt;
	if (Enemy::getCurrentHealth() < _frantic1Threshold) {
		updatedDt += dt * _franticRate;
	}
	if (Enemy::getCurrentHealth() < _frantic2Threshold) {
		//Passing frantic2Threshold doubles the frantic rate basically
		updatedDt += dt * _franticRate;
	}
	Enemy::update(updatedDt);
}

/* Handles taking damage and applying the side modifiers
 * Use this method instead of updateHealth() for appropriate damage multiplication
 * @param damage is the amount of damage being done to the boss
 * @param playerIndex is the index that was assigned to the player by the host
 *
 * This override primarily handles the special condition of the boulder toss attack,
 *		where the boss immidiately does damage based on the side it got hit from
*/
void Cyclops::takeDamage(float damage, int playerIndex) {
	//ATTACK_3 should correspond to the boulder toss for cyclops
	if (Enemy::_currentState == EnemyLoader::State::ATTACK_3) {
		//make sure the time increase doesn't go over the boundary we set to prevent animation from skipping
		float skipDt = std::min(_stateTime + _boulderTossReductionAmount, _boulderHigherBound) - _stateTime;
		//Make Cyclops face whoever hit him and shorten wait time
		Enemy::setTargetIndex(playerIndex);
		//make the enemy skip the amount of time we need to skip
		Enemy::update(skipDt);

		if (_debug) CULog("[Cyclops]: Took damage while in boulder toss. This attack should hit player %d", playerIndex);
	}

	Enemy::takeDamage(damage, playerIndex);
}
