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
    
    // Define screen input zones
    float w = dimen.width;
    float h = dimen.height;
    _zones = {
            {InputController::Action::DROP_BOSS,       Rect(w * 0.15f, h * 0.5f, w * 0.7f,  h * 0.5f)},
            {InputController::Action::DROP_ALLY_LEFT,  Rect(0,         h * 0.5f, w * 0.15f, h * 0.5f)},
            {InputController::Action::DROP_ALLY_RIGHT, Rect(w * 0.85f, h * 0.5f, w * 0.15f, h * 0.5f)},
            {InputController::Action::PASS_LEFT,       Rect(0,         0,        w * 0.15f, h * 0.5f)},
            {InputController::Action::PASS_RIGHT,      Rect(w * 0.85f, 0,        w * 0.15f, h * 0.5f)}
    };
    
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
    
    // ----- Game area sub-nodes -----
    if (_gameArea) {
        _attackArea = _gameArea->getChildByName("attackArea");

        _playerSlots.push_back(_gameArea->getChildByName("player"));
        _playerSlots.push_back(_gameArea->getChildByName("player3"));
        _playerSlots.push_back(_gameArea->getChildByName("player4"));
    }
    
    // ----- Attack area sub-nodes -----
    if (_attackArea) {
        _bossNode = _attackArea->getChildByName("enemy");
    }

    // Attach the fully-constructed scene graph to this Scene2
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
 * Processes one frame of game logic.
 *
 * Handles three main responsibilities each frame:
 *  1. Detecting taps on the reset button to restore ability icons.
 *  2. Managing the drag-and-drop lifecycle for ability icons:
 *     - On touch start: hit-test inventory icons to begin dragging.
 *     - While dragging: move the icon to follow the finger.
 *     - On touch end: resolve the drop action and trigger a glow effect.
 *  3. Ticking down the glow timer used for visual feedback on drop zones.
 *
 * Also forwards the update to the item controller for item-related logic.
 *
 * @param dt     Delta time in seconds since the last frame.
 */
void GameScene::update(float dt) {
    if (!_active) {
        return;
    }

    // --- Reset button tap detection ---
    // Only check when no icon is being dragged, touch just ended, and the button exists
    if (_input.touchEnded() && !_activeIcon && _resetBtn) {
        Vec2 touchPosRaw = _input.getTouchStart();
        Vec2 touchPosScreen = screenToWorldCoords(touchPosRaw);

        Rect boundingBox = _resetBtn->getBoundingBox();
        if (boundingBox.contains(touchPosScreen)) {
            CULog("Reset button tapped!");
            reset();
        }
    }
    
    // --- Zone classification: convert positions to world space and classify action ---
    if (_activeIcon && _input.touchEnded()) {
        Vec2 releaseWorld = screenToWorldCoords(_input.getReleasePosition());
        
        InputController::Action finalAction = InputController::Action::NONE;
        
        for (const auto& pair : _zones){
            if (pair.second.contains(releaseWorld)){
                finalAction = pair.first;
                break;
            }
        }
        
        if (finalAction != InputController::Action::NONE) {
                resolveAction(finalAction);         // Trigger the CULog and logic
                _glowAction = finalAction;          // Start the visual glow
                _glowTimer  = _glowDuration;
            if (_activeIcon != nullptr){
                _activeIcon->setVisible(false); // Consume the icon
            }
            } else {
                // Optional: snap the icon back to original position if missed
//                    reset();
            }
            
            _activeIcon = nullptr;
    }
    
    if (_input.touchEnded()){
        _input.resetAction();
    }
    
    // --- Drop resolution: touch ended while dragging an icon ---
    if (_activeIcon && _input.touchEnded()) {
        InputController::Action action = _input.getAction();
        
        // If the gesture resolved to a meaningful action (not NONE or still dragging),
        // trigger the glow feedback and hide the used icon
        if (action != InputController::Action::NONE && action != InputController::Action::DRAG) {
            _glowAction = action;          // Which zone should glow
            _glowTimer  = _glowDuration;   // Start the glow countdown
            _activeIcon->setVisible(false); // Hide the consumed ability icon
        }
        
        // Release the drag regardless of whether the action was valid
        _activeIcon = nullptr;
    }
    
    // --- Glow timer decay ---
    // The glow fades out over _glowDuration seconds after a successful drop
    if (_glowTimer > 0) {
        _glowTimer -= dt;
        if (_glowTimer <= 0) {
            _glowAction = InputController::Action::NONE; // Clear the glow once expired
        }
    }

    // --- Debug pointer tracking ---
    if (_input.isTouching()) {
        Vec2 current = _input.isDragging() ? screenToWorldCoords(_input.getDragPos()) : screenToWorldCoords(_input.getTouchStart());
        _debugPointerScene = current;
        _hasDebugPointer   = true;
    } else {
        _hasDebugPointer = false;
    }
    
    // --- Drag initiation: first touch on an inventory icon ---
    if (!_activeIcon && _input.isDragging()) {
        Vec2 touchPosRaw = _input.getTouchStart();
        Vec2 touchPosScreen = screenToWorldCoords(touchPosRaw);
        
        // Hit-test each ability icon to see if the touch landed on one
        for (auto& [id,widget] : _itemWidgets) {
            if (!widget) continue;
            cugl::Rect boundingBox = widget->getBoundingBox();
            if (boundingBox.contains(touchPosScreen)) {
            _activeIcon = widget;
            // Store the offset between the icon center and the touch point
            // so the icon doesn't "snap" its center to the finger
            _dragOffset = widget->getPosition() - touchPosScreen;
            break;
        }
        }
    }
    
    // --- Drag tracking: move the active icon to follow the finger ---
    if (_activeIcon && _input.isTouching()) {
        cugl::Vec2 dragScene       = screenToWorldCoords(_input.getDragPos());
        _activeIcon->setPosition(dragScene + _dragOffset);
    }
    
    // Forward to the item controller for any item-related per-frame logic
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

// Resolves input actions
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
 * Custom render pass drawn after the standard scene graph render.
 *
 * Draws translucent debug/gameplay overlays using the sprite batch:
 *  - A green outline around the reset button for visual debugging.
 *  - Five rectangular drop zones covering the upper half of the screen
 *    (boss center, ally left/right, pass left/right), each outlined in green.
 *  - A filled green glow that fades out over the zone matching the most
 *    recent successful drop action.
 *
 * The zones use scene coordinates (bottom-left origin) directly because
 * the sprite batch already uses the scene camera.
 */
void GameScene::render() {
    // First, render the standard scene graph (all SceneNode children)
    Scene2::render();
    
    // Begin a custom overlay pass using the scene's shared sprite batch
    auto batch = getSpriteBatch();
    batch->setPerspective(getCamera()->getCombined());
    batch->begin();
    // Debug outline around the reset button
    if (_resetBtn) {
        Rect boundingBox = _resetBtn->getBoundingBox();
        Path2 path(boundingBox);
        batch->setColor(Color4(0, 255, 0, 150));
        batch->outline(path, Vec2::ZERO, Affine2::IDENTITY);
    }
    
    // Draw each drop zone
    for (auto& [action, rect] : _zones) {
        Path2 path(rect);
        
        // If this zone matches the active glow, draw a fading filled overlay
        if (action == _glowAction && _glowTimer > 0) {
            float timeRemaining = _glowTimer / _glowDuration;    // Normalized time remaining [1 → 0]
            Uint8 alpha = (Uint8)(150 * timeRemaining);           // Fade from 150 → 0 alpha
            batch->setColor(Color4(0, 255, 0, alpha));
            batch->fill(path, Vec2::ZERO, Affine2::IDENTITY);
        }
        
        // Always draw a faint green outline for every zone
        batch->setColor(Color4(0, 255, 0, 80));
        batch->outline(path, Vec2::ZERO, Affine2::IDENTITY);
    }

    // Debug: outline ability icon bounding boxes
    batch->setColor(Color4(255, 0, 255, 140));
    for (auto& [id,widget] : _itemWidgets) {
        if (!widget || !widget->isVisible()) continue;
        Rect box = widget->getBoundingBox();
        Path2 path(box);
        batch->outline(path, Vec2::ZERO, Affine2::IDENTITY);
    }

    // Debug: show current pointer position
    if (_hasDebugPointer) {
        Rect p(_debugPointerScene.x - 6.0f, _debugPointerScene.y - 6.0f, 12.0f, 12.0f);
        Path2 path(p);
        batch->setColor(Color4(255, 0, 0, 200));
        batch->fill(path, Vec2::ZERO, Affine2::IDENTITY);
        batch->setColor(Color4(255, 255, 255, 200));
        batch->outline(path, Vec2::ZERO, Affine2::IDENTITY);
    }
    
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

        // 3. Clear the tracking map
        _itemWidgets.clear();

        CULog("GameScene: All items cleared and inventory reset.");
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
