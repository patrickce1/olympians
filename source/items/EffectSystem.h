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
    /**
     * Applies a shield effect to a player and returns the shield mitigation amount.
     *
     * @param effect  The serialized effect definition to apply.
     * @param target  The player receiving the shield.
     */
    static float applyShieldToPlayer(const ItemDef::Effect& effect, Player& target) {
        target.applyShield(effect.mitigation, effect.duration);
        return effect.mitigation;
    }

    /**
     * Applies a barrier effect to a player and returns the barrier multiplier.
     *
     * @param effect  The serialized effect definition to apply.
     * @param target  The player receiving the barrier.
     */
    static float applyBarrierToPlayer(const ItemDef::Effect& effect, Player& target) {
        target.applyBarrier(effect.multiplier, effect.duration);
        return effect.multiplier;
    }

    /**
     * Applies a stun effect to an enemy and returns the stun duration.
     *
     * @param effect  The serialized effect definition to apply.
     * @param target  The enemy receiving the stun.
     */
    static float applyStunToEnemy(const ItemDef::Effect& effect, Enemy& target) {
        target.applyStun(effect.duration);
        return effect.duration;
    }

    /**
     * Applies a love effect to an enemy and returns the love duration.
     *
     * LOVE effect: same as a stun, but resets the enemy's state to IDLE
     *
     * @param effect  The serialized effect definition to apply.
     * @param target  The enemy receiving the love.
     */
    static float applyLoveToEnemy(const ItemDef::Effect& effect, Enemy& target) {
        target.applyLove(effect.duration);
        return effect.duration;
    }

    /**
     * Applies a vulnerable effect to an enemy and returns the resolved multiplier.
     *
     * @param effect       The serialized effect definition to apply.
     * @param target       The enemy receiving the vulnerability.
     * @param playerIndex  The attacking player's slot index used to resolve the hit side.
     */
    static float applyVulnerableToEnemy(const ItemDef::Effect& effect, Enemy& target, int playerIndex) {
        target.applyVulnerable(effect.multiplier, effect.duration, playerIndex);
        return effect.multiplier;
    }

public:
    /**
     * Applies a supported item effect to a player target.
     *
     * Returns the effect value that was applied, or 0.0f if the effect type
     * does not target players.
     *
     * @param effect             The serialized effect definition to apply.
     * @param resolvedMagnitude  The resolved item magnitude associated with the source item.
     * @param target             The player receiving the effect.
     */
    static float applyEffectToPlayer(const ItemDef::Effect& effect, float resolvedMagnitude, Player& target) {
        (void)resolvedMagnitude;

        switch (effect.type) {
            case ItemDef::EffectType::Shield:
                return applyShieldToPlayer(effect, target);
            case ItemDef::EffectType::Barrier:
                return applyBarrierToPlayer(effect, target);
            case ItemDef::EffectType::Stun:
            case ItemDef::EffectType::Love:
            case ItemDef::EffectType::Vulnerable:
            case ItemDef::EffectType::Upgrade:
                break;
        }

        return 0.0f;
    }

    /**
     * Applies a supported item effect to an enemy target.
     *
     * Returns the effect value that was applied, or 0.0f if the effect type
     * does not target enemies.
     *
     * @param effect             The serialized effect definition to apply.
     * @param resolvedMagnitude  The resolved item magnitude associated with the source item.
     * @param target             The enemy receiving the effect.
     * @param playerIndex        The attacking player's slot index used for side-relative enemy effects.
     */
    static float applyEffectToEnemy(const ItemDef::Effect& effect, float resolvedMagnitude, Enemy& target, int playerIndex) {
        (void)resolvedMagnitude;

        switch (effect.type) {
            case ItemDef::EffectType::Stun:
                return applyStunToEnemy(effect, target);
            case ItemDef::EffectType::Love:
                return applyLoveToEnemy(effect, target);
            case ItemDef::EffectType::Vulnerable:
                return applyVulnerableToEnemy(effect, target, playerIndex);
            case ItemDef::EffectType::Shield:
            case ItemDef::EffectType::Barrier:
            case ItemDef::EffectType::Upgrade:
                break;
        }

        return 0.0f;
    }
};

#endif // __EFFECT_SYSTEM_H__
