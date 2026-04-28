#include "../Enemy.h"
#include <cugl/cugl.h>

/**
 * Cyclops boss subclass.
 *
 * Adds two behaviours on top of the base Enemy:
 *   - Frantic mode: once health drops below configurable thresholds, IDLE cooldowns
 *     shorten so the Cyclops attacks more frequently. Both thresholds stack.
 *   - Boulder-toss interaction: taking damage during ATTACK_3 wind-up retargets the
 *     Cyclops and accelerates (or resets) the buildup timer.
 */
class Cyclops : public Enemy {
private:
	/** Absolute HP below which the first frantic tier activates */
	float _frantic1Threshold;

	/** Absolute HP below which the second frantic tier activates (stacks with first) */
	float _frantic2Threshold;

	/** Multiplier applied to dt during IDLE when a frantic tier is active.
	 *  Each active tier adds one copy of (dt * _franticRate) to the idle timer. */
	float _franticRate;

	/** Seconds removed from the boulder-toss wind-up timer per player hit.
	 *  Capped so that at least one full buildup loop always remains. */
	float _boulderTossReductionAmount;

public:
	Cyclops() {}

	/**
	 * Initializes the Cyclops without asset manager support.
	 *
	 * @param enemyId   The unique enemy ID (should be "cyclops")
	 * @param jsonPath  Path to enemies.json
	 * @return true if initialization succeeds, false on error
	 */
	bool init(const std::string& enemyId, const std::string& jsonPath) override;

	/**
	 * Initializes the Cyclops with animation metadata from the AssetManager.
	 *
	 * @param enemyId   The unique enemy ID (should be "cyclops")
	 * @param jsonPath  Path to enemies.json
	 * @param assets    AssetManager containing enemyAnimations.json
	 * @return true if initialization succeeds, false on error
	 */
	bool init(const std::string& enemyId, const std::string& jsonPath, const std::shared_ptr<cugl::AssetManager>& assets) override;

	/**
	 * Per-frame update. Applies frantic cooldown acceleration during IDLE when
	 * health thresholds are met; attack and defense durations are unaffected.
	 *
	 * @param dt  Elapsed time in seconds since the last update
	 */
	void update(float dt) override;

	/**
	 * Applies damage with boulder-toss interaction.
	 * During ATTACK_3 wind-up, retargets toward the attacker and advances
	 * (or resets) the buildup timer. See class-level doc for full rules.
	 *
	 * @param damage       Raw damage before side multipliers
	 * @param playerIndex  Slot index of the attacking player
	 */
	void takeDamage(float damage, int playerIndex) override;
};
