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

    int getTargetIndex() const { return _targetIndex; }

    void enterIdle(const std::shared_ptr<Enemy>& enemy,
                   const std::vector<Player>& players);
    void update(float dt,
                const std::shared_ptr<Enemy>& enemy,
                const std::vector<Player>& players);

private:
    int _targetIndex = -1;
    cugl::Random _rng;

private:
    int randomIndex(int n);
    int wrapIndex(int i, int n) const;

    void ensureTargetIndexInRange(const std::vector<Player>& players);

    void handleIdleEntryIfNeeded(const std::string& prevState,
                                 const std::string& curState,
                                 const std::shared_ptr<Enemy>& enemy,
                                 const std::vector<Player>& players);

    void maybeRetargetOnIdleEntry(const std::shared_ptr<Enemy> enemy,
                                  const std::vector<Player>& players);

    std::string chooseNextAttackState(const std::shared_ptr<Enemy>& enemy);

    void resolveEnemyEvents(const std::shared_ptr<Enemy>& enemy,
                            const std::vector<Player>& players,
                            const std::vector<Enemy::FiredEvent>& events);

    void resolveDamageEvent(const std::shared_ptr<Enemy>& enemy,
                            const std::vector<Player>& players,
                            const Enemy::FiredEvent& fe);
};

#endif /* __ENEMY_CONTROLLER_H__ */
