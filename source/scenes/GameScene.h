#ifndef __GAME_SCENE_H__
#define __GAME_SCENE_H__

#include <cugl/cugl.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "GameState.h"
#include "../InputController.h"
#include "../AudioController.h"
#include "../items/ItemController.h"
#include "../Enemy.h"
#include "../EnemyLoader.h"
#include "../EnemyController.h"
#include "../NetworkController.h"
#include "../NetworkMessage.h"

/**
 * Represents the state of a single snapback animation for a dropped item.
 * Multiple items can be snapping back simultaneously.
 */
struct SnapbackAnimation {
    /** Screen position where the snapback animation starts. */
    cugl::Vec2 startPos;
    
    /** Target inventory position for snapback animation. */
    cugl::Vec2 targetPos;
    
    /** Normalized progress of snapback animation (0.0 to 1.0). */
    float progress = 0.0f;
};

/**
 * Represents a short-lived visual animation for an item that was just consumed.
 * The real gameplay item is removed immediately; this ghost only handles UX.
 */
struct ConsumedItemAnimation {
    /** Transient visual node shown while the consume animation plays. */
    std::shared_ptr<cugl::scene2::SceneNode> node;

    /** Elapsed animation time in seconds. */
    float elapsed = 0.0f;

    /** Total animation time in seconds. */
    float duration = 0.0f;

    /** Starting scale at animation begin. */
    float startScale = 1.0f;

    /** Ending scale at animation completion. */
    float endScale = 0.0f;
};
/*
 * Represents a single item use animation currently playing on screen.
 * 
 * When a player uses an attack item with animation, an overlay sprite plays and damage
 * is applied at a specific keyframe (damageResolutionFrame). This struct tracks all state
 * needed to manage the animation lifecycle: rendering, frame advancement, and damage timing.
 * 
 * The animation system allows visual feedback to play before damage is applied, enabling
 * effects and sounds to be coordinated with specific animation frames. Multiple animations
 * can be active simultaneously (stored in GameScene::_activeItemUseAnimations).
 */
struct ItemUseAnimation {
    /** The sprite sheet managing frame layout and texture coordinates. */
    std::shared_ptr<cugl::graphics::SpriteSheet> spriteSheet;
    
    /** The sprite node displaying the sprite sheet frames in the scene graph. */
    std::shared_ptr<cugl::scene2::SpriteNode> node;
    
    /** The original animation position before any camera/viewport transforms. */
    cugl::Vec2 basePosition;
    
    /** The size in pixels of a single frame in the sprite sheet. */
    cugl::Size frameSize;
    
    /** Number of columns in the sprite sheet grid layout. */
    int frameCols = 1;
    
    /** Total number of frames in the animation sequence. */
    int frameCount = 0;
    
    /** Total duration of the entire animation in seconds. */
    float animationDuration = 0.0f;
    
    /** Frame index at which damage should be applied to the target and network-broadcasted. */
    int damageResolutionFrame = 0;
    
    /** Pre-calculated damage amount to apply when reaching the resolution frame. */
    float damageAmount = 0.0f;
    
    /** Reserved for future use: originally stored itemId for deferred calculation (now pre-calculated). */
    ItemInstance::ItemId itemId = 0;
    
    /** Elapsed time in seconds since animation started. Used to calculate current frame. */
    float elapsedTime = 0.0f;
    
    /** Flag indicating whether damage has been applied at the resolution frame (prevents re-application). */
    bool damageResolved = false;
    
    /** The frame index currently being displayed (cached to avoid redundant setFrame() calls). */
    int currentFrameIndex = -1;
};

/**
 * Represents a single animation entry from the enemy animations registry (enemyAnimations.json).
 * Contains metadata needed to render and advance animation frames.
 */
struct AnimationEntry {
    std::string id;                  /**< Animation identifier (e.g., "cyclops_idle_animation") */
    std::string texture;             /**< Texture asset key (e.g., "gameScene/cyclops/cyclops_idle_animation") */
    int frameCount;                  /**< Number of frames per animation row */
    float frameDuration;             /**< Duration in seconds per frame */
    int frameRows;                   /**< Number of rows in the sprite sheet */
    
    // Attack phase configuration
    int buildupFrameCount = 0;       /**< Number of frames in buildup phase that loop. 0 = no buildup */
    int damageFrame = -1;            /**< Absolute frame index when damage is dealt (-1 = no auto-damage) */
    
    // Position and scale customization
    float positionX = 196.5f;        /**< Screen X position for this animation */
    float positionY = 120.0f;        /**< Screen Y position for this animation */
    float scale = 0.92f;             /**< Scale multiplier for this animation */
    float offsetX = 0.0f;            /**< X offset from base position */
    float offsetY = 0.0f;            /**< Y offset from base position */
};

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

    /** Audio controller. Manages all audio playback (music and sound effects). */
    AudioController* _audio;

    /** The root scene node for this scene graph. */
    std::shared_ptr<cugl::scene2::SceneNode> _scene;

    /** The node representing the main game area (top half of screen). */
    std::shared_ptr<cugl::scene2::SceneNode> _gameArea;

    /** The node representing the attack interaction area (the red zone). */
    std::shared_ptr<cugl::scene2::PolygonNode> _attackArea;
    
    /** The node representing the left support interaction area (the blue zone on the left). */
    std::shared_ptr<cugl::scene2::SceneNode> _supportLeftArea;
    
    /** The node representing the right support interaction area (the blue zone on the right). */
    std::shared_ptr<cugl::scene2::SceneNode> _supportRightArea;

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

    /** Current visual scale for each inventory item widget (for smooth pickup/release animation). */
    std::unordered_map<ItemInstance::ItemId, float> _itemWidgetScales;

    /** Target visual scale for each inventory item widget. */
    std::unordered_map<ItemInstance::ItemId, float> _itemWidgetScaleTargets;

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
    std::shared_ptr<cugl::scene2::SceneNode> _bossSprite;
    
    /** The scene node representing the animated special effects to be populated in the scene based on the spritesheets. */
    std::shared_ptr<cugl::scene2::SceneNode> _specialEffectsLayer;

#pragma mark - Drag State

    /** The scene node currently being dragged by the player, or nullptr. */
    std::shared_ptr<cugl::scene2::SceneNode> _draggedIcon;

    /** The inventory item currently being dragged, or 0 if none is active. */
    ItemInstance::ItemId _draggedItemId = 0;

    /** Whether the select sound has been played for the current touch. */
    bool _selectSoundPlayedThisTouch = false;

    /** The dragged body's pre-drag position, used to restore invalid drops. */
    cugl::Vec2 _dragStartBodyPosition = cugl::Vec2::ZERO;

    /** The ItemDef of the item currently being dragged, or nullptr. */
    std::shared_ptr<const ItemDef> _draggedItemDef = nullptr;
    
    /** Offset from the icon's origin to the touch point, applied during drag. */
    cugl::Vec2 _dragOffset;

#pragma mark - Sliding Items State

    /** Set of ItemIds currently sliding/animating. */
    std::unordered_set<ItemInstance::ItemId> _slidingItems;

    /** Set of ItemIds that are in transit as passes (not natural spawns). 
     *  These bypass inventory limits and animate from sides instead of center-bottom.
     *  Items are added here when passed (human or networked) and removed when picked up. */
    std::unordered_set<ItemInstance::ItemId> _passedItemIds;

    /** Item body position from previous frame, used to calculate release velocity. */
    cugl::Vec2 _dragPreviousFrameItemBodyPos = cugl::Vec2::ZERO;

    /** ItemId of the item currently being dragged/released. */
    ItemInstance::ItemId _dragReleasedItemId = 0;

    /** Original inventory position of the item being dropped (target for snapback animation). */
    cugl::Vec2 _dragReleasedFromInventoryPos = cugl::Vec2::ZERO;

    /** Map of ItemId to active snapback animations. Multiple items can be snapping back simultaneously. */
    std::unordered_map<ItemInstance::ItemId, SnapbackAnimation> _snapbackAnimations;

    /** Active short-lived consumed-item ghost animations. */
    std::vector<ConsumedItemAnimation> _consumedItemAnimations;
    /** Vector of currently active item use animations. Multiple animations can play concurrently. */
    std::vector<ItemUseAnimation> _activeItemUseAnimations;

#pragma mark - Glow Effect State

    /** The drop zone action whose region should currently glow. */
    InputController::Action _glowAction = InputController::Action::NONE;

    /** Seconds remaining on the active glow effect; 0 means no glow. */
    float _glowTimer = 0;

    /** Maximum duration of a glow effect in seconds. */
    float _glowDuration = 0.3f;

#pragma mark - Teammate Blink State

    /** Seconds remaining on the left teammate damage blink effect. */
    float _leftPlayerDamageBlinkTimer = 0.0f;

    /** Seconds remaining on the right teammate damage blink effect. */
    float _rightPlayerDamageBlinkTimer = 0.0f;

    /** Seconds remaining on the left teammate heal blink effect. */
    float _leftPlayerHealBlinkTimer = 0.0f;

    /** Seconds remaining on the right teammate heal blink effect. */
    float _rightPlayerHealBlinkTimer = 0.0f;

    /** Last observed health snapshot for the left teammate. */
    float _lastLeftPlayerHealth = -1.0f;

    /** Last observed health snapshot for the right teammate. */
    float _lastRightPlayerHealth = -1.0f;

    /** Total duration of the teammate blink effect. */
    float _blinkDuration = 0.45f;

    /** Blink cadence used for teammate flashes. */
    float _blinkInterval = 0.12f;

#pragma mark - Debug State
    
    /** Determines whether the debug mode is on. This inlcudes reset button, zone lines, etc.*/
    bool _debugMode = false;

    /** Latest pointer position in scene coordinates, for debug rendering. */
    cugl::Vec2 _debugPointerScene = cugl::Vec2::ZERO;

    /** True while a touch is active and the debug pointer should be drawn. */
    bool _hasDebugPointer = false;

#pragma mark - Enemy Animation State

    /** Animation registry loaded from enemyAnimations.json. Maps animation ID to metadata. */
    std::unordered_map<std::string, AnimationEntry> _animationRegistry;

    /** Pre-created sprite nodes for all animations, mapped by animation ID. Built once during init(). */
    std::unordered_map<std::string, std::shared_ptr<cugl::scene2::SpriteNode>> _enemyAnimationSpriteNodes;

    /** Currently visible animation sprite node (pointer to one of the sprites in _enemyAnimationSpriteNodes). */
    std::shared_ptr<cugl::scene2::SpriteNode> _currentVisibleAnimationSprite;

    /** Cached animation entry for currently playing animation. Used for frame calculations. */
    AnimationEntry _currentAnimationEntry;

    /** Animation ID of the currently visible animation (for detecting animation changes). */
    std::string _currentAnimationId;

    /** Current direction (0-3) the enemy faces, computed locally per player from local player index + target index. */
    int _enemyAnimationCurrentDirection = 0;

    /** Cached frame index to avoid redundant setFrame() calls (optimization). */
    int _enemyAnimationCachedFrameIndex = -1;

    /** Caches whether current idle state has animation metadata (optimization). */
    bool _enemyAnimationHasMetadata = false;
    
    /** Flag tracking if damage has been dealt during the current enemy state. Resets when state changes. */
    bool _enemyAttackDamageDealtThisState = false;

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
     * Initialises the inventory physics world and Box2D bodies for item widgets.
     *
     * @return true if the physics world was created successfully.
     */
    bool initPhysicsWorld();

    /**
     * Initialises the ItemController and GameState.
     * ItemController must be initialised first because GameState::init()
     * needs the item database to finish setting up AI players.
     *
     * @return true if both systems initialised successfully.
     */
    bool initGameSystems();

    /** Loads data-driven tuning values used by teammate blink UI. */
    void initBlinkConfig();
    
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
    bool init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController, AudioController* audio);

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
     * Disposes and re-initialises the GameState for a fresh session.
     * Call this when aborting the lobby to clear all player house selections.
     */
    void resetGameState();

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

    /**
     * Handles an animated attack: calculates damage, removes item, and queues animation.
     * Called by handleAttack() when the item has an animation config.
     * Damage is applied when animation reaches the resolution frame.
     *
     * @param itemId   The item instance ID being used
     * @param item     The ItemInstance being used
     * @param def      The item definition containing animation config
     * @param local    The local player performing the attack
     * @param enemy    The enemy being attacked
     * @return true if animation was successfully queued, false if damage calculation failed
     */
    bool handleAnimatedAttack(ItemInstance::ItemId itemId, const ItemInstance& item,
                              const std::shared_ptr<const ItemDef>& def,
                              Player* local, Enemy* enemy);

    /**
     * Handles a non-animated attack: applies damage immediately using useItemById.
     * Called by handleAttack() when the item has no animation config.
     * Damage is applied immediately and broadcast to network.
     *
     * @param itemId   The item instance ID being used
     * @param item     The ItemInstance being used
     * @param def      The item definition (no animation config)
     * @param local    The local player performing the attack
     * @param enemy    The enemy being attacked
     * @return true if damage was successfully applied, false if calculation failed
     */
    bool handleImmediateAttack(ItemInstance::ItemId itemId, const ItemInstance& item,
                               const std::shared_ptr<const ItemDef>& def,
                               Player* local, Enemy* enemy);

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
     * Updates enemy idle animation and directional facing based on target.
     * Each frame: recalculates direction from local player index + enemy target index,
     * advances sprite frame based on elapsed time, and updates the sprite node display.
     *
     * If animation metadata is undefined, sprite node is destroyed/hidden and the
     * fallback static sprite is displayed. Direction is computed locally per player
     * from the enemy's target index, so each player sees the correct enemy direction
     * from their perspective.
     *
     * @param dt                Elapsed time in seconds for this frame
     * @param localPlayerIndex  The local player's index (0-3) for calculating relative direction
     */
    void updateEnemyAnimation(float dt, int localPlayerIndex);

    /**
     * Pre-creates all enemy animation sprite nodes with their textures and layouts.
     * 
     * Called during init() to load all animations upfront. This eliminates stuttering
     * when switching between animations since all sprites are pre-allocated and we only
     * swap visibility instead of creating/destroying sprites at runtime.
     *
     * @return true if all animations were successfully initialized, false on error
     */
    bool initializeAllEnemyAnimations();
    
    /**
     * Switches the visible animation sprite by hiding the current one and showing the new one.
     * 
     * Fast O(1) operation that just changes visibility and resets animation timing.
     * All sprites are pre-created, so this avoids runtime texture loading.
     *
     * @param animationId  The animation ID to make visible
     */
    void switchVisibleAnimation(const std::string& animationId);

    /**
     * Updates the current animation frame for direction and elapsed time.
     * 
     * Recalculates the direction the enemy should face (0-3) based on relative
     * positions of local player and target, then advances the animation frame
     * based on accumulated elapsed time and frame duration from animation metadata.
     * Only calls setFrame() if the frame index has changed (cached optimization).
     *
     * @param dt                The elapsed time in seconds since last frame
     * @param localPlayerIndex  The local player's index (0-3) for direction calculation
     */
    void updateEnemyAnimationFrame(float dt, int localPlayerIndex);

    /**
     * Hides the enemy animation sprite and shows the static fallback sprite.
     * 
     * Sets visibility on both the animation sprite node and the container,
     * then reveals the static sprite as a fallback. Called when animation
     * metadata is unavailable or the enemy is dead.
     */
    void hideEnemyAnimationAndShowStatic();

    /**
     * Loads the animation registry from enemyAnimations.json and populates _animationRegistry.
     * This builds a lookup map from animation IDs to their metadata (frameCount, frameDuration, frameRows).
     */
    void loadAnimationRegistry();
    
    /**
     * Plays health and damage indicator sounds based on health changes.
     * Called after game state updates to detect and play appropriate audio feedback
     * for player damage, healing, and enemy damage. 
     *
     * Only plays player hurt/heal sounds for non-AI local player. Also plays enemy hurt
     * sounds. Uses the player's house to determine which hurt sound variant to play.
     *
     * @param playerHealthBefore  The player's health before state updates
     * @param enemyHealthBefore   The enemy's health before state updates
     */
    void playHealthAndDamageSounds(float playerHealthBefore, float enemyHealthBefore);
    
    /**
     * Checks if the current enemy attack animation has finished playing (both buildup and attack phases).
     *
     * @return true if attack animation is complete, false otherwise
     */
    bool isEnemyAttackAnimationComplete() const;
    
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

    /**
     * Updates one teammate icon's blink state and tint based on health deltas.
     *
     * @param slot              The teammate icon node to tint.
     * @param player            The teammate whose health drives the icon state.
     * @param lastHealth        The previous observed health snapshot for this teammate.
     * @param damageBlinkTimer  Countdown used for red damage blinking.
     * @param healBlinkTimer    Countdown used for the green heal flash.
     * @param dt                Delta time in seconds.
     */
    void updateTeammateBlink(
        const std::shared_ptr<cugl::scene2::PolygonNode>& slot,
        Player* player,
        float& lastHealth,
        float& damageBlinkTimer,
        float& healBlinkTimer,
        float dt
    );

    /** Resynchronises teammate blink state with the current local player. */
    void resetTeammateBlinkState();

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
     * Initiates sliding for a released item by calculating velocity based on drag motion
     * and clamping to maximum speed. Used when an item is dropped on an invalid zone
     * or outside any zone.
     *
     * @param itemId  The ID of the item to start sliding
     */
    void slideReleasedItem(ItemInstance::ItemId itemId);

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
    * Processes all the passMessages inside of the vector, putting the correct items in the player's inventory.
    * Marks received items as passes so they bypass inventory limits and spawn from sides.
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
     * Initializes a sliding item with the given velocity and origin type.
     * Marks the item as sliding and configures its state based on origin.
     *
     * @param itemId        The ID of the item to start sliding
     * @param velocity      Initial velocity vector (units/sec)
     * @param origin        The SlideOriginType indicating where the slide came from
     */
    void startItemSliding(ItemInstance::ItemId itemId, const cugl::Vec2& velocity, ItemInstance::SlideOriginType origin);
    
    /**
     * Updates all sliding items each frame, applying friction and checking boundaries.
     * Handles settlement and snapback animations for dropped items.
     *
     * @param dt  Delta time in seconds.
     */
    void updateSlidingItems(float dt);
    
    /**
     * Updates friction deceleration for a sliding item and its body position.
     * Called each frame to slow down items based on ITEM_SLIDE_FRICTION_DECELERATION.
     *
     * @param item       The item instance to update.
     * @param itemBody   The Box2D body representing the item.
     * @param dt         Delta time in seconds.
     * @return           true if the item is still sliding (speed > threshold), false if settled.
     */
    bool updateItemFriction(ItemInstance* item, std::shared_ptr<cugl::physics2::BoxObstacle> itemBody, float dt);
    
    /**
     * Handles settlement logic for dropped items.
     * Checks if the item is within inventory bounds and initiates snapback or settles accordingly.
     *
     * @param item       The item instance that has settled.
     * @param itemBody   The Box2D body representing the item.
     * @param itemId     The ID of the item.
     * @return           true if the item should be removed from sliding set.
     */
    bool handleSettledItemDrop(ItemInstance* item, std::shared_ptr<cugl::physics2::BoxObstacle> itemBody, ItemInstance::ItemId itemId);
    
    /**
     * Dispatches settlement handling based on item origin type.
     * Returns whether the item should be removed from the sliding set.
     *
     * @param item     The settled item to handle.
     * @param itemBody The Box2D body representing the item.
     * @param itemId   The ID of the item.
     * @return         true if the item should be removed from sliding set, false if still animating (snapback).
     */
    bool handleSettledItem(ItemInstance* item, std::shared_ptr<cugl::physics2::BoxObstacle> itemBody, ItemInstance::ItemId itemId);
    
    /**
     * Handles settlement logic for spawned/passed items.
     * Enables zone interactions once the item has settled from its spawn.
     *
     * @param item   The item that has settled.
     * @param itemId The ID of the item.
     * @return       true (spawned/passed items are always removed from sliding set after settlement).
     */
    bool handleSpawnedItemSettled(ItemInstance* item, ItemInstance::ItemId itemId);
    
    /**
     * Checks if an item is in a matching interaction zone.
     * Verifies that the item position is within a zone and that its type matches
     * the zone's expected item type (Attack items for boss zones, Support for ally zones).
     *
     * @param itemPos  The item's current world position
     * @param itemDef  The item definition containing type information
     * @return         true if the item is in a valid matching zone, false otherwise
     */
    bool isItemInMatchingZone(const cugl::Vec2& itemPos, const std::shared_ptr<ItemDef>& itemDef);
    
    /**
     * Initiates a snapback animation for an item returned to inventory.
     * Creates a snapback animation entry with a random target inventory position
     * and adds it to the animations queue.
     *
     * @param itemId   The ID of the item to snapback
     * @param fromPos  The item's current world position (animation start point)
     */
    void initiateSnapbackAnimation(ItemInstance::ItemId itemId, const cugl::Vec2& fromPos);
    
    /**
     * Checks if a settled item should be removed due to being off-screen.
     * Only applies to spawned and passed items; dropped items are exempted.
     *
     * @param item     The item instance to check.
     * @param itemBody The Box2D body representing the item.
     * @return         true if the item is off-screen and should be removed.
     */
    bool shouldRemoveOffscreenItem(ItemInstance* item, std::shared_ptr<cugl::physics2::BoxObstacle> itemBody);
    
    /**
     * Updates snapback animations for dropped items returning to inventory.
     * Smoothly interpolates item positions back to their original inventory locations.
     *
     * @param dt  Delta time in seconds.
     */
    void updateSnapbackAnimations(float dt);
    
    /**
     * Processes zone interactions for zone-interactive sliding items.
     * Verifies strict item-type matching (attack↔attack, support↔support)
     * and triggers the appropriate action if a match is found.
     * Called once per frame after sliding velocity updates.
     */
    void processZoneInteractionsForSlidingItems();
    
    /**
     * Clamps a passed item's position to the inventory zone bounds.
     * Prevents passed items from sliding outside the valid inventory area.
     *
     * @param itemBody      The Box2D body to clamp
     */
    void clampItemToBounds(std::shared_ptr<cugl::physics2::BoxObstacle> itemBody);
    
    /**
     * Checks if an item's position is within visible screen bounds.
     *
     * @param position      The screen position to check
     * @return true if position is within visible area, false otherwise
     */
    bool isItemInVisibleArea(const cugl::Vec2& position);
    
    /**
     * Queues an item use animation for display in the special effects layer.
     * 
     * Creates a SpriteNode from the specified sprite sheet and adds it to the scene graph.
     * The animation will play in updateItemUseAnimations() each frame, advancing frames
     * based on elapsed time. When the current frame index reaches damageResolutionFrame,
     * the pre-calculated damage is applied and broadcast to the network.
     * 
     * The number of sheets and frame layout are specified in animConfig (from ItemDef).
     * Damage is pre-calculated by the caller (not calculated here), allowing hostile
     * consumers to perform custom calculations or effects between damage calc and application.
     * 
     * @param animConfig     Configuration from ItemDef specifying sprite sheet ID, rows/cols, 
     *                       total frames, animation duration, and damage resolution keyframe
     * @param damageAmount   Pre-calculated damage to apply at damageResolutionFrame
     * @param itemPos        Screen position to center animation at (defaults to viewport center)
     * @param itemId         Reserved for future use (currently unused; kept for extensibility)
     */
    void startItemUseAnimation(const ItemUseAnimationConfig& animConfig, float damageAmount, 
                               const cugl::Vec2& itemPos = cugl::Vec2::ZERO, 
                               ItemInstance::ItemId itemId = 0);
    
    /**
     * Updates all active item use animations for one frame.
     * 
     * For each active animation:
     *   1. Advances elapsed time by dt
     *   2. Calculates current frame index based on animation progress
     *   3. Updates sprite sheet frame if frame index changed
     *   4. At damageResolutionFrame: applies pre-calculated damage and broadcasts to network
     *   5. Removes animation from queue when animation duration elapsed
     * 
     * Damage broadcast happens here to ensure tight synchronization between all clients:
     * - Host: applies damage locally, broadcasts via next broadcastGameState()
     * - Clients: broadcast damage message to host immediately after resolution
     * 
     * This architecture ensures damage is consistently applied at the same animation frame
     * across all networked machines, preventing desync issues.
     *
     * @param dt  Delta time in seconds (typically from game loop)
     */
    void updateItemUseAnimations(float dt);
    
    /**
     * Clears all active item use animations, removing them from the scene graph.
     * Called when the game ends or resets.
     */
    void clearItemUseAnimations();
    
    /**
     * Returns whether there are any active item use animations currently playing.
     * Used to defer game-over checks until animations complete.
     *
     * @return true if there are active animations, false otherwise
     */
    bool hasActiveItemAnimations() const { return !_activeItemUseAnimations.empty(); }    
    /** Checks if an item is currently playing an animation.
     * Used to prevent respawning items that are mid-animation.
     *
     * @param itemId The ID of the item to check
     * @return true if the item has an active animation, false otherwise
     */
    bool isItemAnimating(ItemInstance::ItemId itemId) const;
    
    /** Checks if an item type matches an action zone type.
     *
     * @param action  The zone action type
     * @param itemType The type of item
     * @return true if the item can be used in this zone
     */
    bool isItemActionMatch(InputController::Action action, ItemDef::Type itemType) const;    
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
    
    /**
     * Calculates the effective damage value for an attack item.
     * 
     * Computes damage multipliers based on the attacking player's house affiliation
     * and the item's house affinity. Applies base value multiplied by house role
     * multiplier and affinity bonus (for rare/divine items matching player house).
     * 
     * This function mirrors the damage calculation logic in Player::useItemById()
     * but separates it for cases where damage needs to be deferred (animated attacks).
     * 
     * @param player     The attacking player (provides house for multiplier lookup)
     * @param itemDef    The item definition containing base value and affinity info
     * @param database   The item database for house multiplier lookup
     * @return           Calculated damage magnitude, or 0.01f if calculation yields <= 0
     */
    float calculateItemDamage(const Player* player, const std::shared_ptr<const ItemDef>& itemDef, const ItemDatabase& database);
    
    /**
     * Removes an item from a player's inventory by item instance ID.
     * 
     * Searches for the item in the player's inventory and erases it if found.
     * This is used to decouple item removal from damage calculation, allowing
     * animations and effects to be applied between consumption and damage.
     * 
     * @param player   The player whose inventory to modify
     * @param itemId   The unique ID of the item instance to remove
     * @return         true if item was found and successfully removed, false otherwise
     */
    bool removeItemFromInventory(Player* player, ItemInstance::ItemId itemId);
    
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
    
    /** Return a spawn position for a passed item based on which side it came from
     *
     * @param passDirection  0 for none, 1 for passed from left, 2 for passed from right
     * @return               The spawn position below the side the item came from
     */
    cugl::Vec2 getPassSpawnPosition(int passDirection) const;

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

    /** Smoothly animates each item widget's scale towards its current target. */
    void updateItemWidgetScales(float dt);

    /** Spawns a short-lived shrinking ghost visual for a consumed item. */
    void spawnConsumedItemAnimation(const std::shared_ptr<cugl::scene2::SceneNode>& sourceWidget,
                                    const std::shared_ptr<const ItemDef>& itemDef);

    /** Advances and cleans up active consumed-item ghost animations. */
    void updateConsumedItemAnimations(float dt);

    /** Removes and clears all consumed-item ghost animations. */
    void clearConsumedItemAnimations();
    /** Marks an item as used (pending animation resolution, should not be respawned).
     *  Removes the visual widget and physics body, but keeps item in inventory until damage applies.
     *
     * @param itemId  The itemId that was just used
     */
    void markItemAsUsed(ItemInstance::ItemId itemId);

    /**
     * Helper function to spawn an item widget from a given position with animation.
     *
     * @param item       The ItemInstance to spawn
     * @param spawnPos   The world position to spawn from
     * @param slideOrigin The origin type of the slide (SPAWN or PASS)
     */
    void _spawnItemFromPosition(const ItemInstance& item, cugl::Vec2 spawnPos, ItemInstance::SlideOriginType slideOrigin);

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
     * Updates the visibility of all drop zones based on the current interaction.
     *
     * This function evaluates which drop zones should be visible at the current moment
     * (e.g., during drag-and-drop interactions or based on item/type compatibility)
     * and toggles their visibility accordingly.
     */
    void updateDropZoneVisibility();

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
    void renderDropZonesDebug(cugl::graphics::SpriteBatch* batch);

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
