#include "ItemController.h"

using namespace cugl;

bool ItemController::init(const std::shared_ptr<AssetManager>& assets,
                          const std::string& jsonKey) {

    std::string path = "json/items.json";

    auto reader = cugl::JsonReader::alloc(path);
    if (!reader) {
        CULog("ItemController: failed to open %s", path.c_str());
        return false;
    }

    auto itemsJson = reader->readJson();
    if (!itemsJson) {
        CULog("ItemController: failed to parse %s", path.c_str());
        return false;
    }

    // the item array
    auto itemArray = itemsJson->get("items");
    if (!itemArray || !itemArray->isArray()) {
        CULog("ItemController: items.json missing 'items' array");
        return false;
    }

    // Create Item Database
    if (!_itemDb.loadFromJson(itemsJson)) {
        CULog("ItemController: failed to load item database");
        return false;
    }

    
    // Read itemInterval from JSON if present, otherwise return error
    if (itemsJson->has("itemInterval") && itemsJson->get("itemInterval")->isNumber()) {
        _itemInterval = itemsJson->get("itemInterval")->asFloat();
    } else {
        CULogError("No item interval was specified");
    }

    // Read itemTimerStart from JSON if present, otherwise return error
    if (itemsJson->has("itemTimerStart") && itemsJson->get("itemTimerStart")->isNumber()) {
        _itemTimer = itemsJson->get("itemTimerStart")->asFloat();
    } else {
        CULogError("No item timer start was specified");
    }

    _idGen.startGame(ItemInstance::IdGenerator::randomGameId());
    _itemDb.setStartingPointWithTime();

    return true;
}

// Update the item timer and hand out cards
void ItemController::update(float dt, std::vector<Player>& players) {
    _itemTimer += dt;

    while (_itemTimer >= _itemInterval) {
        CULog("[ItemController] Time to give items");
        _itemTimer -= _itemInterval;
        giveRandomItemToAll(players);
    }
}

// Hand out cards to players
void ItemController::giveRandomItemToAll(std::vector<Player>& players) {
    for (Player& player : players) {
        if (!player.isAlive()) {
            CULog("[ItemController] Player is not alive");
            continue;
        }
        
        // Gets defId of random items generated
        std::string itemDefId = _itemDb.rollRandomDefId();
        if (itemDefId.empty()) {
            CULog("[ItemController] DefId is empty");
            continue;
        }

        // Creates instance of generated defId
        auto itemInstance = _itemDb.createInstance(itemDefId, _idGen.next());
        if (!itemInstance) {
            CULog("[ItemController] Failed to create itemInstance");
            continue;
        }

        player.addItem(*itemInstance);
        CULog("Gave player %d item %s (id=%llu)",
              player.getPlayerNumber(),
              itemDefId.c_str(),
              (unsigned long long)itemInstance->getId());
    }
}



