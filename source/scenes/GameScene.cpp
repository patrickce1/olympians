#include <cugl/cugl.h>
#include <iostream>
#include <sstream>

#include "GameScene.h"

using namespace cugl;
using namespace cugl::scene2;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Use native display bounds for a 1:1 scene coordinate space */

#pragma mark -
#pragma mark Constructors
#define GAME_HEIGHT 852

/**
 * Initializes the controller contents and sets up the game scene.
 *
 * This method builds the scene graph from pre-loaded JSON assets, wires up
 * player slots, boss node, reset button), initializes the item controller
 * and enemy loader, and leaves the scene inactive until explicitly activated.
 *
 * The scene is not activated here because an active scene will still
 * receive input even when hidden. Activation is deferred to {@link #setActive}.
 *
 * @param assets  The (loaded) asset manager containing textures, JSON scenes, etc.
 * @return true if initialization succeeded, false otherwise.
 */
bool GameScene::init(const std::shared_ptr<cugl::AssetManager>& assets) {
    // Guard: assets must be valid
    if (assets == nullptr) {
        return false;
    }
    else if (!Scene2::initWithHint(Size(0, GAME_HEIGHT))){
        return false;
    }
    // Cache the asset manager and retrieve the computed scene dimensions
    _assets = assets;
    Size dimen = getSize();
    float w = dimen.width;
    float h = dimen.height;
    _zones = {
            {InputController::Action::DROP_BOSS,       Rect(w * 0.15f, h * 0.5f, w * 0.7f,  h * 0.5f)},
            {InputController::Action::DROP_ALLY_LEFT,  Rect(0,         h * 0.5f, w * 0.15f, h * 0.5f)},
            {InputController::Action::DROP_ALLY_RIGHT, Rect(w * 0.85f, h * 0.5f, w * 0.15f, h * 0.5f)},
            {InputController::Action::PASS_LEFT,       Rect(0,         0,        w * 0.15f, h * 0.5f)},
            {InputController::Action::PASS_RIGHT,      Rect(w * 0.85f, 0,        w * 0.15f, h * 0.5f)}
        };
    // Acquire the scene built by the asset loader and resize it to the scene.
    _scene = _assets->get<scene2::SceneNode>("gameScene");
    if (!_scene) {
        CULog("Scene NOT here!");
        return false;
    }
    _scene->setContentSize(dimen);
    _scene->doLayout(); // Repositions the HUD
    
    // ----- Wire up top-level children -----
    _gameArea  = _scene->getChildByName("gameArea");   // Upper region: players + boss
    _inventory = _scene->getChildByName("inventory");  // Lower bar: draggable ability icons
    _resetBtn  = _scene->getChildByName("resetButton"); // Button to reset ability icons
    
    // ----- Game area sub-nodes -----
    if (_gameArea) {
        // The central region where attacks are directed at the boss
        _attackArea = _gameArea->getChildByName("attackArea");
        
        // Slots representing the local player and two AI-controlled allies
        _playerSlots.push_back(_gameArea->getChildByName("player"));
        _playerSlots.push_back(_gameArea->getChildByName("player3"));
        _playerSlots.push_back(_gameArea->getChildByName("player4"));
    }
    
    // ----- Attack area sub-nodes -----
    if (_attackArea) {
        // The boss enemy widget inside the attack area
        _bossNode = _attackArea->getChildByName("enemy");
    }
    
    // ----- Inventory ability icons -----
    if (_inventory) {
        // Collect all six ability icons (2 heals + 4 attacks) from the inventory bar
        _abilityIcons.push_back(_inventory->getChildByName("heal"));
        _abilityIcons.push_back(_inventory->getChildByName("heal2"));
        _abilityIcons.push_back(_inventory->getChildByName("attack"));
        _abilityIcons.push_back(_inventory->getChildByName("attack4"));
        _abilityIcons.push_back(_inventory->getChildByName("attack5"));
        _abilityIcons.push_back(_inventory->getChildByName("attack6"));

        // Record each icon's starting position so we can restore them on reset
        for (auto& icon : _abilityIcons) {
            if (icon) {
                _abilityOriginalPos.push_back(icon->getPosition());
            }
        }
    }
    
    // Attach the fully-constructed scene graph to this Scene2
    addChild(_scene);
    
    // ----- Character loading -----
    const std::string characterJsonPath = "json/characters.json";
    if (!_characterLoader.loadFromFile(characterJsonPath)) {
        CULog("Failed to load characters.json");
        return false;
    }
    
    _players.emplace_back("Percy", 1, "Player 1", _characterLoader);
    _player = &_players.back();
    
    // Initialize the item controller (manages item logic / spawning)
    if (!_itemController.init(_assets)) {
        return false;
    }
    
    // ----- Enemy loading -----
    const std::string enemyJsonPath = "json/enemies.json";
    if (!_enemyLoader.loadFromFile(enemyJsonPath)) {
        // Non-fatal: log a warning and continue without an enemy
        CULog("GameScene: failed to load enemies from '%s' (continuing without enemy)",
              enemyJsonPath.c_str());
        _enemy.reset();
    } else {
        const std::string enemyId = "enemy1";
        
        if (!_enemyLoader.has(enemyId)) {
            // The JSON loaded but didn't contain the requested enemy ID
            CULog("GameScene: enemy id '%s' not found in '%s' (continuing without enemy)",
                  enemyId.c_str(), enemyJsonPath.c_str());
            _enemy.reset();
        } else {
            // Construct the enemy model from the loaded data
            _enemy = std::make_unique<Enemy>(enemyId, _enemyLoader);
            CULog("GameScene: spawned enemy '%s'", enemyId.c_str());
        }
    }
    
    // Start inactive — caller must explicitly activate the scene
    setActive(false);
    return true;
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
     * @param input  The shared input controller providing touch state and gesture data.
     */
    void GameScene::update(float dt, InputController& input) {
        if (!_active) return;
        // --- Reset button tap detection ---
        // Only check when no icon is being dragged, touch just ended, and the button exists
        if (input.touchEnded() && !_activeIcon && _resetBtn) {
            Vec2 touchPosRaw = input.getTouchStart();
            Vec2 touchPosScreen = screenToWorldCoords(touchPosRaw);

            Rect boundingBox = _resetBtn->getBoundingBox();
            if (boundingBox.contains(touchPosScreen)) {
                CULog("Reset button tapped!");
                reset();
            }
        }
        
        // --- Zone classification: convert positions to world space and classify action ---
        if (_activeIcon && input.touchEnded()) {
            Vec2 startWorld   = screenToWorldCoords(input.getTouchStart());
            Vec2 releaseWorld = screenToWorldCoords(input.getReleasePosition());
            float w = getSize().width;
            float h = getSize().height;
            
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
        if (input.touchEnded()){
            input.resetAction();
        }
        // --- Drop resolution: touch ended while dragging an icon ---
        if (_activeIcon && input.touchEnded()) {
            InputController::Action action = input.getAction();
            
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
        if (input.isTouching()) {
            Vec2 current = input.isDragging() ? screenToWorldCoords(input.getDragPos()) : screenToWorldCoords(input.getTouchStart());
            _debugPointerScene = current;
            _hasDebugPointer   = true;
        } else {
            _hasDebugPointer = false;
        }
        
        // --- Drag initiation: first touch on an inventory icon ---
        if (!_activeIcon && input.isDragging()) {
            Vec2 touchPosRaw = input.getTouchStart();
            Vec2 touchPosScreen = screenToWorldCoords(touchPosRaw);
            
            // Hit-test each ability icon to see if the touch landed on one
            for (auto& icon : _abilityIcons) {
                if (!icon) continue;
                cugl::Rect boundingBox = icon->getBoundingBox();
                if (boundingBox.contains(touchPosScreen)) {
                _activeIcon = icon;
                // Store the offset between the icon center and the touch point
                // so the icon doesn't "snap" its center to the finger
                _dragOffset = icon->getPosition() - touchPosScreen;
                break;
            }
            }
        }
        
        // --- Drag tracking: move the active icon to follow the finger ---
        if (_activeIcon && input.isTouching()) {
            cugl::Vec2 dragScene       = screenToWorldCoords(input.getDragPos());
            _activeIcon->setPosition(dragScene + _dragOffset);
        }
        
        // Forward to the item controller for any item-related per-frame logic
        _itemController.update(dt, _player);
        syncInventoryWidgets();
    }
    
    /**
     * The method called to update the scene.
     * 
     * This override is required by Scene2 but is not used directly.
     * The actual game logic is in update(float dt, InputController& input).
     *
     * @param dt  Delta time in seconds since the last frame.
     */
    void GameScene::update(float dt) {
        // This method is not used - the two-parameter version is called from SceneLoader
    }

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
     * Disposes of all (non-static) resources allocated to this scene.
     *
     * Removes every child node from the scene graph and nullifies cached
     * pointers. Safe to call even if the scene was never activated.
     */
    void GameScene::dispose() {
        if (_active) {
            removeAllChildren();
            _gameArea    = nullptr;
            _inventory   = nullptr;
            _attackArea  = nullptr;
            _bossNode    = nullptr;
            _playerSlots.clear();
            _abilityIcons.clear();
            _active = false;
        }
    }
    
    /**
     * Sets whether the scene is currently active.
     *
     * Toggling activation is the appropriate place to enable/disable UI
     * elements such as buttons. An inactive scene should not process input.
     *
     * @param value  true to activate, false to deactivate.
     */
    void GameScene::setActive(bool value) {
        if (isActive() != value) {
            Scene2::setActive(value);
            if (value) {
                // TODO: Activate buttons and interactive elements here
            }
            else {
                // TODO: Deactivate buttons and interactive elements here
            }
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
        for (auto& icon : _abilityIcons) {
            if (!icon || !icon->isVisible()) continue;
            Rect box = icon->getBoundingBox();
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
     * Resets all ability icons to their original positions and visibility.
     *
     * Called when the player taps the reset button. Cancels any active drag,
     * clears the glow effect, and restores every icon to its initial state.
     */
void GameScene::reset() {
    for (int i = 0; i < _abilityIcons.size(); i++) {
        if (!_abilityIcons[i]) continue;
        _abilityIcons[i]->setVisible(true);                  // Un-hide consumed icons
        _abilityIcons[i]->setPosition(_abilityOriginalPos[i]); // Snap back to starting position
    }
    
    _activeIcon = nullptr;                          // Cancel any in-progress drag
    _glowAction = InputController::Action::NONE;    // Clear the glow action
    _glowTimer  = 0;                                // Stop the glow timer
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
            if (!_inventory || !_player) {
                return;
            }
            
            std::unordered_set<ItemInstance::ItemId> liveIds;
            
            for (const ItemInstance& item : _player->getInventory()) {
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
    


