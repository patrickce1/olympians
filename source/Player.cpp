// Player.cpp
#include "Player.h"

/**
 *Creates a player instance giiven a character ID
 * @param characterID         The ID of the character as appears in the JSON
 */

void Player::Player(const std::string& characterId, int playerNumber,
                    const std::string& playerName,
                    const CharacterLoader& loader){
    
    // Safety check — make sure the character exists in the loader
    CUAssertLog(loader.has(characterId), "Character ID not found: %s", characterId.c_str());

    const CharacterLoader::CharacterDef& def = loader.get(characterId);

    // Load from CharacterDef
    _characterId     = def.id;
    _house           = def.house;
    _maxHealth       = def.maxHealth;
    _currentHealth   = def.maxHealth;
    _abilityClass    = def.abilityClass;
    _spritesheetPath = def.spritesheetPath;
    _specialAbilities= def.specialAbilities;

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
void Player::addItem(const Item& item) {
    _inventory.push_back(item);
}

/**
 *Returns whether the player is alive or not
 */
bool Player::isAlive(){
    return _currentHealth > 0;
}

/**
 * Uses an item from the player's inventory on a target.
 * Calls item.apply() on the target and removes the item if successful.
 * @param item      The item to use
 * @param target    The target to apply the item to (Player or Enemy)
 * @return true if the item was found and used, false otherwise
 */
template <typename T>
bool Player::useItem(const Item& item, T& target) {
    for (auto it = _inventory.begin(); it != _inventory.end(); ++it) {
        if (it->id == item.id) {
            it->apply(*it, target);
            _inventory.erase(it);
            return true;
        }
    }
    return false;
}
