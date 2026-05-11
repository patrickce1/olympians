// Player.cpp
#include "Player.h"
#include <algorithm>
#include <cmath>
#include "items/EffectSystem.h"

/**
 *Creates a player instance giiven a house ID
 * @param houseID         The ID of the house as appears in the JSON
 */

Player::Player(const std::string& houseId, int playerNumber,
                    const std::string& playerName,
                    const HouseLoader& loader) {
    
    // Set player-specific info
    _playerNumber = playerNumber;
    _playerName   = playerName;
    
    if (houseId.empty()) {
            _houseId = "";
            return;
    }
    
    // Safety check — make sure the house exists in the loader
    CUAssertLog(loader.has(houseId), "House ID not found: %s", houseId.c_str());

    const HouseLoader::HouseDef& def = loader.get(houseId);

    // Load from HouseDef
    _houseId      = def.id;
    _maxHealth        = def.maxHealth;
    _currentHealth    = def.maxHealth;
    _spritesheetPath  = def.spritesheetPath;
    _specialAbilities = def.specialAbilities;

    // Inventory starts empty — items are added during gameplay
    _inventory = {};
}

/**
 * Updates the current health of the player by the given delta.
 * Positive values heal, negative values deal damage.
 * Health is clamped between 0 and maxHealth.
 * @param delta     The amount to change health by
 */
void Player::updateHealth(float delta) {
    if (delta < 0.0f) {
        float incomingDamage = -delta;

        // Barrier applies first as percentage mitigation, then shield removes a fixed amount.
        if (_hasBarrier && _barrierDuration > 0.0f) {
            incomingDamage *= _barrierMultiplier;
            
            // Barrier is only removed on hit for invincibility barriers
            if (_barrierMultiplier == 0.0f) {
                _hasBarrier = false;
                _barrierMultiplier = 1.0f;
                _barrierDuration = 0.0f;
                
                if (_debug) {
                    CULog("Invincibility expired: player='%s' house='%s' reason='hit'",
                        _playerName.c_str(),
                        _houseId.c_str());
                }
            }
        }

        if (_hasShield && _shieldDuration > 0.0f) {
            const float absorbedAmount = std::min(incomingDamage, _shieldHealth);
            if (absorbedAmount > 0.0f) _shieldAbsorbedDamage = true;
            float tempDamage = incomingDamage;
            incomingDamage = std::max(0.0f, incomingDamage - _shieldHealth);
            
            _shieldHealth = std::max(0.0f, _shieldHealth - tempDamage);
            
            if (_debug) {
                CULog("Shield update: player='%s' house='%s' reason='hit' absorbed=%.3f remainingDamage=%.3f",
                    _playerName.c_str(),
                    _houseId.c_str(),
                    absorbedAmount,
                    incomingDamage);
            }
            
            if (_shieldHealth <= 0.0f) {
                if (_debug) {
                    CULog("Shield expired: player='%s' house='%s' reason='used'",
                        _playerName.c_str(),
                        _houseId.c_str());
                }
                
                _hasShield = false;
                _shieldHealth = 0.0f;
                _shieldDuration = 0.0f;
            }
        }

        delta = -incomingDamage;
    }

    _currentHealth += delta;
    if (_currentHealth > _maxHealth) _currentHealth = _maxHealth;
    if (_currentHealth < 0.0f)       _currentHealth = 0.0f;
}

/** Applies a shield to this player. Replaces any existing shield.
 *
 * @param mitigation  The amount of damage the shield blocks
 * @param duration      How long the shield will stay up for
 */
void Player::applyShield(float mitigation, float duration) {
    if (duration <= 0.0f) {
        return;
    }

    _hasShield = true;
    _shieldHealth = std::max(0.0f, mitigation);
    _shieldDuration = duration;
    
    if (_debug) {
        CULog("Shield applied: player='%s' house='%s' mitigation=%.3f duration=%.3f",
            _playerName.c_str(),
            _houseId.c_str(),
            _shieldHealth,
            _shieldDuration);
    }
}

/**
 * Applies a timed percentage-mitigation barrier to this player.
 *
 * @param multiplier  The percentage multiplier for incoming damage.
 * @param duration      How long the barrier will stay up for
 */
void Player::applyBarrier(float multiplier, float duration) {
    if (duration <= 0.0f) {
        return;
    }

    _hasBarrier = true;
    _barrierMultiplier = std::max(0.0f, multiplier);
    _barrierDuration = duration;
    
    if (_debug) {
        CULog("Barrier applied: player='%s' house='%s' multiplier=%.3f duration=%.3f",
            _playerName.c_str(),
            _houseId.c_str(),
            _barrierMultiplier,
            _barrierDuration);
    }
}

/**
 * Applies a timed heal-over-time effect to this player.
 *
 * Current `regenDuration` and `regenAmountRemaining` are completely
 * overridden when this function is called when this player already has active regen.
 *
 * @param amount    The total healing to apply over the full duration.
 * @param duration  How long the regen lasts.
 */
void Player::applyRegen(float amount, float duration) {
    if (amount <= 0.0f || duration <= 0.0f) {
        return;
    }

    _hasRegen = true;
    _regenAmountRemaining = std::max(0.0f, amount);
    _regenDuration = duration;

    if (_debug) {
        CULog("Regen applied: player='%s' house='%s' amount=%.3f duration=%.3f",
            _playerName.c_str(),
            _houseId.c_str(),
            _regenAmountRemaining,
            _regenDuration);
    }
}

/**
 * Applies a timed educate effect to this player.
 *
 * @param duration How long the educate effect should stay active.
 */
void Player::applyEducate(float duration) {
    if (duration <= 0.0f) {
        return;
    }

    _educateDuration = duration;

    if (_debug) {
        CULog("Educate applied: player='%s' house='%s' duration=%.3f",
            _playerName.c_str(),
            _houseId.c_str(),
            _educateDuration);
    }
}

/**
 * Applies a timed charm effect to this player.
 *
 * @param duration How long the charm effect should stay active.
 */
void Player::applyCharm(float duration) {
    if (duration <= 0.0f) {
        return;
    }

    _charmDuration = duration;

    if (_debug) {
        CULog("Charm applied: player='%s' house='%s' duration=%.3f",
            _playerName.c_str(),
            _houseId.c_str(),
            _charmDuration);
    }
}

/**
 * Advances this player's active runtime support effects by the elapsed frame time.
 *
 * Both shield and barrier durations are reduced by `dt` and clamped to `0.0f` so
 * they never become negative. When a shield timer reaches zero, the shield is marked
 * inactive, any remaining flat damage absorption is cleared, and an expiration log is
 * emitted. When a barrier timer reaches zero, the barrier is marked inactive and its
 * damage multiplier is restored to the neutral `1.0f` value.
 *
 * @param dt  The elapsed time since the previous frame, in seconds.
 */
void Player::updateEffects(float dt) {
    if (_shieldDuration > 0.0f) {
        _shieldDuration = std::max(0.0f, _shieldDuration - dt);
        if (_shieldDuration <= 0.0f) {
            _hasShield = false;
            _shieldHealth = 0.0f;
            if (_debug) {
                CULog("Shield expired: player='%s' house='%s' reason='duration'",
                    _playerName.c_str(),
                    _houseId.c_str());
            }
        }
    }

    if (_barrierDuration > 0.0f) {
        _barrierDuration = std::max(0.0f, _barrierDuration - dt);
        if (_barrierDuration <= 0.0f) {
            _hasBarrier = false;
            _barrierMultiplier = 1.0f;
            if (_debug) {
                CULog("Barrier expired: player='%s' house='%s' reason='duration'",
                    _playerName.c_str(),
                    _houseId.c_str());
            }
        }
    }

    if (_regenDuration > 0.0f && _regenAmountRemaining > 0.0f) {
        const float appliedDt = std::min(dt, _regenDuration);
        const float healPerSecond = _regenAmountRemaining / _regenDuration;
        const float healAmount = healPerSecond * appliedDt;

        updateHealth(healAmount);
        _regenAmountRemaining = std::max(0.0f, _regenAmountRemaining - healAmount);
        _regenDuration = std::max(0.0f, _regenDuration - appliedDt);

        if (_regenDuration <= 0.0f || _regenAmountRemaining <= 0.0f) {
            _hasRegen = false;
            _regenAmountRemaining = 0.0f;
            _regenDuration = 0.0f;
            if (_debug) {
                CULog("Regen expired: player='%s' house='%s'",
                    _playerName.c_str(),
                    _houseId.c_str());
            }
        }
    }

    if (_educateDuration > 0.0f) {
        _educateDuration = std::max(0.0f, _educateDuration - dt);
        if (_educateDuration <= 0.0f && _debug) {
            CULog("Educate expired: player='%s' house='%s'",
                _playerName.c_str(),
                _houseId.c_str());
        }
    }

    if (_charmDuration > 0.0f) {
        _charmDuration = std::max(0.0f, _charmDuration - dt);
        if (_charmDuration <= 0.0f && _debug) {
            CULog("Charm expired: player='%s' house='%s'",
                _playerName.c_str(),
                _houseId.c_str());
        }
    }
}

/** Clears runtime-only combat effects. */
void Player::clearRuntimeEffects() {
    _hasShield = false;
    _shieldHealth = 0.0f;
    _shieldDuration = 0.0f;
    _hasBarrier = false;
    _barrierMultiplier = 1.0f;
    _barrierDuration = 0.0f;
    _hasRegen = false;
    _regenAmountRemaining = 0.0f;
    _regenDuration = 0.0f;
    _educateDuration = 0.0f;
    _charmDuration = 0.0f;
}

/**
 * Returns an effect copy adjusted by charm if charm is active.
 *
 * Charm only affects effects that are applied while it is active. Regen duration
 * is intentionally unchanged, including resurrect's regen duration.
 *
 * @param effect       The original item effect definition.
 * @param charmActive  Whether charm should modify the effect.
 * @return The original effect, or a charm-boosted copy when charm is active.
 */
static ItemDef::Effect resolveEffectForCharm(const ItemDef::Effect& effect, bool charmActive) {
    ItemDef::Effect resolved = effect;
    if (!charmActive || effect.type == ItemDef::EffectType::Charm) {
        return resolved;
    }

    switch (effect.type) {
        case ItemDef::EffectType::Shield:
            resolved.mitigation *= 2.0f;
            resolved.duration *= 2.0f;
            break;
        case ItemDef::EffectType::Barrier:
            resolved.multiplier *= 0.5f;
            resolved.duration *= 2.0f;
            break;
        case ItemDef::EffectType::Regen:
            resolved.regenAmount *= 2.0f;
            break;
        case ItemDef::EffectType::Resurrect:
            resolved.reviveHealth *= 2.0f;
            resolved.regenAmount *= 2.0f;
            break;
        case ItemDef::EffectType::Educate:
        case ItemDef::EffectType::Stun:
        case ItemDef::EffectType::Love:
            resolved.duration *= 2.0f;
            break;
        case ItemDef::EffectType::Slow:
            resolved.multiplier *= 0.5f;
            resolved.duration *= 2.0f;
            break;
        case ItemDef::EffectType::Vulnerable:
            resolved.multiplier *= 2.0f;
            resolved.duration *= 2.0f;
            break;
        case ItemDef::EffectType::Upgrade:
            break;
        case ItemDef::EffectType::Forge:
            resolved.chance = std::min(1.0f, resolved.chance * 2.0f);
            break;
        case ItemDef::EffectType::Charm:
            break;
        case ItemDef::EffectType::Frenzy:
            resolved.amount *= 0.5f;
            break;
    }

    return resolved;
}

/**
 * Computes the final magnitude of an item use after house role and affinity bonuses.
 *
 * The result starts from the item's base value, applies the current player's
 * house multiplier for the item's type, then applies any rarity-based house
 * affinity bonus. A small positive minimum is enforced so item uses never
 * resolve to zero or a negative value.
 *
 * @param player  The player using the item
 * @param def        The item definition being resolved
 * @param db           The item database that provides multiplier metadata
 * @return       The final resolved item magnitude after applying bonuses
 */
static float computeResolvedItemMagnitude(const Player& player,
                                          const ItemDef& def,
                                          const ItemDatabase& db) {
    float houseRoleMultiplier = 0.0f;
    float affinityBonus = 1.0f;
    const auto* houseMultipliers = db.getHouseMultipliers(player.getHouseName());
    if (houseMultipliers) {
        switch (def.getType()) {
            case ItemDef::Type::Attack:
                houseRoleMultiplier = houseMultipliers->attack;
                break;
            case ItemDef::Type::Support:
                houseRoleMultiplier = houseMultipliers->support;
                break;
        }

        const bool affinityEligible =
            (def.getRarity() == ItemDef::Rarity::Rare || def.getRarity() == ItemDef::Rarity::Divine);
        const bool affinityMatch =
            (def.getHouseAffinity() == ItemDef::houseFromString(player.getHouseName(), ItemDef::House::None));
        if (affinityEligible && affinityMatch) {
            affinityBonus = houseMultipliers->affinityBonus;
        }
    }

    float itemMultiplier = 1.0f;
    for (const ItemDef::Effect& effect : def.getEffects()) {
        const ItemDef::Effect resolvedEffect = resolveEffectForCharm(effect, player.hasCharm());
        if (resolvedEffect.type == ItemDef::EffectType::Upgrade) {
            itemMultiplier *= std::pow(resolvedEffect.multiplier, static_cast<float>(player.getMalletUseCount()));
        }
    }

    float resolvedMagnitude = def.getBaseValue() * itemMultiplier * (1.0f + houseRoleMultiplier) * affinityBonus;
    if (resolvedMagnitude <= 0.0f) {
        resolvedMagnitude = 0.0f;
    }

    return resolvedMagnitude;
}

/**
 * Returns whether this player can apply the item's configured utility effects.
 *
 * Effects are universal when the item has no house affinity. Otherwise, only
 * players whose selected house matches the item affinity can trigger them.
 *
 * @param player  The player attempting to use the item.
 * @param def     The item definition being checked.
 * @return True when the item's effects should be dispatched.
 */
static bool canApplyItemEffects(const Player& player, const ItemDef& def) {
    if (player.hasEducate()) {
        return true;
    }

    if (def.getHouseAffinity() == ItemDef::House::None) {
        return true;
    }

    return def.getHouseAffinity() ==
           ItemDef::houseFromString(player.getHouseName(), ItemDef::House::None);
}

/**
 * Returns whether consuming the given item should advance the player's upgrade streak.
 *
 * @param def The item definition being checked for streak-advancing effects.
 * @return True if the item carries an upgrade effect that should advance the streak.
 */
static bool itemConsumesUpgradeStreak(const ItemDef& def) {
    for (const ItemDef::Effect& effect : def.getEffects()) {
        if (effect.type == ItemDef::EffectType::Upgrade) {
            return true;
        }
    }
    return false;
}

/**
 * Computes the resolved magnitude for this player's use of the given item.
 *
 * @param def The item definition being resolved.
 * @param db The item database that provides multiplier metadata.
 * @return The resolved magnitude after house, affinity, and upgrade modifiers are applied.
 */
float Player::resolveItemMagnitude(const ItemDef& def, const ItemDatabase& db) const {
    return computeResolvedItemMagnitude(*this, def, db);
}

/**
 * Records any round-scoped item-use state changes caused by consuming an item.
 *
 * @param def The item definition that was just consumed.
 */
void Player::recordItemUse(const ItemDef& def) {
    if (itemConsumesUpgradeStreak(def) && canApplyItemEffects(*this, def)) {
        _malletUseCount += hasCharm() ? 2 : 1;
    }
}

/**
 * Applies one enemy effect for an attack item, handling any item-specific targeting rules.
 *
 * @param def                The item definition that produced the effect.
 * @param effect             The enemy effect to apply.
 * @param resolvedMagnitude  The resolved attack magnitude for this item use.
 * @param target             The enemy receiving the effect.
 * @param playerIndex        The attacking player's slot index.
 * @return The applied effect magnitude reported by the effect system.
 */
static float applyAttackEffectToEnemy(const ItemDef::Effect& effect, float resolvedMagnitude,
                                      Enemy& target, int playerIndex) {
    if (effect.type == ItemDef::EffectType::Vulnerable && effect.applyToAllSides) {
        const bool applied = target.applyVulnerableToAllSides(effect.multiplier, effect.duration);
        return applied ? effect.multiplier : 0.0f;
    }

    return EffectSystem::applyEffectToEnemy(effect, resolvedMagnitude, target, playerIndex);
}

/**
 * Collects the connected party members reachable from the given source player's links.
 *
 * @param source The player whose left/right party links should be traversed.
 * @return A vector containing each reachable party member at most once.
 */
static std::vector<Player*> collectPartyMembers(Player& source) {
    std::vector<Player*> party;
    party.push_back(&source);
    for (size_t index = 0; index < party.size() && party.size() < Enemy::NUM_PLAYERS; ++index) {
        Player* player = party[index];
        if (!player) {
            continue;
        }

        Player* neighbors[2] = { player->getLeftPlayer(), player->getRightPlayer() };
        for (Player* neighbor : neighbors) {
            if (!neighbor) {
                continue;
            }
            if (std::find(party.begin(), party.end(), neighbor) == party.end()) {
                party.push_back(neighbor);
            }
        }
    }

    return party;
}

/**
 * Applies a resurrect effect to every dead player reachable from the source player's party links.
 *
 * Traverses the party ring connected to `source` and revives only players who are
 * currently dead. Living players are left unchanged.
 *
 * @param effect The resurrect effect definition containing revive and regen tuning values.
 * @param source The player whose party links define the connected ally set.
 */
static void applyResurrectEffectToParty(const ItemDef::Effect& effect, Player& source) {
    if (!effect.targetAllAllies) {
        return;
    }

    for (Player* player : collectPartyMembers(source)) {
        if (!player || player->isAlive()) {
            continue;
        }
        player->setCurrentHealth(effect.reviveHealth);
        if (effect.regenAmount > 0.0f && effect.duration > 0.0f) {
            player->applyRegen(effect.regenAmount, effect.duration);
        }
    }
}

/**
 * Applies one attack-item effect to every connected allied party member.
 *
 * @param effect The ally-targeted effect definition to apply.
 * @param resolvedMagnitude The resolved attack magnitude associated with the item.
 * @param source The player whose party links define the connected ally set.
 */
static void applyAttackEffectToParty(const ItemDef::Effect& effect, float resolvedMagnitude, Player& source) {
    const ItemDef::Effect resolvedEffect = resolveEffectForCharm(effect, source.hasCharm());

    if (resolvedEffect.type == ItemDef::EffectType::Resurrect) {
        applyResurrectEffectToParty(resolvedEffect, source);
        return;
    }
    if (resolvedEffect.type == ItemDef::EffectType::Educate) {
        for (Player* player : collectPartyMembers(source)) {
            if (!player) {
                continue;
            }
            player->applyEducate(resolvedEffect.duration);
        }
        return;
    }
    if (resolvedEffect.type == ItemDef::EffectType::Forge) {
        return;
    }
    if (resolvedEffect.type == ItemDef::EffectType::Frenzy) {
        return;
    }

    for (Player* player : collectPartyMembers(source)) {
        if (!player) {
            continue;
        }
        EffectSystem::applyEffectToPlayer(resolvedEffect, resolvedMagnitude, *player);
    }
}

/**
 * Uses the inventory item with the given id on a player target.
 *
 * Support items heal the target using the resolved item magnitude, while any
 * configured item effects are dispatched through the effect system. The item is
 * removed from inventory once used. Returns the applied base magnitude, or
 * -1.0f if the item id or item definition cannot be found.
 *
 * @param itemId  The inventory instance id to consume
 * @param target  The player that receives the item's healing and effects
 * @param db           The item database used to resolve the item definition
 * @return       The applied base magnitude, or -1.0f if the item id or item
 *         definition cannot be found
 */
float Player::useItemById(ItemInstance::ItemId itemId, Player& target, const ItemDatabase& db) {
    for (auto item = _inventory.begin(); item != _inventory.end(); ++item) {
        if (item->getId() != itemId) {
            continue;
        }

        std::shared_ptr<ItemDef> def = db.getDef(item->getDefId());
        if (!def) {
            return -1.0f;
        }

        const float resolvedMagnitude = resolveItemMagnitude(*def, db);
        const bool shouldApplyEffects = canApplyItemEffects(*this, *def);
        float returnedMagnitude = 0.0f;
        if (def->getType() == ItemDef::Type::Support) {
            target.updateHealth(resolvedMagnitude);
            returnedMagnitude = resolvedMagnitude;
            if (shouldApplyEffects) {
                for (const ItemDef::Effect& effect : def->getEffects()) {
                    const ItemDef::Effect resolvedEffect = resolveEffectForCharm(effect, hasCharm());
                    EffectSystem::applyEffectToPlayer(resolvedEffect, resolvedMagnitude, target);
                }
            }
        } else if (shouldApplyEffects && !def->getEffects().empty()) {
            for (const ItemDef::Effect& effect : def->getEffects()) {
                const ItemDef::Effect resolvedEffect = resolveEffectForCharm(effect, hasCharm());
                EffectSystem::applyEffectToPlayer(resolvedEffect, resolvedMagnitude, target);
            }
        }

        recordItemUse(*def);
        _inventory.erase(item);
        return returnedMagnitude;
    }

    return -1.0f;
}

/**
 * Uses the inventory item with the given id on an enemy target.
 *
 * Attack items damage the target using the resolved item magnitude, while any
 * configured item effects are dispatched through the effect system. The item is
 * removed from inventory once used. Returns the applied base magnitude, or
 * -1.0f if the item id or item definition cannot be found.
 *
 * @param itemId  The inventory instance id to consume
 * @param target  The enemy that receives the item's damage and effects
 * @param db  The item database used to resolve the item definition
 * @return The applied base magnitude, or -1.0f if the item id or item definition cannot be found
 */
float Player::useItemById(ItemInstance::ItemId itemId, Enemy& target, const ItemDatabase& db) {
    for (auto item = _inventory.begin(); item != _inventory.end(); ++item) {
        if (item->getId() != itemId) {
            continue;
        }

        std::shared_ptr<ItemDef> def = db.getDef(item->getDefId());
        if (!def) {
            return -1.0f;
        }

        const float resolvedMagnitude = resolveItemMagnitude(*def, db);
        const bool shouldApplyEffects = canApplyItemEffects(*this, *def);
        float returnedMagnitude = 0.0f;
        if (def->getId() == "gaia_rock") {
            target.updateHealth(resolvedMagnitude);
            returnedMagnitude = resolvedMagnitude;
        } else if (def->getType() == ItemDef::Type::Attack) {
            const bool targetsAllAllies = def->getAttackTarget() == ItemDef::AttackTarget::AllAllies;
            const bool appliesToEnemy = !targetsAllAllies;

            if (appliesToEnemy) {
                target.takeDamage(resolvedMagnitude, getPlayerNumber());
                returnedMagnitude = resolvedMagnitude;
                if (shouldApplyEffects) {
                    for (const ItemDef::Effect& effect : def->getEffects()) {
                        const ItemDef::Effect resolvedEffect = resolveEffectForCharm(effect, hasCharm());
                        applyAttackEffectToEnemy(resolvedEffect, resolvedMagnitude, target, getPlayerNumber());
                    }
                }
            } else if (shouldApplyEffects) {
                for (const ItemDef::Effect& effect : def->getEffects()) {
                    applyAttackEffectToParty(effect, resolvedMagnitude, *this);
                }
            }
        } else if (shouldApplyEffects && !def->getEffects().empty()) {
            for (const ItemDef::Effect& effect : def->getEffects()) {
                const ItemDef::Effect resolvedEffect = resolveEffectForCharm(effect, hasCharm());
                applyAttackEffectToEnemy(resolvedEffect, resolvedMagnitude, target, getPlayerNumber());
            }
        }

        recordItemUse(*def);
        _inventory.erase(item);
        return returnedMagnitude;
    }

    return -1.0f;
}

/**
 * Removes an item from the player's inventory by item id.
 */
void Player::removeItemById(ItemInstance::ItemId itemId) {
    for (auto it = _inventory.begin(); it != _inventory.end(); ++it) {
        if (it->getId() == itemId) {
            _inventory.erase(it);
            break;
        }
    }
}

/**
 * Adds an item to the player's inventory.
 */
void Player::addItem(const ItemInstance& item) {
    _inventory.push_back(item);
}

/**
 *Returns whether the player is alive or not
 */
bool Player::isAlive() const {
    return _currentHealth > 0.0f;
}
