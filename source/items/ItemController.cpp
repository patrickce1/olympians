#include "ItemController.h"

using namespace cugl;

bool ItemController::init(const std::shared_ptr<AssetManager>& assets,
                          const std::string& jsonKey,
                          float giveInterval) {
    if (!assets) {
        return false;
    }

    auto itemsJson = assets->get<JsonValue>(jsonKey);
    if (!itemsJson) {
        CULog("ItemController: missing json asset '%s'", jsonKey.c_str());
        return false;
    }

    if (!_itemDb.loadFromJson(itemsJson)) {
        CULog("ItemController: failed to load item database");
        return false;
    }

    _idGen.startGame(ItemInstance::IdGenerator::randomGameId());
    _itemDb.setStartingPointWithTime();

    _giveInterval = giveInterval;
    _giveTimer = 0.0f;

    return true;
}

void ItemController::update(float dt, std::vector<Player>& players) {
    _giveTimer += dt;

    while (_giveTimer >= _giveInterval) {
        _giveTimer -= _giveInterval;
        giveRandomItemToAll(players);
    }
}

void ItemController::giveRandomItemToAll(std::vector<Player>& players) {
    for (Player& player : players) {
        if (!player.isAlive()) {
            continue;
        }

        std::string defId = _itemDb.rollRandomDefId();
        if (defId.empty()) {
            continue;
        }

        auto instance = _itemDb.createInstance(defId, _idGen.next());
        if (!instance) {
            continue;
        }

        player.addItem(*instance);
        CULog("Gave player %d item %s (id=%llu)",
              player.getPlayerNumber(),
              defId.c_str(),
              (unsigned long long)instance->getId());
    }
}

bool ItemController::useItemById(int ownerIndex, Enemy enemy, ItemInstance::ItemId itemId, std::vector<Player>& players) {
    if (ownerIndex < 0) {
        return false;
    }
    if (ownerIndex >= (int)players.size()) {
        return false;
    }
    if (!players[ownerIndex].isAlive()) {
        return false;
    }

    bool used = players[ownerIndex].useItemById(itemId, enemy);
    if (used) {
        CULog("Player %d used an item on enemy. Target health now: %.1f",
              players[ownerIndex].getPlayerNumber(),
              players[targetIndex].getCurrentHealth());
    }
    return used;
}