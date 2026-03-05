#include <cugl/cugl.h>
#include <iostream>
#include <sstream>
#include "InputController.h"
#include "PlayerAI.h"
#include "EasyPlayerAI.h"
#include "CharacterLoader.h"
#include "GameScene.h"
#include "Enemy.h"

using namespace cugl;
using namespace cugl::scene2;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Example height for now, change as needed */
#define SCENE_HEIGHT 852

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

        _playerSlots.push_back(_gameArea->getChildByName("player"));
        _playerSlots.push_back(_gameArea->getChildByName("player3"));
        _playerSlots.push_back(_gameArea->getChildByName("player4"));
    }
    
    if (_attackArea) {
        _bossNode = _attackArea->getChildByName("enemy");
    }

    addChild(_scene);
    
    const std::string characterJsonPath = "json/characters.json";
    if (!_characterLoader.loadFromFile(characterJsonPath)) {
        CULog("Failed to load characters.json");
        return false;
    }

    // --- Build the player array ---
    // Index 0: human-controlled Player.
    // Indices 1-3: AI-controlled EasyPlayerAI (inherits PlayerAI -> Player).
    // All stored as shared_ptr<Player> so EnemyController, neighbor links,
    // and any other system that iterates _players works uniformly.
    _players.reserve(4);

    // Human player
    auto humanPlayer = std::make_shared<Player>("Percy", 1, "Player 1", _characterLoader);
    _players.push_back(humanPlayer);

    // AI players — EasyPlayerAI is constructed but NOT yet init()'d here
    // because init() needs the ItemController database which is set up below.
    for (int i = 1; i <= 3; i++) {
        auto aiPlayer = std::make_shared<EasyPlayerAI>("Percy", i + 1,
                                                       "Player " + std::to_string(i + 1),
                                                       _characterLoader);
        _players.push_back(aiPlayer);
    }

    // --- Circular neighbor linking ---
    // Links all 4 players in a ring: 0 <-> 1 <-> 2 <-> 3 <-> 0
    const int playerCount = (int)_players.size();
    for (int i = 0; i < playerCount; i++) {
        int leftIdx  = (i - 1 + playerCount) % playerCount;
        int rightIdx = (i + 1) % playerCount;
        _players[i]->setLeftPlayer(_players[leftIdx].get());
        _players[i]->setRightPlayer(_players[rightIdx].get());
    }

    // Sets the local player for this GameScene instance using an assigned index.
    // NOTE: ONLY HOST SHOULD RUN THIS — IMPORTANT FOR NETWORKING.
    setLocalPlayer(0);

    if (!_itemController.init(_assets)) {
        return false;
    }

    const std::string enemyJsonPath = "json/enemies.json";
    _enemy = std::make_shared<Enemy>();
    if (!_enemy->init("enemy1", enemyJsonPath)) {
        CULog("Failed to initialize enemy");
        return false;
    }
    CULog("GameScene: Enemy initialized id='%s'", _enemy->getId().c_str());

    // Finish initializing AI players now that _itemController is ready.
    // We cast each shared_ptr<Player> back to PlayerAI* for the AI-specific init().
    const std::string aiConfigPath = "json/playerAI.json";
    for (int i = 1; i < playerCount; i++) {
        auto* ai = dynamic_cast<PlayerAI*>(_players[i].get());
        if (!ai) {
            CULog("Player %d is not a PlayerAI — skipping AI init", i);
            continue;
        }
        if (!ai->init(_itemController.getDatabase(), aiConfigPath)) {
            CULog("Failed to initialize AI for player %d", i);
            return false;
        }
        CULog("Initialized AI for player %d", i);
    }

    setActive(false);
    return true;
}

// Called after the network session and assigns all local machines to a network-given player slot
void GameScene::setLocalPlayer(int assignedIndex) {
    CUAssertLog(assignedIndex >= 0 && assignedIndex < (int)_players.size(),
                "Assigned index out of range");
    _localPlayer = _players[assignedIndex].get();
}

/**
 * Handles the local player dropping an item on the boss zone to attack.
 */
void GameScene::handleAttack() {
    if (!_enemy) return;
    
    for (const ItemInstance& item : _localPlayer->getInventory()) {
        auto def = _itemController.getDatabase().getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Attack) {
            _localPlayer->useItemById(item.getId(), *_enemy, _itemController.getDatabase());
            CULog("Player attacked enemy '%s' with item %llu", _enemy->getId().c_str(), (unsigned long long)item.getId());
            return;
        }
    }
}

/**
 * Handles the local player dropping an item on the left ally zone to support.
 */
void GameScene::handleSupportLeft() {
    Player* target = _localPlayer->getLeftPlayer();
    if (!target || !target->isAlive()) return;
    
    for (const ItemInstance& item : _localPlayer->getInventory()) {
        auto def = _itemController.getDatabase().getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Support) {
            _localPlayer->useItemById(item.getId(), *target, _itemController.getDatabase());
            return;
        }
    }
}

/**
 * Handles the human player dropping an item on the right ally zone to support.
 */
void GameScene::handleSupportRight() {
    Player* target = _localPlayer->getRightPlayer();
    if (!target || !target->isAlive()) return;
    
    for (const ItemInstance& item : _localPlayer->getInventory()) {
        auto def = _itemController.getDatabase().getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Support) {
            _localPlayer->useItemById(item.getId(), *target, _itemController.getDatabase());
            CULog("Player supported right ally with item %llu", (unsigned long long)item.getId());
            return;
        }
    }
}

/**
 * Handles the human player passing an item to the left neighbor.
 */
void GameScene::handlePassLeft() {
    Player* target = _localPlayer->getLeftPlayer();
    
    if (!target || !target->isAlive() || _localPlayer->getInventory().empty()) return;
    ItemInstance item = _localPlayer->getInventory()[0];
    _localPlayer->removeItemById(item.getId());
    target->addItem(item);
    CULog("Player passed item %llu to left ally", (unsigned long long)item.getId());
}

/**
 * Handles the human player passing an item to the right neighbor.
 */
void GameScene::handlePassRight() {
    Player* target = _localPlayer->getRightPlayer();
    
    if (!target || !target->isAlive() || _localPlayer->getInventory().empty()) return;
    ItemInstance item = _localPlayer->getInventory()[0];
    _localPlayer->removeItemById(item.getId());
    target->addItem(item);
    CULog("Player passed item %llu to right ally", (unsigned long long)item.getId());
}

void GameScene::update(float dt) {
    if (!_active) {
        return;
    }

    _itemController.update(dt, _localPlayer);
    syncInventoryWidgets();
    
    if (_localPlayer->isAlive()) {
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
    
    if (_enemy->isAlive()) {
        _enemyController.update(dt, _enemy, _players);
        
        // Tick AI update via virtual dispatch — works for any PlayerAI subclass.
        // Use isAI() rather than index position — any slot may be human or AI. Should only be run my the host.
        for (auto& player : _players) {
            if (auto* ai = dynamic_cast<PlayerAI*>(player.get())) {
                ai->update(dt, *_enemy, _itemController);
            }
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
        _players.clear(); // shared_ptrs released; objects destroyed if no other owners
        _localPlayer = nullptr;
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
            _enemyController.enterIdle(_enemy, _players);
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
    widget->setContentSize(Size(64, 64));
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
