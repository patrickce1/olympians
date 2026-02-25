#ifndef __ITEM_CONTROLLER_H__
#define __ITEM_CONTROLLER_H__
#include <cugl/cugl.h>
#include <vector>
#include "ItemDatabase.h"
#include "ItemInstance.h"
#include "Player.h"

class ItemController {
private:
    ItemDatabase _itemDb;
    ItemInstance::IdGenerator _idGen;

    float _itemInterval = 3.0f;
    float _itemTimer = 0.0f;

public:
    ItemController() = default;

    bool init(const std::shared_ptr<cugl::AssetManager>& assets,
              const std::string& jsonKey = "items",
              float itemInterval = 3.0f);

    void update(float dt, std::vector<Player>& players);

    void giveRandomItemToAll(std::vector<Player>& players);

    bool useItemById(int ownerIndex, Enemy enemy, ItemInstance::ItemId itemId, std::vector<Player>& players);

    const ItemDatabase& getDatabase() const { return _itemDb; }
};

#endif // __ITEM_CONTROLLER_H__