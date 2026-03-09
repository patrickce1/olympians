#include <cugl/cugl.h>
#include <algorithm>
#include <iostream>
#include <random>
#include <sstream>
#include <unordered_set>
#include "GameScene.h"

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
 * Builds the five rectangular input zones from the current scene dimensions.
 * Must be called after Scene2::initWithHint() so getSize() is valid.
 */
void GameScene::initInputZones() {
    Size dimen = getSize();
    float w = dimen.width;
    float h = dimen.height;
    _inputZones = {
        {InputController::Action::DROP_BOSS,       Rect(w * 0.15f, h * 0.5f, w * 0.7f,  h * 0.5f)},
        {InputController::Action::DROP_ALLY_LEFT,  Rect(0,         h * 0.5f, w * 0.15f, h * 0.5f)},
        {InputController::Action::DROP_ALLY_RIGHT, Rect(w * 0.85f, h * 0.5f, w * 0.15f, h * 0.5f)},
        {InputController::Action::PASS_LEFT,       Rect(0,         0,        w * 0.15f, h * 0.5f)},
        {InputController::Action::PASS_RIGHT,      Rect(w * 0.85f, 0,        w * 0.15f, h * 0.5f)}
    };
}

/**
 * Loads the scene graph from assets, resizes it to the current scene
 * dimensions, and wires up all named child node references.
 * Must be called after _assets is assigned and Scene2::initWithHint() succeeds.
 *
 * @return true if the root scene node was found in the asset manager.
 */
bool GameScene::initSceneGraph() {
    Size dimen = getSize();

    _scene = _assets->get<scene2::SceneNode>("gameScene");
    if (!_scene) {
        CULogError("Scene NOT here!");
        return false;
    }

    _scene->setContentSize(dimen);
    _scene->doLayout();

    _gameArea  = _scene->getChildByName("gameArea");
    _inventory = _scene->getChildByName("inventory");
    _resetBtn  = _scene->getChildByName("resetButton");

    if (_gameArea) {
        // Left and right teammate icon
        _playerSlots.push_back(_gameArea->getChildByName("leftIcon"));
        _playerSlots.push_back(_gameArea->getChildByName("rightIcon"));
        
        _leftPlayerName = std::dynamic_pointer_cast<scene2::Label>(
             _assets->get<scene2::SceneNode>("gameScene.gameArea.leftName.username"));
        
        _rightPlayerName = std::dynamic_pointer_cast<scene2::Label>(
             _assets->get<scene2::SceneNode>("gameScene.gameArea.rightName.username"));
        
        _bossHealthBar = std::dynamic_pointer_cast<scene2::ProgressBar>(
               _assets->get<scene2::SceneNode>("gameScene.gameArea.enemyHealth.healthFill"));
    }
    
    if (_inventory) {
        _playerHealthBar = std::dynamic_pointer_cast<scene2::ProgressBar>(
            _assets->get<scene2::SceneNode>("gameScene.inventory.playerHealth.healthBarFill"));
    }
    
    addChild(_scene);
    return true;
}

/**
 * Initialises the ItemController and GameState.
 * ItemController must be initialised first because GameState::init()
 * needs the item database to finish setting up AI players.
 *
 * @return true if both systems initialised successfully.
 */
bool GameScene::initGameSystems() {
    if (!_itemController.init(_assets)) {
        return false;
    }
    if (!_gameState.init(_itemController)) {
        return false;
    }
    return true;
}

/**
 * Initialises the scene graph and all game systems.
 *
 * Builds the scene graph from the asset manager, initialises the
 * ItemController, and delegates world-state construction (players,
 * enemy, AI) to GameState::init(). Does not activate the scene —
 * call setActive(true) when ready to receive input.
 *
 * @param assets  The loaded asset manager.
 * @return true if initialisation succeeded, false otherwise.
 */
bool GameScene::init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController) {
    if (assets == nullptr) {
        return false;
    }
    if (!Scene2::initWithHint(Size(0, SCENE_HEIGHT))) {
        return false;
    }

    _assets = assets;
    _network = networkController;

    initInputZones();

    if (!initSceneGraph()) {
        return false;
    }

    if (!initGameSystems()) {
        return false;
    }

    setLocalPlayer(0);

    setActive(false);
    return true;
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void GameScene::dispose() {
    if (_active) {
        removeAllChildren();
        _gameArea   = nullptr;
        _inventory  = nullptr;
        _attackArea = nullptr;
        _bossNode   = nullptr;
        _leftPlayerName = nullptr;
        _rightPlayerName = nullptr;
        _bossHealthBar = nullptr;
        _playerHealthBar = nullptr;
        _network = nullptr;
        _playerSlots.clear();
        _gameState.dispose();
        _active = false;
    }
}

/**
 * Activates or deactivates the scene and its UI.
 * Calls reset() and enters the idle enemy state on activation.
 */
void GameScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            reset();
            _enemyController.enterIdle(_gameState.getEnemy(), _gameState.getPlayers());
        }
    }
    if (_network && _network->checkConnection() == NetworkController::CONNECTED) {
        //check who are real players
        //will change once we have player reordering in
        CULog("I am player %d", _network->getLocalPlayerNumber());
        for (int i = 0; i < _network->checkLobbyOrder().size(); i++) {
            _gameState.setRealPlayer(i, _network->checkLobbyOrder()[i].second);
        }
            //assign our own number
       setLocalPlayer(_network->getLocalPlayerNumber());
       
       _leftPlayerName->setText(_gameState.getLocalPlayer()->getLeftPlayer()->getPlayerName());
       _rightPlayerName->setText(_gameState.getLocalPlayer()->getRightPlayer()->getPlayerName());
    }
}

/**
 * Resets the scene to its start-of-round state.
 * Clears all item widgets, cancels any active drag, resets the glow
 * effect, and clears every player's inventory via GameState::reset().
 */
void GameScene::reset() {
    _activeIcon = nullptr;
    _glowAction = InputController::Action::NONE;
    _glowTimer  = 0;

    for (auto& [id, widget] : _itemWidgets) {
        if (widget && _inventory) {
            _inventory->removeChild(widget);
        }
    }
    _itemWidgets.clear();

    // Delegate inventory clearing to the model.
    _gameState.reset();
}

#pragma mark -
#pragma mark Player Assignment

/**
 * Assigns the local player slot for this machine.
 * Delegates to GameState::setLocalPlayer().
 */
void GameScene::setLocalPlayer(int assignedIndex) {
    _gameState.setLocalPlayer(assignedIndex);
}

#pragma mark -
#pragma mark Action Handlers

/**
 * Handles the local player dropping an attack item on the boss zone.
 */
void GameScene::handleAttack() {
    auto enemy    = _gameState.getEnemy();
    Player* local = _gameState.getLocalPlayer();
    if (!enemy || !local) return;

    for (const ItemInstance& item : local->getInventory()) {
        auto def = _itemController.getDatabase().getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Attack) {
            local->useItemById(item.getId(), *enemy, _itemController.getDatabase());
            CULog("Player attacked enemy '%s' with item %llu",
                  enemy->getId().c_str(), (unsigned long long)item.getId());
            return;
        }
    }
}

/**
 * Handles the local player dropping a support item on the left ally zone.
 */
void GameScene::handleSupportLeft() {
    Player* local  = _gameState.getLocalPlayer();
    Player* target = local ? local->getLeftPlayer() : nullptr;
    if (!target || !target->isAlive()) return;

    for (const ItemInstance& item : local->getInventory()) {
        auto def = _itemController.getDatabase().getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Support) {
            local->useItemById(item.getId(), *target, _itemController.getDatabase());
            return;
        }
    }
}

/**
 * Handles the local player dropping a support item on the right ally zone.
 */
void GameScene::handleSupportRight() {
    Player* local  = _gameState.getLocalPlayer();
    Player* target = local ? local->getRightPlayer() : nullptr;
    if (!target || !target->isAlive()) return;

    for (const ItemInstance& item : local->getInventory()) {
        auto def = _itemController.getDatabase().getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Support) {
            local->useItemById(item.getId(), *target, _itemController.getDatabase());
            return;
        }
    }
}

/**
 * Passes the first item in the local player's inventory to the left neighbour.
 */
void GameScene::handlePassLeft() {
    CULog("HELLO I AM THE LEFT PASS");
    Player* local  = _gameState.getLocalPlayer();
    Player* target = local ? local->getLeftPlayer() : nullptr;
    //if (!target || !target->isAlive() || local->getInventory().empty()) return;

    ItemInstance item = local->getInventory()[0];
    local->removeItemById(item.getId());

    CULog("Passing left to player %d", target->getPlayerNumber());

    if (!target->isAI()) {
        CULog("Passing left to a real player");
    }
    _network->broadcastPass(item.getDefId(), target->getPlayerNumber());
}

/**
 * Passes the first item in the local player's inventory to the right neighbour.
 */
void GameScene::handlePassRight() {
    CULog("HELLO I AM THE RIGHT PASS");
    Player* local  = _gameState.getLocalPlayer();
    Player* target = local ? local->getRightPlayer() : nullptr;
    //if (!target || !target->isAlive() || local->getInventory().empty()) return;

    ItemInstance item = local->getInventory()[0];
    local->removeItemById(item.getId());
    if (!target->isAI()) {
        CULog("Passing right to a real player");
    }
    CULog("I am passing to player %d", target->getPlayerNumber());
    _network->broadcastPass(item.getDefId(), target->getPlayerNumber());
}

void GameScene::updateInventoryPasses(std::vector<NetworkController::PassMessage> passes) {
    if (passes.size() > 0) {
        CULog("I got called, there were some passe");
    }
    for (NetworkController::PassMessage pass : passes) {
        Player* player = _gameState.getPlayerById(pass.playerID);
        CULog("I'm in savage mode");
        //for now, passing just gives a random item in the player inventory
        _itemController.giveRandomItem(_gameState.getLocalPlayer());
        //IN THE FUTURE, can we just give the ItemController a makeItem function that takes in a specific item id
        //so that I can just do pass.itemID
        // the ID is based on the one from the database
    }
}

/**
 * Dispatches the resolved drop-zone action to the appropriate handler
 * and resets the input action afterwards.
 */
void GameScene::handlePlayerActions(InputController::Action action) {
    Player* local = _gameState.getLocalPlayer();
    if (!local || !local->isAlive()) return;

    switch (action) {
        case InputController::Action::DROP_BOSS:       handleAttack();       break;
        case InputController::Action::DROP_ALLY_LEFT:  handleSupportLeft();  break;
        case InputController::Action::DROP_ALLY_RIGHT: handleSupportRight(); break;
        case InputController::Action::PASS_LEFT:       handlePassLeft();     break;
        case InputController::Action::PASS_RIGHT:      handlePassRight();    break;
        default: break;
    }
    //input.resetAction();
}

#pragma mark -
#pragma mark Update Helpers

/**
 * Ticks the enemy controller and all AI-controlled players forward by one frame.
 */
void GameScene::updateEnemyAndAI(float dt) {
    auto enemy = _gameState.getEnemy();
    if (!enemy || !enemy->isAlive()) return;

    _enemyController.update(dt, enemy, _gameState.getPlayers());

    for (auto& player : _gameState.getPlayers()) {
        if (auto* ai = dynamic_cast<PlayerAI*>(player.get())) {
            ai->update(dt, *enemy, _itemController);
        }
    }
}

/**
 * Updates the progress bar with the current ratios of player and enemy health.
 */
void GameScene::updatePlayerAndEnemyHealthUI(float dt) {
    auto enemy = _gameState.getEnemy();
    if (!enemy || !enemy->isAlive()) return;
    
    _bossHealthBar->setProgress(enemy->getCurrentHealth()/enemy->getMaxHealth());
    
    auto player = _gameState.getLocalPlayer();
    _playerHealthBar->setProgress(player->getCurrentHealth()/player->getMaxHealth());
}

/**
 * Checks whether the reset button was tapped and calls reset() if so.
 */
void GameScene::handleResetButton(InputController& input) {
    if (!input.touchEnded() || _activeIcon || !_resetBtn) return;

    Vec2 touchPosScreen = screenToWorldCoords(input.getTouchStart());
    if (_resetBtn->getBoundingBox().contains(touchPosScreen)) {
        CULog("Reset button tapped!");
        reset();
    }
}

/**
 * Classifies a drag-and-drop release into a drop zone, triggers the
 * appropriate action and glow effect, then clears the active icon.
 */
void GameScene::handleDropResolution(InputController& input) {
    if (!_activeIcon || !input.touchEnded()) return;

    Vec2 releaseWorld = screenToWorldCoords(input.getReleasePosition());
    InputController::Action finalAction = InputController::Action::NONE;

    for (const auto& pair : _inputZones) {
        if (pair.second.contains(releaseWorld)) {
            finalAction = pair.first;
            break;
        }
    }

    if (finalAction != InputController::Action::NONE) {
        handlePlayerActions(finalAction);
        resolveAction(finalAction);
        _glowAction = finalAction;
        _glowTimer  = _glowDuration;
        if (_activeIcon) {
            _activeIcon->setVisible(false);
        }
    }

    _activeIcon = nullptr;
}

/**
 * Decrements the glow timer each frame. Clears the active glow action
 * once the timer expires.
 */
void GameScene::tickGlowTimer(float dt) {
    if (_glowTimer <= 0) return;
    _glowTimer -= dt;
    if (_glowTimer <= 0) {
        _glowAction = InputController::Action::NONE;
    }
}

/**
 * Updates the debug pointer position in scene coordinates.
 */
void GameScene::updateDebugPointer(InputController& input) {
    if (!input.isTouching()) {
        _hasDebugPointer = false;
        return;
    }

    Vec2 current = input.isDragging()
        ? screenToWorldCoords(input.getDragPos())
        : screenToWorldCoords(input.getTouchStart());

    _debugPointerScene = current;
    _hasDebugPointer   = true;
}

/**
 * Hit-tests item widgets against the initial touch position.
 */
void GameScene::handleDragInitiation(InputController& input) {
    if (_activeIcon || !input.isDragging()) return;

    Vec2 touchPosScreen = screenToWorldCoords(input.getTouchStart());

    for (auto& [id, widget] : _itemWidgets) {
        if (!widget) continue;
        if (widget->getBoundingBox().contains(touchPosScreen)) {
            _activeIcon = widget;
            _dragOffset = widget->getPosition() - touchPosScreen;
            break;
        }
    }
}

/**
 * Moves the active dragged icon to follow the current touch position.
 */
void GameScene::handleDragTracking(InputController& input) {
    if (!_activeIcon || (!input.isTouching() && !input.isMouseDown())) return;

    Vec2 dragScene = screenToWorldCoords(input.getDragPos());
    _activeIcon->setPosition(dragScene + _dragOffset);
}

/**
 * Logs the resolved drop-zone action for debugging.
 */
void GameScene::resolveAction(InputController::Action action) {
    switch (action) {
    case InputController::Action::DROP_BOSS:
        CULog("boss drop");
        break;
    case InputController::Action::DROP_ALLY_LEFT:
        CULog("ally drop left");
        break;
    case InputController::Action::DROP_ALLY_RIGHT:
        CULog("ally drop right");
        break;
    case InputController::Action::PASS_LEFT:
        CULog("pass left");
        break;
    case InputController::Action::PASS_RIGHT:
        CULog("pass right");
        break;
    case InputController::Action::DRAG:
        CULog("Dragging");
        break;
    case InputController::Action::NONE:
        CULog("Nothing");
        break;
    case InputController::Action::PAUSE:
        CULog("Nothing");
        break;
    }
}

#pragma mark -
#pragma mark Update

/**
 * Processes one frame of game logic.
 */
void GameScene::update(float dt, InputController& input) {
    if (!_active) return;

    handleResetButton(input);
    handleDropResolution(input);

    if (input.touchEnded()) {
        input.resetAction();
    }

    tickGlowTimer(dt);
    updateDebugPointer(input);
    handleDragInitiation(input);
    handleDragTracking(input);

    _network->getNetworkUpdates();
    
    updateInventoryPasses(_network->getPassUpdates());
    _itemController.update(dt, _gameState.getLocalPlayer());
    syncInventoryWidgets();

    //handlePlayerActions(input);
    updateEnemyAndAI(dt);
    _network->clearQueues();
    updatePlayerAndEnemyHealthUI(dt);
}

#pragma mark -
#pragma mark Inventory UI

/** Creates a scene-node widget for the given item and adds it to the inventory container. */
std::shared_ptr<SceneNode> GameScene::createItemWidget(const ItemInstance& item) {
    auto itemDef = _itemController.getDatabase().getDef(item.getDefId());
    if (!itemDef) return nullptr;

    const std::string textureKey =
        (itemDef->getType() == ItemDef::Type::Attack) ? "attack" : "heal";

    auto texture = _assets->get<cugl::graphics::Texture>(textureKey);
    if (!texture) return nullptr;

    auto widget = PolygonNode::allocWithTexture(texture);
    widget->setContentSize(Size(74, 85));
    widget->setAnchor(Vec2::ANCHOR_BOTTOM_LEFT);
    widget->setName("item_" + std::to_string((unsigned long long)item.getId()));
    _inventory->addChild(widget);
    return widget;
}

/** Return a random in-bounds inventory position for a newly spawned item widget */
cugl::Vec2 GameScene::getRandomInventoryPosition(const cugl::Size& widgetSize) const {
    const cugl::Size inventorySize = _inventory->getContentSize();
    Size dimen = getSize();
    float w = dimen.width;

    const float maxX = std::max(0.0f, inventorySize.width - widgetSize.width - (w * 0.15f));
    const float maxY = std::max(0.0f, inventorySize.height - widgetSize.height);

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<float> xDist(w * 0.15f, maxX);
    std::uniform_real_distribution<float> yDist(0.0f, maxY);

    return cugl::Vec2(xDist(rng), yDist(rng));
}

/** Synchronises on-screen item widgets with the local player's current inventory. */
void GameScene::syncInventoryWidgets() {
    Player* local = _gameState.getLocalPlayer();
    if (!_inventory || !local) return;

    std::unordered_set<ItemInstance::ItemId> liveIds;

    for (const ItemInstance& item : local->getInventory()) {
        ItemInstance::ItemId id = item.getId();
        liveIds.insert(id);

        auto found = _itemWidgets.find(id);
        if (found == _itemWidgets.end()) {
            auto widget = createItemWidget(item);
            if (!widget) continue;
            widget->setPosition(getRandomInventoryPosition(widget->getContentSize()));
            _itemWidgets.emplace(id, widget);
        }
    }

    for (auto it = _itemWidgets.begin(); it != _itemWidgets.end();) {
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

#pragma mark -
#pragma mark Render

/** Draws a green debug outline around the reset button's bounding box. */
void GameScene::renderResetButton(cugl::graphics::SpriteBatch* batch) {
    if (!_resetBtn) return;
    Rect boundingBox = _resetBtn->getBoundingBox();
    Path2 path(boundingBox);
    batch->setColor(Color4(0, 255, 0, 150));
    batch->outline(path, Vec2::ZERO, Affine2::IDENTITY);
}

/** Draws zone outlines and a fading glow on the last successfully used zone. */
void GameScene::renderDropZones(cugl::graphics::SpriteBatch* batch) {
    for (auto& [action, rect] : _inputZones) {
        Path2 path(rect);
        if (action == _glowAction && _glowTimer > 0) {
            float t = _glowTimer / _glowDuration;
            Uint8 alpha = (Uint8)(150 * t);
            batch->setColor(Color4(0, 255, 0, alpha));
            batch->fill(path, Vec2::ZERO, Affine2::IDENTITY);
        }
        batch->setColor(Color4(0, 255, 0, 80));
        batch->outline(path, Vec2::ZERO, Affine2::IDENTITY);
    }
}

/** Draws a magenta outline around each visible item widget's bounding box. */
void GameScene::renderItemWidgetDebug(cugl::graphics::SpriteBatch* batch) {
    batch->setColor(Color4(255, 0, 255, 140));
    for (auto& [id, widget] : _itemWidgets) {
        if (!widget || !widget->isVisible()) continue;
        Path2 path(widget->getBoundingBox());
        batch->outline(path, Vec2::ZERO, Affine2::IDENTITY);
    }
}

/** Draws a small red square at the current touch position. */
void GameScene::renderPointerDebug(cugl::graphics::SpriteBatch* batch) {
    if (!_hasDebugPointer) return;
    Rect p(_debugPointerScene.x - 6.0f, _debugPointerScene.y - 6.0f, 12.0f, 12.0f);
    Path2 path(p);
    batch->setColor(Color4(255, 0, 0, 200));
    batch->fill(path, Vec2::ZERO, Affine2::IDENTITY);
    batch->setColor(Color4(255, 255, 255, 200));
    batch->outline(path, Vec2::ZERO, Affine2::IDENTITY);
}

/**
 * Custom render pass drawn after the standard scene graph render.
 */
void GameScene::render() {
    Scene2::render();

    auto batch = getSpriteBatch();
    batch->setPerspective(getCamera()->getCombined());
    batch->begin();

    renderResetButton(batch.get());
    renderDropZones(batch.get());
    renderItemWidgetDebug(batch.get());
    renderPointerDebug(batch.get());

    batch->end();
}
