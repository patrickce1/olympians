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

    static float applyRegenToPlayer(const ItemDef::Effect& effect, Player& target) {
        target.applyRegen(effect.magnitude, effect.duration);
        return effect.magnitude;
    }

    static float applyStunToEnemy(const ItemDef::Effect& effect, Enemy& target) {
        target.applyStun(effect.duration);
        return effect.duration;
    }

    /** Applies a vulnerable effect to an enemy and returns the resolved multiplier. */
    static float applyVulnerableToEnemy(const ItemDef::Effect& effect, Enemy& target) {
        target.applyVulnerable(effect.multiplier, effect.duration);
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
            case ItemDef::EffectType::Regen:
                return applyRegenToPlayer(effect, target);
            case ItemDef::EffectType::Stun:
            case ItemDef::EffectType::Vulnerable:
                break;
        }

        return 0.0f;
    }

    static float applyToEnemy(const ItemDef::Effect& effect, float resolvedMagnitude, Enemy& target) {
        (void)resolvedMagnitude;

        switch (effect.type) {
            case ItemDef::EffectType::Stun:
                return applyStunToEnemy(effect, target);
            case ItemDef::EffectType::Vulnerable:
                return applyVulnerableToEnemy(effect, target);
            case ItemDef::EffectType::Shield:
            case ItemDef::EffectType::Barrier:
            case ItemDef::EffectType::Regen:
                break;
        }

        return 0.0f;
    }
};

#endif // __EFFECT_SYSTEM_H__
