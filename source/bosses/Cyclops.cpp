#include "Cyclops.h"

//the entire class will be implemented in a future pr
//this included as an example for future boss implementers

bool Cyclops::init(const std::string& jsonPath) {
	Enemy::init("Cyclops", jsonPath);
	//How you might do extracting custom data
	_eyeMultiplier = _customData->getFloat("eyeMultiplier", 1.0f);
}

void Cyclops::update(float dt) {
	Enemy::update(dt);
	//Any custom logic would go here
}