#ifndef __GAME_SCENE_H__
#define __GAME_SCENE_H__

#include <cugl/cugl.h>
#include <vector>
#include <unordered_map>
#include "GameState.h"
#include "../InputController.h"
#include "../items/ItemController.h"
#include "../Enemy.h"
#include "../EnemyLoader.h"
#include "../EnemyController.h"
#include "../NetworkController.h"
#include "../NetworkMessage.h"

/**
 * Controller for the core game scene.
 *
 * GameScene is a pure controller: it owns the scene graph, handles input,
 * drives rendering, and delegates all world-state reads and writes to its
 * GameState member. GameScene holds no game-world data itself — if a field
 * describes what is happening in the world (player health, enemy position,
 * inventories) it lives in GameState, not here.
 *
 * This separation makes it straightforward to broadcast a read-only
 * GameState snapshot over the network without touching any rendering code.
 */
class GameScene : public cugl::scene2::Scene2 {
protected:
#pragma mark - Scene Graph Nodes

    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;

    /** Network controller. Responsible for sending networking messages and process messages sent
     * over the network. */
    std::shared_ptr<NetworkController> _network;

    /** The root scene node for this scene graph. */
    std::shared_ptr<cugl::scene2::SceneNode> _scene;

    /** The node representing the main game area (top half of screen). */
    std::shared_ptr<cugl::scene2::SceneNode> _gameArea;

    /** The node representing the attack interaction area (the red zone). */
    std::shared_ptr<cugl::scene2::SceneNode> _attackArea;

    /** The node representing the boss character in the scene. */
    std::shared_ptr<cugl::scene2::SceneNode> _bossNode;

    /** UI slots used to display each teammate's avatar. */
    std::vector<std::shared_ptr<cugl::scene2::SceneNode>> _playerSlots;

    /** The player's inventory UI container node. */
    std::shared_ptr<cugl::scene2::SceneNode> _inventory;

    /** Maps ItemId to the on-screen widget node representing that item. */
    std::unordered_map<ItemInstance::ItemId, std::shared_ptr<cugl::scene2::SceneNode>> _itemWidgets;

    /** Input zones: each entry maps an Action to the screen Rect that triggers it. */
    std::vector<std::pair<InputController::Action, cugl::Rect>> _inputZones;

    /** The reset button node. */
    std::shared_ptr<cugl::scene2::SceneNode> _resetBtn;
    
    /** The boss health bar */
    std::shared_ptr<cugl::scene2::ProgressBar> _bossHealthBar;
    
    /** The player's health bar*/
    std::shared_ptr<cugl::scene2::ProgressBar> _playerHealthBar;
    
    /** Left teammate username label */
    std::shared_ptr<cugl::scene2::Label> _leftPlayerName;

    /** Right teammate username label */
    std::shared_ptr<cugl::scene2::Label> _rightPlayerName;

#pragma mark - Drag State

    /** The scene node currently being dragged by the player, or nullptr. */
    std::shared_ptr<cugl::scene2::SceneNode> _activeIcon;

    /** Offset from the icon's origin to the touch point, applied during drag. */
    cugl::Vec2 _dragOffset;

#pragma mark - Glow Effect State

    /** The drop zone action whose region should currently glow. */
    InputController::Action _glowAction = InputController::Action::NONE;

    /** Seconds remaining on the active glow effect; 0 means no glow. */
    float _glowTimer = 0;

    /** Maximum duration of a glow effect in seconds. */
    float _glowDuration = 0.3f;

#pragma mark - Debug State
    
    /** Determines whether the debug mode is on. This inlcudes reset button, zone lines, etc.*/
    bool _debugMode = false;

    /** Latest pointer position in scene coordinates, for debug rendering. */
    cugl::Vec2 _debugPointerScene = cugl::Vec2::ZERO;

    /** True while a touch is active and the debug pointer should be drawn. */
    bool _hasDebugPointer = false;

#pragma mark - Controllers

    /** Manages item spawning, timers, and the item definition database. */
    ItemController _itemController;

    /** Drives enemy behaviour and resolves enemy attacks against players. */
    EnemyController _enemyController;

#pragma mark - World State

    /**
     * The authoritative game-world model.
     *
     * All player data (health, inventory, neighbour links), enemy data, and
     * player-ID mappings live here. GameScene reads and mutates world state
     * exclusively through this object so that the state can later be
     * serialised and broadcast over the network without touching any
     * rendering or input code.
     */
    GameState _gameState;

    /** Keeps track of whether or not we are the host */
    bool _host;

public:
#pragma mark - Constructors

    /**
     * Constructs an uninitialised GameScene.
     * No heap allocation occurs until init() is called.
     */
    GameScene() : cugl::scene2::Scene2() {}

    /**
     * Destructs the GameScene, disposing all owned resources.
     */
    ~GameScene() { dispose(); }

#pragma mark - Lifecycle

    /**
     * Disposes of all (non-static) resources allocated to this mode.
     */
    void dispose() override;

    /**
     * Builds the five rectangular input zones from the current scene dimensions.
     * Must be called after Scene2::initWithHint() so getSize() is valid.
     */
    void initInputZones();

    /**
     * Loads the scene graph from assets, resizes it to the current scene
     * dimensions, and wires up all named child node references
     * (_scene, _gameArea, _inventory, _resetBtn, _attackArea, _bossNode, _playerSlots).
     * Must be called after _assets is assigned and Scene2::initWithHint() succeeds.
     *
     * @return true if the root scene node was found in the asset manager.
     */
    bool initSceneGraph();

    /**
     * Initialises the ItemController and GameState.
     * ItemController must be initialised first because GameState::init()
     * needs the item database to finish setting up AI players.
     *
     * @return true if both systems initialised successfully.
     */
    bool initGameSystems();

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
    bool init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController);

    /**
     * Activates or deactivates the scene and its UI.
     * Calls reset() and enters the idle enemy state on activation.
     *
     * @param value  true to activate, false to deactivate.
     */
    virtual void setActive(bool value) override;

    /**
     * Resets the scene to its start-of-round state.
     * Clears all item widgets, cancels any active drag, resets the glow
     * effect, and clears every player's inventory via GameState::reset().
     */
    void reset() override;

#pragma mark - Player Assignment

    /**
     * Assigns the local player slot for this machine.
     * Delegates to GameState::setLocalPlayer(). Should be called by the
     * host after network lobby slot assignment.
     *
     * @param assignedIndex  Zero-based index into the player array.
     */
    void setLocalPlayer(int assignedIndex);

#pragma mark - Action Handlers

    /**
     * Handles the local player dropping an attack item on the boss zone.
     * Finds the first attack item in the local player's inventory and
     * applies it to the enemy.
     */
    void handleAttack();

    /**
     * Handles the local player dropping a support item on the left ally zone.
     * Finds the first support item and applies it to the left neighbour.
     */
    void handleSupportLeft();

    /**
     * Handles the local player dropping a support item on the right ally zone.
     * Finds the first support item and applies it to the right neighbour.
     */
    void handleSupportRight();

    /**
     * Passes the first item in the local player's inventory to the left neighbour.
     */
    void handlePassLeft();

    /**
     * Passes the first item in the local player's inventory to the right neighbour.
     */
    void handlePassRight();

    /**
     * Dispatches the resolved drop-zone action to the appropriate handler
     * and resets the input action afterwards.
     * No-op if the local player is not alive.
     *
     * @param input  The active input controller.
     */
    void handlePlayerActions(InputController::Action action);

#pragma mark - Update Helpers

    /**
     * Ticks the enemy controller and all AI-controlled players forward by
     * one frame. Uses dynamic_cast for correct virtual dispatch across all
     * PlayerAI subclasses. Should only run on the host in a networked session.
     *
     * @param dt  Delta time in seconds.
     */
    void updateEnemyAndAI(float dt);
    
    /**
     * Updates the progress bar with the current ratios of player and enemy health.
     *
     * @param dt Delta time in seconds
     */
    void updatePlayerAndEnemyHealthUI(float dt);

    /**
     * Checks whether the reset button was tapped and calls reset() if so.
     * Only fires when no icon is being dragged and a touch just ended.
     *
     * @param input  The active input controller.
     */
    void handleResetButton(InputController& input);

    /**
     * Handles the full pipeline of a player's drag-and-drop input for one frame.
     *
     * When the player releases a dragged item, this function determines which
     * drop zone the item landed in and dispatches the appropriate game action
     * (attack, support, or pass). Also triggers a glow effect on the activated
     * zone for visual feedback and clears the active dragged icon.
     *
     * Does nothing if no item is being dragged or if the touch has not ended.
     * Does nothing if the local player is dead.
     *
     * @param input     The input controller for this frame.
     */
    void handlePlayerInput(InputController& input);

    /**
     * Decrements the glow timer each frame. Clears the active glow action
     * once the timer expires.
     *
     * @param dt  Delta time in seconds.
     */
    void tickGlowTimer(float dt);

    /**
     * Updates the debug pointer position in scene coordinates.
     * Sets _hasDebugPointer to false when no touch is active.
     *
     * @param input  The active input controller.
     */
    void updateDebugPointer(InputController& input);

    /**
     * Hit-tests item widgets against the initial touch position.
     * Begins a drag and records the offset if a widget is hit.
     *
     * @param input  The active input controller.
     */
    void handleDragInitiation(InputController& input);

    /**
     * Moves the active dragged icon to follow the current touch position.
     * Applies _dragOffset so the icon does not snap to the finger centre.
     *
     * @param input  The active input controller.
     */
    void handleDragTracking(InputController& input);

    /**
     * Logs the resolved drop-zone action for debugging.
     * Replace CULog calls with real game logic as features are implemented.
     *
     * @param action  The action resolved from the drop-zone hit-test.
     */
    void debugLogAction(InputController::Action action);

    /**
    * Processes all the passMessages inside of the vector, putting the correct items in the player's inventory
    * If we are the host, it will also give the correct items to the AI
    * Intended usage: get the pass message vector from the network controller and pass into this function
    */
    void processNetworkedPasses(std::vector<PassMessage> passes);

#pragma mark - Inventory UI

    /**
     * Creates a scene-node widget for the given item and adds it to the
     * inventory container. Texture is chosen by item type (attack or support).
     *
     * @param item  The item instance to represent.
     * @return      The new widget node, or nullptr if assets were missing.
     */
    std::shared_ptr<cugl::scene2::SceneNode> createItemWidget(const ItemInstance& item);

    /** Return a random valid inventory position for a newly spawned item widget */
    cugl::Vec2 getRandomInventoryPosition(const cugl::Size& widgetSize) const;

    /** Sync player inventory and item widgets displayed on screen */
    void syncInventoryWidgets();

#pragma mark - Update & Render

    /**
     * Processes one frame of game logic.
     *
     * Delegates to focused helper methods in this order:
     *  1. Reset button tap detection.
     *  2. Drag-and-drop release zone classification.
     *  3. Glow timer decay.
     *  4. Debug pointer tracking.
     *  5. Drag initiation hit-testing.
     *  6. Active icon position tracking.
     *  7. Item controller update and inventory widget sync.
     *  8. Local player action dispatch.
     *  9. Enemy and AI tick.
     *
     * @param dt     Delta time in seconds.
     * @param input  The input controller owned by SceneLoader.
     */
    void update(float dt, InputController& input);

    /**
     * Draws a green debug outline around the reset button's bounding box.
     *
     * @param batch  The active sprite batch.
     */
    void renderResetButton(cugl::graphics::SpriteBatch* batch);

    /**
     * Draws a faint green outline around every input zone. Draws a fading
     * filled overlay on the zone matching the most recent successful drop.
     *
     * @param batch  The active sprite batch.
     */
    void renderDropZones(cugl::graphics::SpriteBatch* batch);

    /**
     * Draws a magenta outline around each visible item widget's bounding box.
     *
     * @param batch  The active sprite batch.
     */
    void renderItemWidgetDebug(cugl::graphics::SpriteBatch* batch);

    /**
     * Draws a small red square at the current touch position.
     * Only draws when _hasDebugPointer is true.
     *
     * @param batch  The active sprite batch.
     */
    void renderPointerDebug(cugl::graphics::SpriteBatch* batch);

    /**
     * Custom render pass drawn after the standard scene graph render.
     *
     * Draws translucent debug/gameplay overlays:
     *  1. Reset button debug outline.
     *  2. Drop zone outlines and glow effect.
     *  3. Item widget bounding box outlines.
     *  4. Current pointer position indicator.
     *
     * All coordinates are in scene space (bottom-left origin) because the
     * sprite batch is already configured with the scene camera.
     */
    void render() override;
    
#pragma mark - Debug Mode
    /**
     * Sets whether debug mode is on.
     *
     * @param enabled  The state _debugMode should be set to.
     */
    void setDebugMode(bool enabled);
    
    /**
     * Retrieves the current state of `_debugMode.
     */
    bool isDebugMode() const {return _debugMode; }

#pragma mark - Networking
    /* Checks if any updates about the state of the game were sent over the network. 
    * If we are a client, we update the state of the game to match the hosts' version and process any passes sent to us. 
    * If we are the host, we process any attack, heal, and pass messages. 
    * After doing so, we send out a new authoritative version of the game state as the host*/
    void handleNetworkUpdates();
};

#endif /* __GAME_SCENE_H__ */
