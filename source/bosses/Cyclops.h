#include "../Enemy.h"
#include <cugl/cugl.h>

/* This class represents the Cyclops and allows the implementation of any custom behavior associated with this boss */
class Cyclops : public Enemy {
private:
	//keeps track of what the default multiplier is for damage to the eye
	float _eyeMultiplier;

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

};
