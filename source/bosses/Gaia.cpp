#include "Gaia.h"

/** Override version of Enemy's init method, where custom data can be initialized
  * @param enemyID represents the name/id of the boss we are trying to get the data for
  * @param jsonPath is the path to the enemies.json file
  */
bool Gaia::init(const std::string& enemyId, const std::string& jsonPath) {
	Enemy::init(enemyId, jsonPath);
	_spawnCooldownConstant = _customData->getFloat("spawnCooldownConstant", 1.0f);
	_currentSpawnTime = 0.0f;
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
bool Gaia::init(const std::string& enemyId, const std::string& jsonPath, const std::shared_ptr<cugl::AssetManager>& assets) {
	Enemy::init(enemyId, jsonPath, assets);
	_spawnCooldownConstant = _customData->getFloat("spawnCooldownConstant", 1.0f);
	_currentSpawnTime = 0.0f;
}

/** Override of the enemy update method for custom logic
  * @param dt is the time that passed from the last time update was called
  *
  * The boss attacks immidately upon the defense thesholds being triggered
  * Certain defense thresholds being met also makes the boss more frantic, switching states more often
  */
void Gaia::update(float dt) {
	_currentSpawnTime += dt;
	Enemy::update(dt);
}

/** This method tells us if Gaia's timer for spawning a rock is done
  * The idea is that if the boolean is true, whatever player Gaia is facing will recieve the rock into their hand
  * Resets the timer associated with the spawning
  */
bool Gaia::spawnRockForPlayer() {
	if (_currentSpawnTime >= _spawnCooldownConstant) {
		_currentSpawnTime = 0.0f;
		return true;
	}
	else {
		return false;
	}
}