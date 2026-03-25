#ifndef __GAME_SCENE_H__
#define __GAME_SCENE_H__

#include <cugl/cugl.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
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
class GameScene : public cugl::scene2::Scene2{
public:
    /*Keeps track of the game state. This is how the app knows when to swap scenes*/
    enum Status {
        PLAYING,
        WON,
        LOST,
    };
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

    /** UI slot used to display left teammate's avatar. */
    std::shared_ptr<cugl::scene2::PolygonNode> _leftPlayerSlot;
    
    /** UI slot used to display right teammate's avatar. */
    std::shared_ptr<cugl::scene2::PolygonNode> _rightPlayerSlot;

    /** The player's inventory UI container node. */
    std::shared_ptr<cugl::scene2::SceneNode> _inventory;

    /** Maps ItemId to the on-screen widget node representing that item. */
    std::unordered_map<ItemInstance::ItemId, std::shared_ptr<cugl::scene2::SceneNode>> _itemWidgets;

    /** Inventory-only physics world used to attach Box2D bodies to item widgets. */
    std::shared_ptr<cugl::physics2::ObstacleWorld> _itemPhysicsWorld;

    /** Maps ItemId to the Box2D body representing that inventory widget. */
    std::unordered_map<ItemInstance::ItemId, std::shared_ptr<cugl::physics2::BoxObstacle>> _itemBodies;

    /** Input zones: each entry maps an Action to the screen Rect that triggers it. */
    std::vector<std::pair<InputController::Action, cugl::Rect>> _inputZones;
    
    /** Zones used for attack on screen. */
    std::vector<std::pair<InputController::Action, cugl::Rect>> _attackZones;
    
    /** Zones used for support on screen. */
    std::vector<std::pair<InputController::Action, cugl::Rect>> _supportZones;
    
    /** Zones used for inventory on screen. */
    std::vector<std::pair<InputController::Action, cugl::Rect>> _inventoryZones;
    
    /** IZones used for pass on screen. . */
    std::vector<std::pair<InputController::Action, cugl::Rect>> _passZones;

    /** The reset button node. */
    std::shared_ptr<cugl::scene2::SceneNode> _resetBtn;
    
    /** The boss health bar */
    std::shared_ptr<cugl::scene2::ProgressBar> _bossHealthBar;
    
    /** The boss health bar text showing amount of health left */
    std::shared_ptr<cugl::scene2::Label> _bossHealthBarText;
    
    /** The player's health bar*/
    std::shared_ptr<cugl::scene2::ProgressBar> _playerHealthBar;
    
    /** The player's health bar text showing amount of health left */
    std::shared_ptr<cugl::scene2::Label> _playerHealthBarText;
    
    /** UI slot used to display player's avatar in inventory. */
    std::shared_ptr<cugl::scene2::PolygonNode> _localPlayerSlot;
    
    /** Left teammate username label */
    std::shared_ptr<cugl::scene2::Label> _leftPlayerName;

    /** Right teammate username label */
    std::shared_ptr<cugl::scene2::Label> _rightPlayerName;
    
    /** Slots already demoted to Easy AI this session; prevents re-demoting each frame. */
    std::unordered_set<int> _slotsDemotedToAI;
    
    /** The sprite node representing the boss character frame in the scene based on the spritesheets. */
    std::shared_ptr<cugl::scene2::SpriteNode> _bossSprite;

#pragma mark - Drag State

    /** The scene node currently being dragged by the player, or nullptr. */
    std::shared_ptr<cugl::scene2::SceneNode> _draggedIcon;

    /** The inventory item currently being dragged, or 0 if none is active. */
    ItemInstance::ItemId _draggedItemId = 0;

    /** The dragged body's pre-drag position, used to restore invalid drops. */
    cugl::Vec2 _dragStartBodyPosition = cugl::Vec2::ZERO;

    /** The ItemDef of the item currently being dragged, or nullptr. */
    std::shared_ptr<const ItemDef> _draggedItemDef = nullptr;
    
    /** Offset from the icon's origin to the touch point, applied during drag. */
    cugl::Vec2 _dragOffset;

#pragma mark - Glow Effect State

    /** The drop zone action whose region should currently glow. */
    InputController::Action _glowAction = InputController::Action::NONE;

    /** Seconds remaining on the active glow effect; 0 means no glow. */
    float _glowTimer = 0;

    /** Maximum duration of a glow effect in seconds. */
    float _glowDuration = 0.3f;

#pragma mark - Teammate Damage Blink State

    /** Seconds remaining on the left teammate damage blink effect. */
    float _leftPlayerDamageBlinkTimer = 0.0f;

    /** Seconds remaining on the right teammate damage blink effect. */
    float _rightPlayerDamageBlinkTimer = 0.0f;

    /** Last observed health snapshot for the left teammate. */
    float _lastLeftPlayerHealth = -1.0f;

    /** Last observed health snapshot for the right teammate. */
    float _lastRightPlayerHealth = -1.0f;

    /** Total duration of the teammate damage blink effect. */
    float _damageBlinkDuration = 0.45f;

    /** Blink cadence used for teammate damage flashes. */
    float _damageBlinkInterval = 0.12f;

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

    Status _status;

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
    * Returns the current status of the game and whether or not the player wants to go back to a different scene
    */
    Status getStatus() { return _status; }

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
     * Initialises the dedicated Box2D world used for inventory item widgets.
     *
     * Bodies in this world are only used for debugging and future inventory
     * interactions, so the world has zero gravity and scene-space bounds.
     *
     * @return true if the physics world was created successfully.
     */
    bool initInventoryPhysics();

    /**
     * Initialises the ItemController and GameState.
     * ItemController must be initialised first because GameState::init()
     * needs the item database to finish setting up AI players.
     *
     * @return true if both systems initialised successfully.
     */
    bool initGameSystems();
    
    /**
     * Initializes the background and boss images for the current game scene.
     *
     * This function sets the visual assets for both the background and the boss
     * based on the active enemy in the game state. It retrieves the enemy ID and
     * uses it to construct texture keys for the corresponding assets.
     */
    void initBackgroundAndBossImage();

    /**
     * Initialises the scene graph and all game systems.
     *
     * Builds the scene graph from the asset manager, initialises the
     * ItemController, and delegates world-state construction (players,
     * enemy, AI) to GameState::init(). Does not activate the scene —
     * call setActive(true) when ready to receive input.
     *
     * @param assets  The loaded asset manager.
     * @param networkController The network controller shared across all scenes
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
     * Applies the dragged attack item to the enemy.
     *
     *@param itemId  The id of the item being handled.
     */
    bool handleAttack(ItemInstance::ItemId itemId);

    /**
     * Handles the local player dropping a support item on the left ally zone.
     * Applies the dragged support item to the left neighbour.
     *
     *@param itemId  The id of the item being handled.
     */
    bool handleSupportLeft(ItemInstance::ItemId itemId);

    /**
     * Handles the local player dropping a support item on the right ally zone.
     * Applies the dragged support item to the right neighbour.
     *
     *@param itemId  The id of the item being handled.
     */
    bool handleSupportRight(ItemInstance::ItemId itemId);

    /**
     * Passes the dragged item to the left neighbour.
     *
     *@param itemId  The id of the item being handled.
     */
    bool handlePassLeft(ItemInstance::ItemId itemId);

    /**
     * Passes the dragged item to the right neighbour.
     *
     *@param itemId  The id of the item being handled.
     */
    bool handlePassRight(ItemInstance::ItemId itemId);

    /**
     * Dispatches the resolved drop-zone action to the appropriate handler
     * and resets the input action afterwards.
     * No-op if the local player is not alive.
     *
     * @param itemId  The id of the item being handled.
     */
    bool handlePlayerActions(InputController::Action action, ItemInstance::ItemId itemId);

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
     * Updates the player and teammate UI icons to reflect their current health.
     *
     * @param dt Delta time in seconds.
     */
    void updatePlayerAndTeammateIcons(float dt);

    /** Resynchronises teammate damage blink state with the current local player. */
    void resetTeammateDamageBlinkState();

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
    * Processes all the passMessages inside of the vector, putting the correct items in the player's inventory
    * If we are the host, it will also give the correct items to the AI
    * Intended usage: get the pass message vector from the network controller and pass into this function
    */
    void processNetworkedPasses(std::vector<PassMessage> passes);
    
    /**
     * Looks up the ItemDef for the item currently being dragged.
     * Returns nullptr if the item is not found or has no definition.
     *
     * @param itemId  The ID of the held item.
     * @return        A shared pointer to the item's definition, or nullptr.
     */
    std::shared_ptr<const ItemDef> getHeldItemDef(ItemInstance::ItemId itemId);
    
    /**
     * Spawns items for the local player every frame, and for all AI-controlled
     * players if this machine is the host. AI item spawning is host-only since
     * the host is the authoritative source for all AI state.
     *
     * @param dt  Delta time in seconds.
     */
    void handleItemSpawn(float dt);
    
    /**
     * Top-level disconnect handler. Called every frame from update().
     * Delegates to the three helpers below.
     */
    void handleDisconnectedPlayers();
    
    /**
     * HOST ONLY. Builds a slot -> networkID map for every real (non-AI)
     * player and passes it to the NetworkController to diff against the
     * still-connected peer list. Populates _disconnectedSlots with any
     * newly-dropped slots.
     */
    void detectDroppedPeers();

    /**
     * HOST ONLY. Replaces the player at the given slot with an EasyPlayerAI,
     * re-wires the neighbour ring, and restores the disconnected player's
     * health and inventory onto the new AI.
     *
     * @param slot  The 0-based slot index of the disconnected player.
     */
    void demoteSlotToAI(int slot);

    /**
     * HOST + CLIENTS. Updates the left and right teammate name labels to
     * reflect the current AI/human state of each neighbour.
     */
    void refreshTeammateNameLabels();
    
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

    /** Creates and registers the Box2D body for an item widget.
     *
     * @param itemId  The ItemInstance for which the item body is created.
     * @param widget  The widget to attach the physics body to.
     */
    std::shared_ptr<cugl::physics2::BoxObstacle> createItemBody(
        ItemInstance::ItemId itemId,
        const std::shared_ptr<cugl::scene2::SceneNode>& widget
    );

    /** Updates all inventory widgets so they exactly match their body positions. */
    void syncItemWidgetsToBodies();

    /** Removes the widget and its Box2D body for the given item.
     *
     * @param itemId  The itemId representing the ItemInstance to be removed.
     */
    void removeItemWidget(ItemInstance::ItemId itemId);

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
     * Rebuilds the active input zones based on the type of item currently being dragged.
     * Attack items show the boss drop zone; support items show the ally drop zones.
     * Pass zones are always included while dragging. Clears all zones if nothing is held.
     */
    void updateInputZones();

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

    /** Draws a cyan outline around Box2D debug wireframes for inventory item bodies.
     *
     * @param batch  The active sprite batch.
     */
    void renderItemBodyDebug(cugl::graphics::SpriteBatch* batch);

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
    
    /**
     * Syncs the local game state with the current network player order.
     *
     * Converts all networked player slots to real human players, assigns
     * the local player index, and updates the left and right neighbor
     * name labels in the UI. Should be called once when the game scene
     * becomes active after the lobby has finalized the player order.
     *
     * Does nothing if the network is not connected.
     */
    void updateNetworkOrder();
    
#pragma mark - Getters

    /**
     * Returns a reference to the authoritative game state owned by this scene.
     *
     * Intended for use by other scenes (e.g. LobbyScene) that need read access
     * to the player array and circle order without owning or copying the state.
     * The reference is valid for the lifetime of this GameScene instance.
     *
     * @return  A reference to the GameState owned by this scene.
     */
    GameState& getGameState() { return _gameState; }
};
#endif /* __GAME_SCENE_H__ */
