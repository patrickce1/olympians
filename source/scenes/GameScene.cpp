#include <cugl/cugl.h>
#include <iostream>
#include <sstream>
#include "GameScene.h"
#include "../InputController.h"
#include "../playerAI/PlayerAI.h"
#include "../playerAI/EasyPlayerAI.h"
#include "../CharacterLoader.h"
#include "../Enemy.h"

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
    
    // Get game input zones
    float w = dimen.width;
    float h = dimen.height;
    _inputZones = {
        {InputController::Action::DROP_BOSS,       Rect(w * 0.15f, h * 0.5f, w * 0.7f,  h * 0.5f)},
        {InputController::Action::DROP_ALLY_LEFT,  Rect(0,         h * 0.5f, w * 0.15f, h * 0.5f)},
        {InputController::Action::DROP_ALLY_RIGHT, Rect(w * 0.85f, h * 0.5f, w * 0.15f, h * 0.5f)},
        {InputController::Action::PASS_LEFT,       Rect(0,         0,        w * 0.15f, h * 0.5f)},
        {InputController::Action::PASS_RIGHT,      Rect(w * 0.85f, 0,        w * 0.15f, h * 0.5f)}
    };
    
    // Acquire the scene built by the asset loader and resize it the scene.
    _scene = _assets->get<scene2::SceneNode>("gameScene");
    
    if (!_scene) {
        CULogError("Scene NOT here!");
        return false;
    }

    _scene->setContentSize(dimen);
    _scene->doLayout(); // Repositions the HUD
    
    // Elements setup from assets
    _gameArea  = _scene->getChildByName("gameArea");
    _inventory = _scene->getChildByName("inventory");
    _resetBtn  = _scene->getChildByName("resetButton");
    
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

/**
 * Checks if the reset button was tapped and resets the scene if so.
 * Only fires when no icon is being dragged and a touch just ended.
 *
 * Uses the input parameter passed from SceneLoader.
 */
void GameScene::handleResetButton(InputController& input) {
    if (!input.touchEnded() || _activeIcon || !_resetBtn) return;

    Vec2 touchPosScreen = screenToWorldCoords(input.getTouchStart());
    Rect boundingBox = _resetBtn->getBoundingBox();

    if (boundingBox.contains(touchPosScreen)) {
        CULog("Reset button tapped!");
        reset();
    }
}

/**
 * Resets all ability icons to their original positions and visibility.
 *
 * Called when the player taps the reset button. Cancels any active drag,
 * clears the glow effect, and restores every icon to its initial state.
 */
void GameScene::reset() {
    _activeIcon = nullptr;                          // Cancel any in-progress drag
    _glowAction = InputController::Action::NONE;    // Clear the glow action
    _glowTimer  = 0;                                // Stop the glow timer
    for (auto& [id, widget] : _itemWidgets) {
        if (widget != nullptr && _inventory != nullptr) {
            _inventory->removeChild(widget);
        }
    }
    
    // Clear the item widgets
    _itemWidgets.clear();
    
    if (_localPlayer) {
        _localPlayer->clearInventory();
    }
}

/**
 * Resolves a drag-and-drop release by classifying the release position
 * into a drop zone and triggering the appropriate action and glow effect.
 * Clears the active icon after resolution.
 *
 * Uses the input parameter passed from SceneLoader.
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
 * Ticks down the glow timer each frame. Once expired, clears the active
 * glow action so the visual feedback stops rendering.
 *
 * @param dt  Delta time in seconds since the last frame.
 */
void GameScene::tickGlowTimer(float dt) {
    if (_glowTimer <= 0) return;

    _glowTimer -= dt;
    if (_glowTimer <= 0) {
        _glowAction = InputController::Action::NONE;
    }
}

/**
 * Dispatches the local player's current input action to the appropriate
 * handler (attack, support, or pass). Resets the action after handling.
 * Does nothing if the local player is not alive.
 */
void GameScene::handlePlayerActions(InputController& input) {
    if (!_localPlayer->isAlive()) return;

    switch (input.getAction()) {
        case InputController::Action::DROP_BOSS:       handleAttack();       break;
        case InputController::Action::DROP_ALLY_LEFT:  handleSupportLeft();  break;
        case InputController::Action::DROP_ALLY_RIGHT: handleSupportRight(); break;
        case InputController::Action::PASS_LEFT:       handlePassLeft();     break;
        case InputController::Action::PASS_RIGHT:      handlePassRight();    break;
        default: break;
    }
    input.resetAction();
}

/**
 * Ticks the enemy controller and all AI players forward by one frame.
 * The enemy controller handles enemy behaviour and attacks against players.
 * Each AI player is updated via virtual dispatch, so any PlayerAI subclass
 * is handled correctly regardless of which slot it occupies.
 * Should only be called by the host in a networked session.
 *
 * @param dt  Delta time in seconds since the last frame.
 */
void GameScene::updateEnemyAndAI(float dt) {
    if (!_enemy->isAlive()) return;

    _enemyController.update(dt, _enemy, _players);

    for (auto& player : _players) {
        if (auto* ai = dynamic_cast<PlayerAI*>(player.get())) {
            ai->update(dt, *_enemy, _itemController);
        }
    }
}

/**
 * Tracks the current pointer position in scene coordinates for debug rendering.
 * Sets _hasDebugPointer to false when no touch is active.
 *
 * Uses the input parameter passed from SceneLoader.
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
 * Hit-tests inventory item widgets against the initial touch position.
 * If a touch begins on a widget, begins dragging it and stores the
 * offset so the icon doesn't snap its center to the finger.
 *
 * Uses the input parameter passed from SceneLoader.
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
 * Moves the active dragged icon to follow the finger each frame.
 * Applies the stored drag offset so movement feels natural.
 *
 * Uses the input parameter passed from SceneLoader.
 */
void GameScene::handleDragTracking(InputController& input) {
    if (!_activeIcon || (!input.isTouching() && !input.isMouseDown())) return;

    Vec2 dragScene = screenToWorldCoords(input.getDragPos());
    _activeIcon->setPosition(dragScene + _dragOffset);
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
}

/**
 * Logs the action that was resolved from a drag-and-drop gesture.
 *
 * Called after a successful drop zone classification to confirm which
 * action was triggered. Currently logs each case for debugging purposes —
 * replace CULog calls with real game logic as each action is implemented.
 *
 * @param action  The action to resolve, as classified by the drop zone hit-test.
 */
void GameScene::resolveAction(InputController::Action action){
    switch (action){
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

/**
 * Processes one frame of game logic.
 *
 * Delegates to focused helper methods for each responsibility:
 *  1. Reset button tap detection.
 *  2. Drag-and-drop release zone classification.
 *  3. Glow timer decay.
 *  4. Debug pointer tracking.
 *  5. Drag initiation hit-testing.
 *  6. Active icon position tracking.
 *  7. Item controller and inventory sync.
 *
 * @param dt  Delta time in seconds since the last frame.
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

    _itemController.update(dt, _localPlayer);
    syncInventoryWidgets();

    handlePlayerActions(input);
    updateEnemyAndAI(dt);
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
        _players.clear(); // shared_ptrs released; objects destroyed if no other owners
        _localPlayer = nullptr;
        _active = false;
    }
}

/**
 * Draws a green debug outline around the reset button's bounding box.
 * Only draws if the reset button node exists in the scene graph.
 *
 * @param batch  The active sprite batch to draw into.
 */
void GameScene::renderResetButton(cugl::graphics::SpriteBatch* batch) {
    if (!_resetBtn) return;
    Rect boundingBox = _resetBtn->getBoundingBox();
    Path2 path(boundingBox);
    batch->setColor(Color4(0, 255, 0, 150));
    batch->outline(path, Vec2::ZERO, Affine2::IDENTITY);
}

/**
 * Draws a faint green outline for every input zone, plus a fading
 * filled green overlay on whichever zone most recently received a drop.
 * The fill alpha decays over _glowDuration seconds.
 *
 * @param batch  The active sprite batch to draw into.
 */
void GameScene::renderDropZones(cugl::graphics::SpriteBatch* batch) {
    for (auto& [action, rect] : _inputZones) {
        Path2 path(rect);

        if (action == _glowAction && _glowTimer > 0) {
            float timeRemaining = _glowTimer / _glowDuration;
            Uint8 alpha = (Uint8)(150 * timeRemaining);
            batch->setColor(Color4(0, 255, 0, alpha));
            batch->fill(path, Vec2::ZERO, Affine2::IDENTITY);
        }

        batch->setColor(Color4(0, 255, 0, 80));
        batch->outline(path, Vec2::ZERO, Affine2::IDENTITY);
    }
}

/**
 * Draws a magenta outline around each visible item widget's bounding box.
 * Useful for verifying hit-test regions during development.
 *
 * @param batch  The active sprite batch to draw into.
 */
void GameScene::renderItemWidgetDebug(cugl::graphics::SpriteBatch* batch) {
    batch->setColor(Color4(255, 0, 255, 140));
    for (auto& [id, widget] : _itemWidgets) {
        if (!widget || !widget->isVisible()) continue;
        Rect box = widget->getBoundingBox();
        Path2 path(box);
        batch->outline(path, Vec2::ZERO, Affine2::IDENTITY);
    }
}

/**
 * Draws a small red filled square at the current pointer position,
 * outlined in white. Only draws when a touch is active.
 *
 * @param batch  The active sprite batch to draw into.
 */
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
 *
 * Draws translucent debug/gameplay overlays using the sprite batch:
 *  - A green outline around the reset button for visual debugging.
 *  - Five rectangular drop zones, each outlined in green.
 *  - A filled green glow that fades out over the zone matching the most
 *    recent successful drop action.
 *  - Magenta outlines around visible item widget bounding boxes.
 *  - A red square at the current pointer position.
 *
 * The zones use scene coordinates (bottom-left origin) directly because
 * the sprite batch already uses the scene camera.
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
            reset();
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
