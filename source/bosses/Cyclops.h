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
	/** The rate at which Cyclops starts to shorten the wait times in his states. A value of 1 means that all build up goes by 1 second faster than usual*/
	float _franticRate;

	/** The rate at which a hit from players shortens the boulder toss build up. 
	  * Ex. a value of 1.0 means the build up time is shortened by 1 second every time cyclops is hit*/
	float _boulderTossReductionAmount;

public:
	Cyclops() {}

	/** Override version of Enemy's init method, where custom data can be initialized
	 * @param enemyID represents the name/id of the boss we are trying to get the data for
	 * @param jsonPath is the path to the enemies.json file
	 */
	bool init(const std::string& enemyId, const std::string& jsonPath) override;
	
	/** Override of the enemy update method for custom logic 
	 * @param dt is the time that passed from the last time update was called
	 * 
	 * The boss attacks immidately upon the defense thesholds being triggered
	 * Certain defense thresholds being met also makes the boss more frantic, switching states more often
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
