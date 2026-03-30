#include "../Enemy.h"
#include <cugl/cugl.h>

class Cyclops : public Enemy {
private:
	float _eyeMultiplier;

public:
	Cyclops();

	bool init(const std::string& jsonPath);
	
	void update(float dt) override;
};
