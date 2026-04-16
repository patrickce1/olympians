#include "../Enemy.h"
#include <cugl/cugl.h>

/* This class represents the Cyclops and allows the implementation of any custom behavior associated with this boss */
class Cyclops : public Enemy {
private:
	/** True once the first health threshold been triggered, preventing it from firing again */
	bool _defense1Triggered = false;
	/** True once the second health threshold been triggered, preventing it from firing again */
	bool _defense2Triggered = false;
	/** The health value (absolute, not percentage) at which the first enrage triggers */
	float _defense1Threshold;
	/** The health value (absolute, percentage) at which the second enrage triggers */
	float _defense2Threshold;

	/** This variable is used in boulder toss to check how long it has been since our last turn
	  * It is used to clamp the turning rate to prevent the cyclops looking like he is glitching in and out
	 */

	/**
	 * This variable defines the clamp of the rate at which the cyclops turns
	*/

public:
	Cyclops() {}

	/** Override version of Enemy's init method, where custom data can be initialized
	 * @param enemyID represents the name/id of the boss we are trying to get the data for
	 * @param jsonPath is the path to the enemies.json file
	 */
	bool init(const std::string& enemyId, const std::string& jsonPath) override;
	
	/** Override of the enemy update method for custom logic 
	 * @param dt is the time that passed from the last time update was called
	 */
	void update(float dt) override;

	/** Defines the cyclops' custom behavior for when he chooses to defend */
	bool shouldDefend() override;

	/* Handles taking damage and applying the side modifiers
     * Use this method instead of updateHealth() for appropriate damage multiplication 
     * @param damage is the amount of damage being done to the boss
     * @param playerIndex is the index that was assigned to the player by the host
	 * 
	 * This override primarily handles the special condition of the boulder toss attack,
	 *		where the boss immidiately does damage based on the side it got hit from
    */
	void takeDamage(float damage, int playerIndex) override;
};
