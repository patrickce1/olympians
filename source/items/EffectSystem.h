#ifndef __EFFECT_SYSTEM_H__
#define __EFFECT_SYSTEM_H__

#include "ItemDef.h"

class Player;
class Enemy;

/**
 * Centralized dispatcher for data-driven item effects.
 *
 * Add new effects by extending ItemDef::EffectType and wiring a new
 * per-target handler into the dispatch switch below.
 */
class EffectSystem {
private:
    static float applyShieldToPlayer(const ItemDef::Effect& effect, Player& target) {
        target.applyShield(effect.mitigation, effect.duration);
        return effect.mitigation;
    }

    static float applyBarrierToPlayer(const ItemDef::Effect& effect, Player& target) {
        target.applyBarrier(effect.multiplier, effect.duration);
        return effect.multiplier;
    }

public:
    static float applyToPlayer(const ItemDef::Effect& effect, float resolvedMagnitude, Player& target) {
        (void)resolvedMagnitude;

        switch (effect.type) {
            case ItemDef::EffectType::Shield:
                return applyShieldToPlayer(effect, target);
            case ItemDef::EffectType::Barrier:
                return applyBarrierToPlayer(effect, target);
        }

        return 0.0f;
    }

    static float applyToEnemy(const ItemDef::Effect& effect, float resolvedMagnitude, Enemy& target) {
        (void)effect;
        (void)resolvedMagnitude;
        (void)target;
        return 0.0f;
    }
};

#endif // __EFFECT_SYSTEM_H__
