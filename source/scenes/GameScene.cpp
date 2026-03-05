#include <cugl/cugl.h>
#include <iostream>
#include <sstream>
#include "../InputController.h"
#include "../playerAI/PlayerAI.h"
#include "../playerAI/EasyPlayerAI.h"
#include "../CharacterLoader.h"

#include "GameScene.h"

using namespace cugl;
using namespace cugl::scene2;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Example height for now, change as needed */
#define SCENE_HEIGHT  852

#pragma mark -
#pragma mark Constructors
/**
 * Initializes the controller contents, and starts the game
 *
 * In previous labs, this method "started" the scene.  But in this
 * case, we only use to initialize the scene user interface.  We
 * do not activate the user interface yet, as an active user
 * interface will still receive input EVEN WHEN IT IS HIDDEN.
 *
 * That is why we have the method {@link #setActive}.
 *
 * @param assets    The (loaded) assets for this game mode
 *
 * @return true if the controller is initialized properly, false otherwise.
 */
bool GameScene::init(const std::shared_ptr<cugl::AssetManager>& assets) {
    // Initialize the scene to a locked width
    if (assets == nullptr) {
        return false;
    }
    else if (!Scene2::initWithHint(Size(0, SCENE_HEIGHT))) {
        return false;
    }

    // Start up the input handler
    _assets = assets;
    Size dimen = getSize();
    
    // The comments are outline of how loading a scene from json should work. This DOES NOT WORK YET. Danielle should set this up
    // Acquire the scene built by the asset loader and resize it the scene. 
    _scene = _assets->get<scene2::SceneNode>("gameScene");
    
    if (!_scene) {
        printf("Scene NOT here!");
        return false;
    }

    _scene->setContentSize(dimen);
    _scene->doLayout(); // Repositions the HUD
    
    // Elements setup from assets
    _gameArea  = _scene->getChildByName("gameArea");
    _inventory = _scene->getChildByName("inventory");
    
    if (_gameArea) {

        _attackArea = _gameArea->getChildByName("attackArea");

        // AI player slots
        _playerSlots.push_back(_gameArea->getChildByName("player"));
        _playerSlots.push_back(_gameArea->getChildByName("player3"));
        _playerSlots.push_back(_gameArea->getChildByName("player4"));
    }
    
    if (_attackArea) {
        // attack_area widget contains:
        //   "attack_area" (image region for attack input)
        //   "enemy" (boss widget)
        _bossNode = _attackArea->getChildByName("enemy");
    }

    addChild(_scene);
    
    const std::string characterJsonPath = "json/characters.json";
    if (!_characterLoader.loadFromFile(characterJsonPath)) {
        CULog("Failed to load characters.json");
        return false;
    }
    
    // Create Player Array for all real players
    _players.emplace_back("Percy", 1, "Player 1", _characterLoader);
    
    // --- Circular neighbor linking ---
    // Links all 4 players in a ring: 0 <-> 1 <-> 2 <-> 3 <-> 0
    // Neighbors are used by both the human pass actions and AI support/pass logic.
    //NOTE: ONLY HOST SHOULD RUN THIS-- IMPORTANT FOR NETWORKING
    const int playerCount = (int)_players.size();
    for (int i = 0; i < playerCount; i++) {
        int leftIdx  = (i - 1 + playerCount) % playerCount;
        int rightIdx = (i + 1) % playerCount;
        _players[i].setLeftPlayer(&_players[leftIdx]);
        _players[i].setRightPlayer(&_players[rightIdx]);
    }
    
    // Sets the local player for this GameScene instance using an assigned index
    // Note: Changes with networking
    setLocalPlayer(0);

    if (!_itemController.init(_assets)) {
        return false;
    }
    
    // Spawn GameScene Enemy
    const std::string enemyJsonPath = "json/enemies.json";
    if (!_enemyLoader.loadFromFile(enemyJsonPath)) {
        CULog("GameScene: failed to load enemies from '%s' (continuing without enemy)",
              enemyJsonPath.c_str());
        _enemy.reset();
    } else {
        const std::string enemyId = "enemy1";

        if (!_enemyLoader.has(enemyId)) {
            CULog("GameScene: enemy id '%s' not found in '%s' (continuing without enemy)",
                  enemyId.c_str(), enemyJsonPath.c_str());
            _enemy.reset();
        } else {
            _enemy = std::make_unique<Enemy>(enemyId, _enemyLoader);
            CULog("GameScene: spawned enemy '%s'", enemyId.c_str());
        }
    }

    setActive(false);
    return true;
}

// Called after the network session and assigns all local machines to a network-given player slot
void GameScene::setLocalPlayer(int assignedIndex) {
    CUAssertLog(assignedIndex >= 0 && assignedIndex < (int)_players.size(),
                "Assigned index out of range");
    _localPlayer = &_players[assignedIndex];
}

/**
 * Handles the local player dropping an item on the boss zone to attack.
 */
void GameScene::handleAttack() {
    if (!_enemy) return;
    
    for (const ItemInstance& item : _localPlayer -> getInventory()) {
        auto def = _itemController.getDatabase().getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Attack) {
            _localPlayer -> useItemById(item.getId(), *_enemy, _itemController.getDatabase());
            return;
        }
    }
}

/**
 * Handles the local player dropping an item on the left ally zone to support.
 */
void GameScene::handleSupportLeft() {
    Player* target = _localPlayer -> getLeftPlayer();
    if (!target || !target->isAlive()) return;
    
    for (const ItemInstance& item : _localPlayer -> getInventory()) {
        auto def = _itemController.getDatabase().getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Support) {
            _localPlayer -> useItemById(item.getId(), *target, _itemController.getDatabase());
            return;
        }
    }
}

/**
 * Handles the human player dropping an item on the right ally zone to support.
 */
void GameScene::handleSupportRight() {
    Player* target = _localPlayer -> getRightPlayer();
    if (!target || !target->isAlive()) return;
    
    for (const ItemInstance& item : _localPlayer -> getInventory()) {
        auto def = _itemController.getDatabase().getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Support) {
            _localPlayer -> useItemById(item.getId(), *target, _itemController.getDatabase());
            return;
        }
    }
}

/**
 * Handles the human player passing an item to the left neighbor.
 */
void GameScene::handlePassLeft() {
    Player* target = _localPlayer -> getLeftPlayer();
    
    if (!target || !target->isAlive() || _localPlayer -> getInventory().empty()) return;
    ItemInstance item = _localPlayer -> getInventory()[0];
    _localPlayer -> removeItemById(item.getId());
    target->addItem(item);
}

/**
 * Handles the human player passing an item to the right neighbor.
 */
void GameScene::handlePassRight() {
    Player* target = _localPlayer -> getRightPlayer();
    
    if (!target || !target->isAlive() || _localPlayer -> getInventory().empty()) return;
    ItemInstance item = _localPlayer -> getInventory()[0];
    _localPlayer -> removeItemById(item.getId());
    target->addItem(item);
}

void GameScene::update(float dt) {
    if (!_active) {
        return;
    }

    if (_host) {
        //if we are the host, do host-exlucisve updates that will get sent out
        //example: boss behavior
    }

    _itemController.update(dt, _localPlayer);
    syncInventoryWidgets();
    
    if (_localPlayer -> isAlive()) {
        switch (_input.getAction()) {
            case InputController::Action::DROP_BOSS:       handleAttack();       break;
            case InputController::Action::DROP_ALLY_LEFT:  handleSupportLeft();  break;
            case InputController::Action::DROP_ALLY_RIGHT: handleSupportRight(); break;
            case InputController::Action::PASS_LEFT:       handlePassLeft();     break;
            case InputController::Action::PASS_RIGHT:      handlePassRight();    break;
            default: break;
        }
        _input.resetAction();
    }

    if (_enemy) {
        for (auto& ai : _playerAIs) {
            ai->update(dt, *_enemy, _itemController);
        }
    }
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void GameScene::dispose() {
    if (_active) {
        removeAllChildren();
        _gameArea = nullptr;
        _inventory = nullptr;
        _attackArea = nullptr;
        _bossNode = nullptr;
        _playerSlots.clear();
        _abilityIcons.clear();
        _active = false;
    }
}

/**
 * Sets whether the scene is currently active
 *
 * This method should be used to toggle all the UI elements.  Buttons
 * should be activated when it is made active and deactivated when
 * it is not.
 *
 * @param value whether the scene is currently active
 */
void GameScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            //put anything that needs to be activated as part of the scene
        }
        else {
            //deactivate any children that are part of the scene
        }
    }
}

/** Create and return an item Widget with a given ItemInstance */
std::shared_ptr<SceneNode> GameScene::createItemWidget(const ItemInstance& item) {
    auto itemDef = _itemController.getDatabase().getDef(item.getDefId());
    if (!itemDef) return nullptr;

    const std::string textureKey =
        (itemDef->getType() == ItemDef::Type::Attack) ? "attack" : "heal";
    
    auto texture = _assets->get<cugl::graphics::Texture>(textureKey);
    if (!texture) return nullptr;
    
    auto widget = PolygonNode::allocWithTexture(texture);
    
    widget->setContentSize(Size(64,64));
    widget->setAnchor(Vec2::ANCHOR_BOTTOM_LEFT);
    widget->setName("item_" + std::to_string((unsigned long long)item.getId()));
    _inventory->addChild(widget);
    return widget;
}

/** Return the world position for an item widget's initial inventory position */
cugl::Vec2 GameScene::getInitialInventoryPosition() const {
    cugl::Size size = _inventory->getContentSize();
    return cugl::Vec2(size.width * 0.5f, size.height * 0.5f);
}

/** Sync player inventory and item widgets displayed on screen */
void GameScene::syncInventoryWidgets() {
    if (!_inventory || !_localPlayer) {
        return;
    }
    
    std::unordered_set<ItemInstance::ItemId> liveIds;
    
    for (const ItemInstance& item : _localPlayer->getInventory()) {
        ItemInstance::ItemId id = item.getId();
        liveIds.insert(id);
        
        auto found = _itemWidgets.find(id);
        if (found == _itemWidgets.end()) {
            auto widget = createItemWidget(item);
            if (!widget) {
                continue;
            }
            
            widget->setPosition(getInitialInventoryPosition());
            
            _itemWidgets.emplace(id, widget);
        }
    }
    
    for (auto it = _itemWidgets.begin(); it != _itemWidgets.end(); ) {
        if (liveIds.find(it->first) == liveIds.end()) {
            if (it->second) {
                _inventory->removeChild(it->second);
            }
            it = _itemWidgets.erase(it);
        } else {
            ++it;
        }
    }
}
