//  Player.h
#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <cugl/cugl.h>
#include <string>
#include <vector>
#include "HouseLoader.h"
#include "items/ItemInstance.h"
#include "items/ItemDatabase.h"
#include "Enemy.h"
#include <type_traits>


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
    /** The house name for the house the player is from*/
    std::string _houseId;
    /** The max health of the player*/
    float _maxHealth;
    /** The current health of the house and player*/
    float _currentHealth;
    /** The spritesheet correlating to this house*/
    std::string _spritesheetPath;
    /** The list of the house special abilities*/
    std::vector<std::string> _specialAbilities;
    /** The player to the left of this player, or nullptr if none */
    Player* _leftPlayer = nullptr;
    /** The player to the right of this player, or nullptr if none */
    Player* _rightPlayer = nullptr;
    
public:
    /**
     *Creates a player instance given a house ID
     * @param houseId         The ID of the house as appears in the JSON
     */
    Player(const std::string& houseId, int playerNumber,
           const std::string& playerName,
           const HouseLoader& loader);
    
    /**
     * Discards the player and releases all resources
     */
    ~Player() {};
    
#pragma mark Properties
    /**
     * Returns the inventory of the player
     */
    const std::vector<ItemInstance>& getInventory() const { return _inventory;}
    
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
     * Return the name of the house
     */
    std::string getHouseName() const { return _houseId; }
    
    /**
     * Returns whether the player's house is female (for sound effect purposes)
     */
    bool isFemaleHouse() const {
        std::string house = getHouseName();
        return house == "athena" || house == "aphrodite" || house == "demeter";
    }

    /**
     * Return the max health of the house/player
     */
    float getMaxHealth() const { return _maxHealth; }
    
    /**
     * Return the current health of the player
     */
    float getCurrentHealth() const { return _currentHealth; }

    /*Setter for current health*/
    void setCurrentHealth(float health) { _currentHealth = health; }
    
    /**
     * Returns the path of the spritesheet for the house
     */
    std::string getSpritesheetPath() const { return _spritesheetPath; }
    
    /**
     * Returns the list of the house special ability items
     */
    std::vector<std::string> getSpecialAbilities() const { return _specialAbilities; }
    
    /**
     * Returns the player to the left of this player, or nullptr if none.
     */
    Player* getLeftPlayer() const { return _leftPlayer; }

    /**
     * Returns the player to the right of this player, or nullptr if none.
     */
    Player* getRightPlayer() const { return _rightPlayer; }
    
    /**
     * Returns whether this player is AI or not.
     */
    virtual bool isAI() const { return false; }
    
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
    bool isAlive() const;
    
    /**
     * Uses  an item from the player's inventory by item id.
      * @param itemId  The inventory instance id to consume
     * @return resolved item magnitude, 0 if consumed but no matching target type, -1 on failure
     */
    template <typename T>
    float useItemById(ItemInstance::ItemId itemId, T& target, const ItemDatabase& db) {
        for (auto item = _inventory.begin(); item != _inventory.end(); ++item) {
            if (item->getId() == itemId) {
                std::shared_ptr<ItemDef> def = db.getDef(item->getDefId());
                if (!def) {
                    return -1.0f;
                }

                float houseRoleMultiplier = 0.0f;
                float affinityBonus = 1.0f;
                const auto* houseMultipliers = db.getHouseMultipliers(_houseId);
                if (houseMultipliers) {
                    switch (def->getType()) {
                        case ItemDef::Type::Attack:  houseRoleMultiplier = houseMultipliers->attack;  break;
                        case ItemDef::Type::Support: houseRoleMultiplier = houseMultipliers->support; break;
                        case ItemDef::Type::Utility: houseRoleMultiplier = houseMultipliers->utility; break;
                    }
                    // Affinity bonus only applies to rare/divine items when item affinity matches player house.
                    const bool affinityEligible =
                        (def->getRarity() == ItemDef::Rarity::Rare || def->getRarity() == ItemDef::Rarity::Divine);
                    const bool affinityMatch =
                        (def->getHouseAffinity() == ItemDef::houseFromString(_houseId, ItemDef::House::None));
                    if (affinityEligible && affinityMatch) {
                        affinityBonus = houseMultipliers->affinityBonus;
                    }
                }

                float resolvedMagnitude = def->getBaseValue() * (1.0f + houseRoleMultiplier) * affinityBonus;
                if (resolvedMagnitude <= 0.0f) {
                    resolvedMagnitude = 0.01f;
                }

                if constexpr (std::is_same<T, Player>::value) {
                    if (def->getType() == ItemDef::Type::Support) {
                        CULog(
                            "ItemUseCalc: item='%s' type=support playerHouse='%s' effectiveVal = baseVal(%.3f) * classSlider(1+%.3f) * affinity(%.3f) | = %.3f",
                            def->getId().c_str(),
                            _houseId.c_str(),
                            def->getBaseValue(),
                            houseRoleMultiplier,
                            affinityBonus,
                            resolvedMagnitude
                        );
                        target.updateHealth(resolvedMagnitude);
                        _inventory.erase(item);
                        return resolvedMagnitude;
                    }
                }
                else if constexpr (std::is_same<T, Enemy>::value) {
                    if (def->getType() == ItemDef::Type::Attack) {
                        CULog(
                            "ItemUseCalc: item='%s' type=attack playerHouse='%s' effectiveVal = baseVal(%.3f) * classSlider(1+%.3f) * affinity(%.3f) | = %.3f",
                            def->getId().c_str(),
                            _houseId.c_str(),
                            def->getBaseValue(),
                            houseRoleMultiplier,
                            affinityBonus,
                            resolvedMagnitude
                        );
                        target.takeDamage(resolvedMagnitude, _playerNumber);
                        _inventory.erase(item);
                        return resolvedMagnitude;
                    }
                }

                _inventory.erase(item);
                return 0.0f;
            }
        }
        return -1.0f;
    }
    
    /**
     * Removes all items from the player's inventory.
     *
     * Called when the scene resets to ensure no stale items persist
     * into the next round. Any widgets synced from this inventory
     * will be removed on the next syncInventoryWidgets() call.
     */
    void clearInventory() {
        _inventory.clear();
    }
    
    /**
     * Removes  an item from the player's inventory by item id.
     * @param item    The item to remove
     * @param target    The target to apply the item to (Player or Enemy)
     * @return true if the item was found and removed, false otherwise
     */
    void removeItemById(ItemInstance::ItemId itemId);
    
    /**
     * Sets the player to the left of this player.
     * @param player    The player to the left, or nullptr to clear.
     */
    void setLeftPlayer(Player* player) { _leftPlayer = player; }

    /**
     * Sets the player to the right of this player.
     * @param player    The player to the right, or nullptr to clear.
     */
    void setRightPlayer(Player* player) { _rightPlayer = player; }
    
    /**
     * Sets the display name of the player.
     * Used when a networked player joins before selecting a house,
     * so their name can be shown in the lobby without reconstructing
     * the Player object.
     *
     * @param name  The display name to assign.
     */
    void setPlayerName(const std::string& name) { _playerName = name; }
    
};
#endif /* !__PLAYER_H__ */

