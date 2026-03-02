// Player.cpp
#include "Player.h"

/**
 *Creates a player instance giiven a character ID
 * @param characterID         The ID of the character as appears in the JSON
 */

Player::Player(const std::string& characterId, int playerNumber,
                    const std::string& playerName,
                    const CharacterLoader& loader){
    
    // Safety check — make sure the character exists in the loader
    CUAssertLog(loader.has(characterId), "Character ID not found: %s", characterId.c_str());

    const CharacterLoader::CharacterDef& def = loader.get(characterId);

    // Load from CharacterDef
    _characterId      = def.id;
    _house            = def.house;
    _maxHealth        = def.maxHealth;
    _currentHealth    = def.maxHealth;
    _abilityClass     = def.abilityClass;
    _spritesheetPath  = def.spritesheetPath;
    _specialAbilities = def.specialAbilities;

    // Set player-specific info
    _playerNumber = playerNumber;
    _playerName   = playerName;

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
    _currentHealth += delta;
    if (_currentHealth > _maxHealth) _currentHealth = _maxHealth;
    if (_currentHealth < 0)          _currentHealth = 0;
}

/**
 * Adds an item to the player's inventory.
 * @param item      The item to add
 */
void Player::addItem(const ItemInstance& item) {
    _inventory.push_back(item);
}

/**
 *Returns whether the player is alive or not
 */
bool Player::isAlive() const{
    return _currentHealth > 0;
}

/**
 * Uses and removes an item from the player's inventory on a target.
 * @param item        The item to use
 * @param target    The target to apply the item to (Player or Enemy)
 * @param db             The item database
 * @return true if the item was found and used, false otherwise
 */
template <typename T>
bool Player::useItemById(ItemInstance::ItemId itemId, T& target, const ItemDatabase& db) {
    for (auto item = _inventory.begin(); item != _inventory.end(); ++item) {
        if (item->getId() == itemId) {
            std::shared_ptr<ItemDef> def = db.getDef(item->getDefId());
            if (!def) {
                CULogError("Player: could not find ItemDef for defId '%s'", item->getDefId().c_str());
                return false;
            }

            if constexpr (std::is_same<T, Player>::value) {
                if (def->getType() == ItemDef::Type::Support) {
                    target.updateHealth(def->getEffectiveValue());
                }
            }
            else if constexpr (std::is_same<T, Enemy>::value) {
                if (def->getType() == ItemDef::Type::Attack) {
                    target.updateHealth(-def->getEffectiveValue());
                }
            }

            _inventory.erase(item);
            return true;
        }
    }
    return false;
}

