#ifndef __CYCLOPS__
#define __CYCLOPS__
#include "../Enemy.h"
#include <cugl/cugl.h>

/* This class represents the Cyclops and allows the implementation of any custom behavior associated with this boss */
class Cyclops : public Enemy {
private:
	/** The health value (absolute, not percentage) at which the first enrage triggers */
	float _frantic1Threshold;
	/** The health value (absolute, percentage) at which the second enrage triggers */
	float _frantic2Threshold;
	/** By what percentage the cyclops begins to increase his rate of attack after each enrage.
	  * Ex. 0.5 would mean his attacks go by 50% faster. If both thresholds were met in this scenario,
	  * his attacks would be going by 100% faster */
	float _franticRate;
	/** The rate at which a hit from players shortens the boulder toss build up.
	  * Ex. a value of 1.0 means the build up time is shortened by 1 second every time cyclops is hit*/
	float _boulderTossReductionAmount;
	/** The maximum stateTime threshold for the boulder throw animation's attack phase.
	  * When the boss is attacked, this variable prevents the state from ending before the throw animation completes,
	  * ensuring the boulder release frames are always fully played out.
	*/
	float _boulderHigherBound;

public:
	Cyclops() {}

	/** Override version of Enemy's init method, where custom data can be initialized
	 * @param enemyID represents the name/id of the boss we are trying to get the data for
	 * @param jsonPath is the path to the enemies.json file
	 */
	bool init(const std::string& enemyId, const std::string& jsonPath) override;

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
	bool init(const std::string& enemyId, const std::string& jsonPath, const std::shared_ptr<cugl::AssetManager>& assets) override;

	/** Override of the enemy update method for custom logic
	 * @param dt is the time that passed from the last time update was called
	 *
	 * The boss attacks immidately upon the defense thesholds being triggered
	 * Certain defense thresholds being met also makes the boss more frantic, switching states more often
	 */
	void update(float dt) override;

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
#endif // __CYCLOPS__