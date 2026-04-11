// EnemyController.h
#ifndef __ENEMY_CONTROLLER_H__
#define __ENEMY_CONTROLLER_H__

#include <string>
#include <vector>
#include <cugl/cugl.h>

#include "Enemy.h"
#include "Player.h"

/**
 * EnemyController
 * - Maintains a current target index into players
 * - Optional retarget on entering idle (chance 0..1)
 * - Chooses next attack while idle based on StateDef.tag == "attack"
 * - Resolves fired enemy events onto players
 *
 * Targeting rule for DAMAGE:
 *   victimIndex = (targetIndex + event.targetOffset) mod N
 */
class EnemyController {
public:
    EnemyController();

    void enterIdle(const std::shared_ptr<Enemy>& enemy,
                   std::vector<std::shared_ptr<Player>>& players);

    void update(float dt,
                const std::shared_ptr<Enemy>& enemy,
                std::vector<std::shared_ptr<Player>>& players);

private:
    cugl::Random _rng;

private:
    int randomIndex(int n);
    int wrapIndex(int i, int n) const;

    /** Checks whether the enemy has just entered idle on this frame. */
    void handleIdleEntryIfNeeded(EnemyLoader::State prevState,
                                 EnemyLoader::State curState,
                                 const std::shared_ptr<Enemy>& enemy,
                                 std::vector<std::shared_ptr<Player>>& players);

    /** Upon entering idle state, this function possibly chooses a new target for the enemy. */
    void maybeRetargetOnIdleEntry(const std::shared_ptr<Enemy> enemy,
                                  std::vector<std::shared_ptr<Player>>& players);

    /** Chooses the next state tagged with "attack" for the enemy to enter. */
    EnemyLoader::State chooseNextAttackState(const std::shared_ptr<Enemy>& enemy);

    /* Determines whether the boss should enter a defensive state.
       Returns true if a random chance roll succeeds, or if the enemy's defensive condition is met.*/ 
    bool shouldDefend(const std::shared_ptr<Enemy>& enemy);

    void resolveEnemyEvents(const std::shared_ptr<Enemy>& enemy,
                            std::vector<std::shared_ptr<Player>>& players,
                            const std::vector<Enemy::FiredEvent>& events);
    
    /** Deals damage to the targeted players from a damage event. */
    void resolveDamageEvent(const std::shared_ptr<Enemy>& enemy,
                          std::vector<std::shared_ptr<Player>>& players,
                            const Enemy::FiredEvent& fe);
    
    /** Applies side modifiers to the boss based on a side modifier event */
    void resolveSideMultiplierEvent(const std::shared_ptr<Enemy>& enemy,
        const Enemy::FiredEvent& fe);
    
    /** Applies a heal to the boss based on a heal event */
    void resolveHealEvent(const std::shared_ptr<Enemy>& enemy,
        const Enemy::FiredEvent& fe);
};

#endif /* __ENEMY_CONTROLLER_H__ */
