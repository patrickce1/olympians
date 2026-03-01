#ifndef __ITEM_CONTROLLER_H__
#define __ITEM_CONTROLLER_H__
#include <cugl/cugl.h>
#include <vector>
#include "ItemDatabase.h"
#include "ItemInstance.h"
#include "Player.h"

class ItemController {
private:
    // Instance of the item Database
    ItemDatabase _itemDb;
    // Instance of the item ID generator
    ItemInstance::IdGenerator _idGen;
    // The item interval that determines how long you have to wait till recceiving another item
    float _itemInterval;
    // The item timer which tells us how long it has been since players last received an item
    float _itemTimer;

public:
    ItemController() = default;

    bool init(const std::shared_ptr<cugl::AssetManager>& assets, const std::string& jsonKey = "items");

    /**
     * // Update the item timer and hand out cards
     * @param dt  time elapsed
     */
    void update(float dt, Player* player);

    /**
     * // Gives random item to player
     * @param & player The player of the game
     */
    void giveRandomItem(Player* player);

    /**
     * // Retrieves the item database
     * @return the item database
     */
    const ItemDatabase& getDatabase() const { return _itemDb; }
};

#endif // __ITEM_CONTROLLER_H__
