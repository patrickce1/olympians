#include "Cyclops.h"

/** Override version of Enemy's init method, where custom data can be initialized
 * @param enemyID represents the name/id of the boss we are trying to get the data for
 * @param jsonPath is the path to the enemies.json file
 * 
 */
bool Cyclops::init(const std::string& enemyId, const std::string& jsonPath) {
	bool success = Enemy::init("cyclops", jsonPath);
	//Extract and assign the defense thresholds
	_defenseThreshold1 = Enemy::getMaxHealth() * _customData->getFloat("defenseThreshold1", 1.0f);
	_defenseThreshold2 = Enemy::getMaxHealth() * _customData->getFloat("defenseThreshold2", 1.0f);
	return success;
}

/** Override of the enemy update method for custom logic
 * @param dt is the time that passed from the last time update was called
 */
void Cyclops::update(float dt) {
	Enemy::update(dt);
	//end the current state ASAP to be able to enter defense if the condition was met
	bool threshold1Met = Enemy::getCurrentHealth() < _defenseThreshold1 && !_defenseTriggered1;
	bool threshold2Met = Enemy::getCurrentHealth() < _defenseThreshold2 && !_defenseTriggered2;
	if (threshold1Met || threshold2Met) {
		if (_debug) {
			CULog("[Cyclops]: Entering defense as soon as possible, currently in %s", Enemy::getStates().at(Enemy::getCurrentState()).name.c_str());
		}
		Enemy::setStateTime(Enemy::getStates().at(Enemy::getCurrentState()).buildUpTime);
		//necessary to skip the idle
		Enemy::skipCooldown();
	}
}

/** Defines the cyclops' custom behavior for when he chooses to defend 
  * Triggers if either of damage thresholds are reached
  */
bool Cyclops::shouldDefend() {
	if (Enemy::getCurrentHealth() < _defenseThreshold1 && !_defenseTriggered1) {
		if (_debug) {
			CULog("[Cyclops]: defense threshold 1 at health %f", Enemy::getCurrentHealth());
		}
		_defense1Triggered = true;
		Enemy::setStateTime(Enemy::getStates().at(EnemyLoader::State::ATTACK_3).buildUpTime);
		return true;
	}
	if (Enemy::getCurrentHealth() < _defenseThreshold2 && !_defenseTriggered2) {
		if (_debug) {
			CULog("[Cyclops]: defense threshold 2 at health %f", Enemy::getCurrentHealth());
		}
		_defense2Triggered = true;
		Enemy::setStateTime(Enemy::getStates().at(EnemyLoader::State::ATTACK_3).buildUpTime);
		return true;
	}
	return false;
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
		//Make Cyclops face whoever hit him
		Enemy::setTargetIndex(playerIndex);
		//Set the timer to the end of the attack, so that it triggers
		Enemy::setStateTime(Enemy::getStates().at(EnemyLoader::State::ATTACK_3).buildUpTime);
		//Debug statement
		if (_debug) {
			CULog("[Cyclops]: Took damage while in boulder toss. This attack should hit player %d", playerIndex);
		}
	}

	Enemy::takeDamage(damage, playerIndex);
}