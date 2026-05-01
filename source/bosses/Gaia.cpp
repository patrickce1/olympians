#include "Gaia.h"

/** Override version of Enemy's init method, where Gaia's custom data can be initialized
  * @param enemyID represents the name/id of the boss we are trying to get the data for
  * @param jsonPath is the path to the enemies.json file
  * @return true if initialization succeeds, false on error
  */
bool Gaia::init(const std::string& enemyId, const std::string& jsonPath) {
	bool success = Enemy::init(enemyId, jsonPath);
	_spawnCooldownConstant = _customData->getFloat("spawnCooldownConstant", 1.0f);
	_currentSpawnTime = 0.0f;
	_targetIndex = 0;
	return success;
}

/** Initializes Gaia with animation metadata from AssetManager.
  * This version uses smart caching to load animation registry only when needed.
  * Prefers this method when assets are available to ensure proper animation setup.
  * This also initializes all custom data that Gaia uses
  *
  * @param enemyId The unique ID of the enemy to load (e.g., "cyclops")
  * @param jsonPath Path to enemies.json configuration file
  * @param assets AssetManager containing enemyAnimations.json and other asset definitions
  * @return true if initialization succeeds, false on error
  */
bool Gaia::init(const std::string& enemyId, const std::string& jsonPath, const std::shared_ptr<cugl::AssetManager>& assets) {
	bool success = Enemy::init(enemyId, jsonPath, assets);
	_spawnCooldownConstant = _customData->getFloat("spawnCooldownConstant", 1.0f);
	_currentSpawnTime = 0.0f;
	_targetIndex = 0;
	return success;
}

/** Override of the enemy update method for custom logic
  * @param dt is the time that passed from the last time update was called
  * 
  * Gaia upticks her _currentSpawnTime during update
  */
void Gaia::update(float dt) {
	_currentSpawnTime += dt;
	Enemy::update(dt);
}

/** This method tells us if Gaia's timer for spawning a rock is done
  * Calling this method also resets the timer associated with the spawning
  * @return true means whatever player Gaia is facing should receive a rock in their inventory
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