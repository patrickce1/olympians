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

    void handleIdleEntryIfNeeded(EnemyLoader::State prevState,
                                 EnemyLoader::State curState,
                                 const std::shared_ptr<Enemy>& enemy,
                                 std::vector<std::shared_ptr<Player>>& players);

    void maybeRetargetOnIdleEntry(const std::shared_ptr<Enemy> enemy,
                                  std::vector<std::shared_ptr<Player>>& players);

    EnemyLoader::State chooseNextAttackState(const std::shared_ptr<Enemy>& enemy);

    void resolveEnemyEvents(const std::shared_ptr<Enemy>& enemy,
                            std::vector<std::shared_ptr<Player>>& players,
                            const std::vector<Enemy::FiredEvent>& events);

    void resolveDamageEvent(const std::shared_ptr<Enemy>& enemy,
                            std::vector<std::shared_ptr<Player>>& players,
                            const Enemy::FiredEvent& fe);
    
    void resolveSideMultiplierEvent(const std::shared_ptr<Enemy>& enemy,
        const Enemy::FiredEvent& fe);

    void resolveHealEvent(const std::shared_ptr<Enemy>& enemy,
        const Enemy::FiredEvent& fe);
};

#endif /* __ENEMY_CONTROLLER_H__ */
