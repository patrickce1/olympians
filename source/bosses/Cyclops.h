#include "../Enemy.h"
#include <cugl/cugl.h>

class Cyclops : public Enemy {
private:
	//keeps track of what the default multiplier is for damage to the eye
	float _eyeMultiplier;

public:
	Cyclops() {}

	//override version of Enemy's init method, where custom data like the _eyeMultiplier are initialized
	bool init(const std::string& enemyId, const std::string& jsonPath) override;
	
	//override version of update for custom behavior processing
	void update(float dt) override;

};
