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
 * Initializes touch/mouse input zones mapped to game actions.
 *
 * Divides the screen into named rectangular regions scaled to the current
 * scene dimensions. Populates _attackZones (top center, DROP_BOSS),
 * _supportZones (top sides, DROP_ALLY), and _passZones
 * (bottom sides, PASS_LEFT / PASS_RIGHT).
 *
 */
void GameScene::initInputZones(){
    Size dimen = getSize();
    float w = dimen.width;
    float h = dimen.height;
    
    _attackZones = {{InputController::Action::DROP_BOSS, Rect(w * 0.05f, h * 0.4f, w * 0.9f, h * 0.47f)}};
    
    _supportZones = {
        {InputController::Action::DROP_ALLY_LEFT,  Rect(0, h * 0.45f, w * 0.15f, h * 0.40f)},
        {InputController::Action::DROP_ALLY_RIGHT, Rect(w * 0.85f, h * 0.45f, w * 0.15f, h * 0.40f)}
    };
    
    _passZones = {
        {InputController::Action::PASS_LEFT,  Rect(0, 0, w * 0.15f, h * 0.35f)},
        {InputController::Action::PASS_RIGHT, Rect(w * 0.85f, 0, w * 0.15f, h * 0.35f)}
    };
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

    /*since networking not initialized yet, just assume we are the host
    we recheck if we are player 0 whenever another scene transitions back into this one*/
    setLocalPlayer(0);
    _status = Status::PLAYING;
    setDebugMode(false);
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
        _activeIcon = nullptr;
        _playerSlots.clear();
        _gameState.dispose();
        _active = false;
    }
}

/**
 * Syncs the local game state with the current network player order.
 *
 * Iterates through the lobby's finalized player list and promotes each
 * slot from an AI placeholder to a real human player via setRealPlayer().
 * Then sets the local player index so this machine knows which player
 * it controls, and updates the teammate name labels to show the correct
 * left and right neighbors.
 *
 * NOTE: For now, assumes real players always occupy the first N consecutive slots
 * in the player array. Will need to be updated if player reordering is
 * added in the future.
 *
 * Does nothing if the network is not connected.
 */
void GameScene::updateNetworkOrder() {
    if (_network && _network->checkConnection() == NetworkController::CONNECTED) {
        const auto& networkedPlayers = _network->getNetworkedPlayers();
        for (int i = 0; i < (int)networkedPlayers.size(); i++) {
            _gameState.setRealPlayer(
                i,
                networkedPlayers[i].username,
                networkedPlayers[i].houseID   // ← new third argument
            );
        }

        setLocalPlayer(_network->getLocalPlayerNumber());

        _leftPlayerName->setText(_gameState.getLocalPlayer()->getLeftPlayer()->getPlayerName());
        _rightPlayerName->setText(_gameState.getLocalPlayer()->getRightPlayer()->getPlayerName());
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
    updateNetworkOrder();
}

/**
 * Resets the scene to its start-of-round state.
 * Clears all item widgets, cancels any active drag, resets the glow
 * effect, and clears every player's inventory via GameState::reset().
 */
void GameScene::reset() {
    _activeIcon = nullptr;
    _glowAction = InputController::Action::NONE;
    _status = Status::PLAYING;
    _glowTimer  = 0;
    _slotsDemotedToAI.clear();

    for (auto& [id, widget] : _itemWidgets) {
        if (widget && _inventory) {
            _inventory->removeChild(widget);
        }
    }
    _itemWidgets.clear();

    // Delegate inventory clearing and health resetting to the model.
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
            //NETWORKING
            if (!_network->isHost()) {
                _network->broadcastDamage(def->getEffectiveValue());
            }
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
            //NETWORK
            if (_network->isHost()) {
                target->updateHealth(def->getEffectiveValue());
            }
            else {
                _network->broadcastHeal(def->getEffectiveValue(), target->getPlayerNumber());
            }
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
            //NETWORK
            if (_network->isHost()) {
                target->updateHealth(def->getEffectiveValue());
            }
            else {
                _network->broadcastHeal(def->getEffectiveValue(), target->getPlayerNumber());
            }
            return;
        }
    }
}

/**
 * Passes the first item in the local player's inventory to the left neighbour.
 */
void GameScene::handlePassLeft() {
    Player* local  = _gameState.getLocalPlayer();
    Player* target = local ? local->getLeftPlayer() : nullptr;
    if (!target || !target->isAlive() || local->getInventory().empty()) return;

    ItemInstance item = local->getInventory()[0];
    local->removeItemById(item.getId());

    if (!target->isAI()) {
        CULog("Passing left to a real player with the number %d", target->getPlayerNumber());
    }
    else {
        CULog("Passing left to player AI player with number %d", target->getPlayerNumber());
    }

    //NETWORK
    _network->broadcastPass(item.getDefId(), target->getPlayerNumber());
}

/**
 * Passes the first item in the local player's inventory to the right neighbour.
 */
void GameScene::handlePassRight() {
    Player* local  = _gameState.getLocalPlayer();
    Player* target = local ? local->getRightPlayer() : nullptr;
    if (!target || !target->isAlive() || local->getInventory().empty()) return;

    ItemInstance item = local->getInventory()[0];
    local->removeItemById(item.getId());

    if (!target->isAI()) {
        CULog("Passing right to a real player with the number %d", target->getPlayerNumber());
    }
    else {
        CULog("Passing right to player AI player with number %d", target->getPlayerNumber());
    }

    //NETWORK
    _network->broadcastPass(item.getDefId(), target->getPlayerNumber());
}

/**
* Processes all the passMessages inside of the vector, putting the correct items in the player's inventory
* If we are the host, it will also give the correct items to the AI
* Intended usage: get the pass message vector from the network controller and pass into this function
*/
void GameScene::processNetworkedPasses(std::vector<PassMessage> passes) {
    for (PassMessage pass : passes) {
        Player* player = _gameState.getPlayerById(pass.playerID);
        //for now, passing just gives a random item in the player inventory
        _itemController.giveRandomItem(_gameState.getLocalPlayer());
    }
}
/**
 * Returns the definition for an item in the local player's inventory.
 *
 * @param itemId  The instance ID of the item to look up.
 * @return The item's definition, or nullptr if the local player does not
 *         exist or does not hold an item with the given ID.
 */
std::shared_ptr<const ItemDef> GameScene::getHeldItemDef(ItemInstance::ItemId itemId){
    Player* local = _gameState.getLocalPlayer();
    if (!local) {
        return nullptr;
    }
    for (const ItemInstance& item : local->getInventory()){
        if (item.getId() != itemId){
            continue;
        }
        return _itemController.getDatabase().getDef(item.getDefId());
    }
    return nullptr;
}


/**
 * Calls the appropriate handle action helper based on the input that we recieved
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
    if (!isDebugMode()){ return; }
    if (!input.touchEnded() || _activeIcon || !_resetBtn) return;

    Vec2 touchPosScreen = screenToWorldCoords(input.getTouchStart());
    if (_resetBtn->getBoundingBox().contains(touchPosScreen)) {
        CULog("Reset button tapped!");
        reset();
    }
}

/**
 * Handles the full pipeline of a player's drag-and-drop input for one frame.
 *
 * When the player releases a dragged item, this function:
 *   1. Checks if a drag-and-drop release occurred this frame.
 *   2. Determines which drop zone (if any) the item was released into.
 *   3. Validates that the local player is alive before acting.
 *   4. Dispatches the appropriate game action (attack, support, pass).
 *   5. Triggers a glow effect on the activated zone for visual feedback.
 *   6. Logs the action for debugging.
 *   7. Clears the active dragged icon.
 *
 * @param input     The input controller for this frame.
 */
void GameScene::handlePlayerInput(InputController& input) {
    if (!_activeIcon || !input.touchEnded()) return;

    // 1. Determine which drop zone the item was released into
    Vec2 releaseWorld = screenToWorldCoords(input.getReleasePosition());
    InputController::Action finalAction = InputController::Action::NONE;

    for (const auto& pair : _inputZones) {
        if (pair.second.contains(releaseWorld)) {
            finalAction = pair.first;
            break;
        }
    }

    if (finalAction != InputController::Action::NONE) {
        // 2. Dispatch to the appropriate action handler
        handlePlayerActions(finalAction);

        // 3. Trigger glow effect on the activated zone
        _glowAction = finalAction;
        _glowTimer  = _glowDuration;
        if (_activeIcon) {
            _activeIcon->setVisible(false);
        }
    }

    _activeIcon = nullptr;
    _draggedItemId = 0;
    _draggedItemDef = nullptr;
    updateInputZones();
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
    if (!isDebugMode()) {
        _hasDebugPointer = false;
        return;
    }
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
            _draggedItemId = id;
            _draggedItemDef = getHeldItemDef(id);
            updateInputZones();
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

/* Checks if any updates about the state of the game were sent over the network.
* If we are a client, we update the state of the game to match the hosts' version and process any passes sent to us.
* If we are the host, we process any attack, heal, and pass messages.
* After doing so, we send out a new authoritative version of the game state as the host*/
void GameScene::handleNetworkUpdates() {
    /*Networking pull cycle*/
    _network->getNetworkUpdates();

    if (_network->isHost()) {
        // handle incoming attack/heal messages from clients
        _gameState.attackUpdates(_network->getAttackUpdates());
        _gameState.healUpdates(_network->getHealUpdates());
        // broadcast authoritative state to all clients
        _network->broadcastGameState(_gameState);
        //check if we won or lost
        if (_gameState.checkWon()) {
            _network->broadcastWinGame();
            _status = Status::WON;
        }
        else if(_gameState.checkLost()){
            _network->broadcastLoseGame();
            _status = Status::LOST;
        }
    }
    else {
        // clients just apply the latest state from host
        _gameState.networkUpdate(_network->getStateUpdate());
        // clients check if the host told us anything about winning/losing
        if (_network->checkGameWon()) {
            _status = Status::WON;
            CULog("We were told that we won");
        }
        else if (_network->checkGameLost()) {
            _status = Status::LOST;
            CULog("We were told that we lost");
        }
    }

    processNetworkedPasses(_network->getPassUpdates());
}

/**
 * Spawns items for the local player every frame, and for all AI-controlled
 * players if this machine is the host. AI item spawning is host-only since
 * the host is the authoritative source for all AI state.
 *
 * @param dt  Delta time in seconds.
 */
void GameScene::handleItemSpawn(float dt) {
    // Always spawn items for the local human player.
    _itemController.update(dt, _gameState.getLocalPlayer());

    // Only the host spawns items for AI players, since the host is the
    // authoritative source for all AI state and broadcasts it to clients.
    if (!_network->isHost()) return;

    for (auto& player : _gameState.getPlayers()) {
        if (!player || !player->isAI()) continue;
        _itemController.update(dt, player.get());
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
    handlePlayerInput(input);
    input.resetAction();

    if (input.touchEnded()) {
        input.resetAction();
    }

    handleNetworkUpdates();
    handleDisconnectedPlayers();

    // now safe to iterate players
    handleItemSpawn(dt);
    syncInventoryWidgets();
    updateEnemyAndAI(dt);

    tickGlowTimer(dt);
    updateDebugPointer(input);
    handleDragInitiation(input);
    handleDragTracking(input);

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
    
    if (isDebugMode()){
        renderResetButton(batch.get());
        renderItemWidgetDebug(batch.get());
        renderPointerDebug(batch.get());
    }
    renderDropZones(batch.get());
    batch->end();
}

/**
 * Recreates the on-screen input zones depending on the item the player is holding.
 * If nothing is held, no zones are added to _inputZones.
 * If any item is held, the pass zones are added to _inputZones.
 * If an attack item is held, the attack zone is added to _inputZones.
 * If a support item is held, the support zones are added to _inputZones.
 */
void GameScene::updateInputZones(){
    if (!_draggedItemDef){
        _inputZones = {};
        return;
    }
    
    if (_draggedItemDef->getType() == ItemDef::Type::Attack){
        _inputZones = _attackZones;
    }
    else {
        _inputZones = _supportZones;
    }
    
    _inputZones.insert(_inputZones.end(), _passZones.begin(), _passZones.end());
}

/**
 * Enables or disables debug mode for the scene.
 *
 * When active, debug mode shows additional overlays and UI elements
 * to aid development, including the reset button, drop zone outlines,
 * item widget bounding boxes, and a touch position indicator.
 */
void GameScene::setDebugMode(bool enabled){
    _debugMode = enabled;
    if (_resetBtn) _resetBtn->setVisible(enabled);
}

/**
 * HOST ONLY. Builds a slot -> networkID map for every real (non-AI)
 * player and passes it to the NetworkController to diff against the
 * still-connected peer list. Populates _disconnectedSlots with any
 * newly-dropped slots.
 */
void GameScene::detectDroppedPeers() {
    std::unordered_map<int, std::string> activeNetworkIDs;
    const auto& networkedPlayers = _network->getNetworkedPlayers();

    for (const auto& player : _gameState.getPlayers()) {
        // Skip AI slots — they have no network peer to check.
        if (player->isAI()) continue;

        int slot = player->getPlayerNumber();

        // Skip our own slot — we are still here by definition.
        if (slot == _network->getLocalPlayerNumber()) continue;

        // networkID lives at the same index as slot in _onlinePlayers,
        // since lobby order and slot order are kept in sync.
        if (slot < (int)networkedPlayers.size()) {
            activeNetworkIDs[slot] = networkedPlayers[slot].networkID;
        }
    }
}

/**
 * HOST ONLY. Replaces the player at the given slot with an EasyPlayerAI,
 * re-wires the neighbour ring, and restores the disconnected player's
 * health and inventory onto the new AI.
 *
 * @param slot  The 0-based slot index of the disconnected player.
 */
void GameScene::demoteSlotToAI(int slot) {
    Player* player = _gameState.getPlayerBySlot(slot);
    if (!player) return;

    CULog("GameScene: host demoting slot %d to EasyPlayerAI", slot);

    // Snapshot the disconnected player's state before overwriting.
    float savedHealth    = player->getCurrentHealth();
    auto  savedInventory = player->getInventory();

    // Construct the replacement AI. GameScene owns this step because
    // _itemController and _gameState.getCharacterLoader() both live here.
    auto aiPlayer = std::make_shared<EasyPlayerAI>(
        player->getHouseName(),
        slot,
        player->getPlayerName(),
        _gameState.getHouseLoader()
    );
    aiPlayer->init(_itemController.getDatabase(), "json/playerAI.json");

    // Swap the slot in the player array.
    auto& players = _gameState.getPlayers();
    players[slot] = aiPlayer;

    // Re-wire the full circular neighbour ring so every player's
    // left/right pointers are valid after the swap.
    const int n = (int)players.size();
    for (int i = 0; i < n; i++) {
        players[i]->setLeftPlayer (players[(i - 1 + n) % n].get());
        players[i]->setRightPlayer(players[(i + 1)     % n].get());
    }

    // Restore the disconnected player's health and inventory onto the
    // new AI so the game continues without a state jump.
    aiPlayer->setCurrentHealth(savedHealth);
    for (const ItemInstance& item : savedInventory) {
        aiPlayer->addItem(item);
    }
}

/**
 * HOST + CLIENTS. Updates the left and right teammate name labels to
 * reflect the current AI/human state of each neighbour.
 */
void GameScene::refreshTeammateNameLabels() {
    Player* local = _gameState.getLocalPlayer();
    if (!local) return;

    if (_leftPlayerName && local->getLeftPlayer()) {
        _leftPlayerName->setText(
            local->getLeftPlayer()->isAI()
                ? "AI Player " + std::to_string(local->getLeftPlayer()->getPlayerNumber())
                : local->getLeftPlayer()->getPlayerName());
    }
    if (_rightPlayerName && local->getRightPlayer()) {
        _rightPlayerName->setText(
            local->getRightPlayer()->isAI()
                ? "AI Player " + std::to_string(local->getRightPlayer()->getPlayerNumber())
                : local->getRightPlayer()->getPlayerName());
    }
}

/**
 * Top-level disconnect handler. Called every frame from update().
 * Delegates to the three helpers below.
 */
void GameScene::handleDisconnectedPlayers() {
    if (!_network) return;

    // No polling needed — _disconnectedSlots is populated automatically
    // by the NetworkController's disconnect callback when any peer closes.

    for (int slot : _network->getDisconnectedSlots()) {

        // Skip slots we already handled in a previous frame.
        if (_slotsDemotedToAI.count(slot)) continue;

        Player* existing = _gameState.getPlayerBySlot(slot);

        // Skip if the slot is already AI or doesn't exist.
        if (!existing || existing->isAI()) continue;

        // Step 2a: Host replaces the player object with an EasyPlayerAI.
        // Clients skip this — their state is kept in sync each frame
        // via broadcastGameState / networkUpdate.
        if (_network->isHost()) {
            demoteSlotToAI(slot);
        }

        // Mark this slot as handled so we don't re-demote it next frame.
        _slotsDemotedToAI.insert(slot);

        // Step 2b: Both host and clients refresh the teammate name labels.
        refreshTeammateNameLabels();
    }
}
