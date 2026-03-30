// Enemy.h
#ifndef __ENEMY_H__
#define __ENEMY_H__

#include <string>
#include <unordered_map>
#include <vector>
#include "EnemyLoader.h"

/**
 * Enemy runtime instance.
 * - Holds health + current state
 * - Runs state timing (buildUp -> fire events once -> nextState -> cooldown lockout)
 * - Queues fired events for EnemyController to resolve onto players
 *
 * Enemy does NOT choose behavior. External code calls requestState().
 */
class Enemy {
public:
    struct FiredEvent {
        EnemyLoader::EventDef def;   // type, target offset, amount, etc.
        EnemyLoader::State state;       // state that fired this event (debug)
    };

protected:
    std::string _enemyId;
    std::string _name;
    std::string _spritesheetPath;

    //for bosses we have custom data. Subclasses can use this to extract any class-specific data
    std::shared_ptr<cugl::JsonValue> _customData;
    
    int _targetIndex = -1;

    float _maxHealth = 0.0f;
    float _currentHealth = 0.0f;

    std::unordered_map<EnemyLoader::State, EnemyLoader::StateDef> _states;
    std::unordered_map<int, float> _sideMultipliers; //stores the damage multipliers for each side. The sides are relative, so 0 would be direction boss facing

    EnemyLoader::State _currentState = EnemyLoader::State::IDLE;
    //how long we have been in this state
    float _stateTime = 0.0f;
    bool _eventsFiredThisState = false;

    // Blocks starting non-idle states while > 0
    float _attackLockout = 0.0f;
    float _retargetLikelihood = 0.0f;

    std::vector<FiredEvent> _firedEvents;

public:
    Enemy() = default;
    
    bool init(const std::string& enemyId, const std::string& jsonPath);

    const std::string& getId() const { return _enemyId; }
    const std::string& getName() const { return _name; }
    const std::string& getSpritesheetPath() const { return _spritesheetPath; }

    float getMaxHealth() const { return _maxHealth; }
    float getCurrentHealth() const { return _currentHealth; }
    bool isAlive() const { return _currentHealth > 0.0f; }
    void setCurrentHealth(float health) { _currentHealth = health; }

    int getTargetIndex() const { return _targetIndex; }
    int setTargetIndex(int index) { _targetIndex = index;  }

    EnemyLoader::State getCurrentState() const { return _currentState; }
    float getStateTime() const { return _stateTime; }
    const EnemyLoader::StateDef* getCurrentStateDef() const;

    float getAttackLockoutRemaining() const { return _attackLockout; }
    bool canStartNonIdleState() const { return _attackLockout <= 0.0f; }
    float getRetargetLikelihood() const { return _retargetLikelihood; }
    void  setRetargetLikelihood(float v);

    //returns the multiplier data for that side
    float getSideMultiplier(int index);
    
    //lets you change the multipler value for that side
    void changeSideMultiplier(int index, float multiplier);
    
    // Expose state defs so controller can pick attacks by tag
    const std::unordered_map<EnemyLoader::State, EnemyLoader::StateDef>& getStates() const { return _states; }

    bool requestState(EnemyLoader::State state);
    void virtual update(float dt);

    std::vector<FiredEvent> takeFiredEvents();

    // Positive heals, negative damages; clamps to [0, maxHealth]
    void updateHealth(float delta);

    // Handles taking damage and records the hits that we took
    void takeDamage(float damage, int playerIndex);

protected:
    void enterState(EnemyLoader::State state);
    void tick(float dt);
    bool readyToFire() const;
    void fireEvents();
    void applyCooldown();
    EnemyLoader::State getNextStateOrIdle() const;
};

#endif /* !__ENEMY_H__ */