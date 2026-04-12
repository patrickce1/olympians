// Player.cpp
#include "Player.h"
#include <algorithm>
#include "items/EffectSystem.h"

/**
 *Creates a player instance giiven a house ID
 * @param houseID         The ID of the house as appears in the JSON
 */

Player::Player(const std::string& houseId, int playerNumber,
                    const std::string& playerName,
                    const HouseLoader& loader){
    
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
            _barrierMultiplier = 1.0f;
        }

        if (_hasShield && _shieldDuration > 0.0f) {
            const float absorbedAmount = std::min(incomingDamage, _shieldMitigation);
            incomingDamage = std::max(0.0f, incomingDamage - _shieldMitigation);
            CULog("Shield expired: player='%s' house='%s' reason='hit' absorbed=%.3f remainingDamage=%.3f",
                  _playerName.c_str(),
                  _houseId.c_str(),
                  absorbedAmount,
                  incomingDamage);
            _hasShield = false;
            _shieldMitigation = 0.0f;
            _shieldDuration = 0.0f;
        }

        delta = -incomingDamage;
    }

    _currentHealth += delta;
    if (_currentHealth > _maxHealth) _currentHealth = _maxHealth;
    if (_currentHealth < 0.0f)       _currentHealth = 0.0f;
}

/** Applies a shield to this player. Replaces any existing shield. */
void Player::applyShield(float mitigation, float duration) {
    if (duration <= 0.0f) {
        return;
    }

    _hasShield = true;
    _shieldMitigation = std::max(0.0f, mitigation);
    _shieldDuration = duration;
    CULog("Shield applied: player='%s' house='%s' mitigation=%.3f duration=%.3f",
          _playerName.c_str(),
          _houseId.c_str(),
          _shieldMitigation,
          _shieldDuration);
}

/** Applies a barrier to this player. Replaces any existing barrier. */
void Player::applyBarrier(float multiplier, float duration) {
    if (duration <= 0.0f) {
        return;
    }

    _hasBarrier = true;
    _barrierMultiplier = std::max(0.0f, multiplier);
    _barrierDuration = duration;
}

/** Advances timed runtime effects. */
void Player::updateEffects(float dt) {
    if (_shieldDuration > 0.0f) {
        _shieldDuration = std::max(0.0f, _shieldDuration - dt);
        if (_shieldDuration == 0.0f) {
            CULog("Shield expired: player='%s' house='%s' reason='duration'",
                  _playerName.c_str(),
                  _houseId.c_str());
            _hasShield = false;
            _shieldMitigation = 0.0f;
        }
    }

    if (_barrierDuration > 0.0f) {
        _barrierDuration = std::max(0.0f, _barrierDuration - dt);
        if (_barrierDuration == 0.0f) {
            _hasBarrier = false;
            _barrierMultiplier = 1.0f;
        }
    }
}

/**
 * Computes the final magnitude of an item use after house role and affinity bonuses.
 *
 * The result starts from the item's base value, applies the current player's
 * house multiplier for the item's type, then applies any rarity-based house
 * affinity bonus. A small positive minimum is enforced so item uses never
 * resolve to zero or a negative value.
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

    float resolvedMagnitude = def.getBaseValue() * (1.0f + houseRoleMultiplier) * affinityBonus;
    if (resolvedMagnitude <= 0.0f) {
        resolvedMagnitude = 0.01f;
    }

    CULog(
        "ItemUseCalc: item='%s' playerHouse='%s' effectiveVal = baseVal(%.3f) * classSlider(1+%.3f) * affinity(%.3f) | = %.3f",
        def.getId().c_str(),
        player.getHouseName().c_str(),
        def.getBaseValue(),
        houseRoleMultiplier,
        affinityBonus,
        resolvedMagnitude
    );

    return resolvedMagnitude;
}

/**
 * Uses the inventory item with the given id on a player target.
 *
 * Support items heal the target using the resolved item magnitude, while any
 * configured item effects are dispatched through the effect system. The item is
 * removed from inventory once used. Returns the applied base magnitude, or
 * -1.0f if the item id or item definition cannot be found.
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

        const float resolvedMagnitude = computeResolvedItemMagnitude(*this, *def, db);
        float returnedMagnitude = 0.0f;
        if (def->getType() == ItemDef::Type::Support) {
            target.updateHealth(resolvedMagnitude);
            returnedMagnitude = resolvedMagnitude;
            for (const ItemDef::Effect& effect : def->getEffects()) {
                EffectSystem::applyToPlayer(effect, resolvedMagnitude, target);
            }
        } else if (!def->getEffects().empty()) {
            for (const ItemDef::Effect& effect : def->getEffects()) {
                EffectSystem::applyToPlayer(effect, resolvedMagnitude, target);
            }
        }

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

        const float resolvedMagnitude = computeResolvedItemMagnitude(*this, *def, db);
        float returnedMagnitude = 0.0f;
        if (def->getType() == ItemDef::Type::Attack) {
            target.updateHealth(-resolvedMagnitude);
            returnedMagnitude = resolvedMagnitude;
            for (const ItemDef::Effect& effect : def->getEffects()) {
                EffectSystem::applyToEnemy(effect, resolvedMagnitude, target);
            }
        } else if (!def->getEffects().empty()) {
            for (const ItemDef::Effect& effect : def->getEffects()) {
                EffectSystem::applyToEnemy(effect, resolvedMagnitude, target);
            }
        }

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
