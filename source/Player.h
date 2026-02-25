//  Player.h
#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <cugl/cugl.h>
#include <string>
#include <vector>
#include "CharacterLoader.h"
#include "ItemInstance.h"
/**
 * Model Class representing the Player
 */
class Player {
private:
    /** The inventory of the player stored as a vector of ItemInstance objects*/
    std::vector<ItemInstance> _inventory;
    /** The player number of this user**/
    int _playerNumber;
    /** The player name*/
    std::string _playerName;
    /** The character name for the character the player is using*/
    std::string _characterId;
    /** The house that the character hails from*/
    std::string _house;
    /** The max health of the character and player*/
    float _maxHealth;
    /** The current health of the character and player*/
    int _currentHealth;
    /** The  ability class of the character*/
    CharacterLoader::AbilityClass _abilityClass;
    /** The spritesheet correlating to this character*/
    std::string _spritesheetPath;
    /** The list of the characters special abilities*/
    std::vector<std::string> _specialAbilities;
    
public:
    /**
     *Creates a player instance giiven a character ID
     * @param characterId         The ID of the character as appears in the JSON
     */
    Player(const std::string& characterId, int playerNumber,
           const std::string& playerName,
           const CharacterLoader& loader);
    
    /**
     * Discards the player and releases all resources
     */
    Player() {};
    
#pragma mark Properties
    /**
     * Returns the inventory of the player
     */
    std::vector<ItemInstance> getInventory() const { return _inventory; }
    
    /**
     * Returns whether player's inventory is not empty
     */
    bool hasItems() const { return !_inventory.empty(); }
    
    /**
     * Returns the player number
     */
    int getPlayerNumber() const { return _playerNumber; }
    
    /**
     * Returns the player name
     */
    std::string getPlayerName() const { return _playerName; }
    
    /**
     * Return the name of the character
     */
    std::string getCharacterName() const { return _characterId; }
    
    /**
     * Return the house of the character
     */
    std::string getCharacterHouse() const { return _house; }
    
    /**
     * Return the max health of the character/player
     */
    float getMaxHealth() const { return _maxHealth; }
    
    /**
     * Return the current health of the character/player
     */
    float getCurrentHealth() const { return _currentHealth; }
    
    /**
     * Returns the ability class of the character
     */
    CharacterLoader::AbilityClass getAbilityClass() const { return _abilityClass; }
    
    /**
     * Returns the path of the spritesheet for the character
     */
    std::string getSpritesheetPath() const { return _spritesheetPath; }
    
    /**
     * Returns the list of the characters special ability items
     */
    std::vector<std::string> getSpecialAbilities() const { return _specialAbilities; }
    
#pragma mark Gameplay
    /**
     * Updates the current health of the player by the given delta.
     * Positive values heal, negative values deal damage.
     * Health is clamped between 0 and maxHealth.
     * @param delta     The amount to change health by
     */
    
    void updateHealth(float delta);
    /**
     * Adds an item to the player's inventory.
     * @param item      The item to add
     */
    void addItem(const ItemInstance& item);
    
    /**
     * Returns whether the player is alive or not
     */
    bool isAlive();
    
    /**
     * Uses and removes an item from the player's inventory by item id.
     * @param item    The item to remove
     * @param target    The target to apply the item to (Player or Enemy)
     * @return true if the item was found and removed, false otherwise
     */
    template <typename T>
    bool useItemById(ItemInstance::ItemId itemId, T& target);
    
};
    
#endif /* !__PLAYER_H__ */
