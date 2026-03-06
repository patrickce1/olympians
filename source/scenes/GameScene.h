#ifndef __GAME_SCENE_H__
#define __GAME_SCENE_H__
#include <cugl/cugl.h>
#include <vector>
#include <unordered_map>
#include "../Player.h"
#include "../InputController.h"
#include "../items/ItemController.h"
#include "../Enemy.h"
#include "../EnemyController.h"
#include "../CharacterLoader.h"
#include "../playerAI/PlayerAI.h"
#include "../playerAI/EasyPlayerAI.h"

/**
 * This class represents the core game scene
 * Add things to this class as necessary
 */
class GameScene : public cugl::scene2::Scene2 {
public:
    /*getters that represent game state should go here*/

protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;
    /** The root scene node for this scene graph. */
    std::shared_ptr<cugl::scene2::SceneNode> _scene;

    /** The node representing the main game area, top half of screen. */
    std::shared_ptr<cugl::scene2::SceneNode> _gameArea;

    /** The node representing the attack interaction area, the red zone. */
    std::shared_ptr<cugl::scene2::SceneNode> _attackArea;

    /** The node representing the boss character in the scene, the centerpiece. */
    std::shared_ptr<cugl::scene2::SceneNode> _bossNode;

    /** The collection of UI slots used to display teammate-related nodes. */
    std::vector<std::shared_ptr<cugl::scene2::SceneNode>> _playerSlots;

    /** The node representing the player's inventory UI container. */
    std::shared_ptr<cugl::scene2::SceneNode> _inventory;

    /** The collection of item widgets mapping itemId to its corresponding widget */
    std::unordered_map<ItemInstance::ItemId, std::shared_ptr<cugl::scene2::SceneNode>> _itemWidgets;
    
    /** The vector representing the various zones on screen.*/
    std::vector<std::pair<InputController::Action, cugl::Rect>> _inputZones;
    
    /**The button allowing for a reset on the scene. */
    std::shared_ptr<cugl::scene2::SceneNode> _resetBtn;
    
    /** The node representing the active (placeholder) item that the player is holding**/
    std::shared_ptr<cugl::scene2::SceneNode> _activeIcon;
    
    /**Distance that the active item has moved.**/
    cugl::Vec2 _dragOffset;
    
    /** The Action corresponding to the zone that should glow once the player performs that action**/
    InputController::Action _glowAction = InputController::Action::NONE;
    
    /**The original locations of the abilities. */
    std::vector<cugl::Vec2> _abilityOriginalPos;
    
    /**Internal clock to measure how long we have been glowing**/
    float _glowTimer = 0;
    
    /**How long that a region that glows should glow for at maximum.**/
    float _glowDuration = 0.3f;

    /** Debug: latest pointer position in scene coordinates */
    cugl::Vec2 _debugPointerScene = cugl::Vec2::ZERO;
    
    /** Debug: whether a pointer is currently active */
    bool _hasDebugPointer = false;
    
    /** The character loader to load in player characters for this GameScene instance */
    CharacterLoader _characterLoader;
    
    /** Pointer to the player belonging to the local machine. Set once in init(), never reallocated. */
    Player* _localPlayer = nullptr;
    
    /**
     * All players in this party — both human-controlled (Player) and
     * AI-controlled (PlayerAI) — stored polymorphically as shared_ptr<Player>.
     * Index 0 is the human player; indices 1+ are AI bots.
     * PlayerAI must inherit from Player for virtual dispatch to work.
     */
    std::vector<std::shared_ptr<Player>> _players;
    
    /** The ItemController for this GameScene instance*/
    ItemController _itemController;

    /** The enemy for this scene and its controller */
    std::shared_ptr<Enemy> _enemy;
    EnemyController _enemyController;


public:
#pragma mark -
#pragma mark Constructors
    /**
     * This constructor does not allocate any objects or start the game.
     * This allows us to use the object without a heap pointer.
     */
    GameScene() : cugl::scene2::Scene2() {}

    /**
     * Disposes of all (non-static) resources allocated to this mode.
     *
     * This method is different from dispose() in that it ALSO shuts off any
     * static resources, like the input controller.
     */
    ~GameScene() { dispose(); }

    /**
     * Disposes of all (non-static) resources allocated to this mode.
     */
    void dispose() override;

    /**
     * Initializes the controller contents.
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
    bool init(const std::shared_ptr<cugl::AssetManager>& assets);

    /**
     * Sets whether the scene is currently active
     *
     * This method should be used to toggle all the UI elements.  Buttons
     * should be activated when it is made active and deactivated when
     * it is not.
     *
     * @param value whether the scene is currently active
     */
    virtual void setActive(bool value) override;
    
    /**
     * Called after the network session and assigns all local machines to a network-given player slot
     */
    void setLocalPlayer(int assignedIndex);
    /**
     * Handles the human player dropping an item on the boss zone to attack.
     */
    void handleAttack();

    /**
     * Handles the human player dropping an item on the left ally zone to support.
     */
    void handleSupportLeft();

    /**
     * Handles the human player dropping an item on the right ally zone to support.
     */
    void handleSupportRight();

    /**
     * Handles the human player passing an item to the left neighbor.
     */
    void handlePassLeft();

    /**
     * Handles the human player passing an item to the right neighbor.
     */
    void handlePassRight();
    
    /**
     * Dispatches the local player's current input action to the appropriate
     * handler method and resets the action afterwards.
     * No-op if the local player is not alive.
     *
     * @param input  The active input controller from SceneLoader.
     */
    void handlePlayerActions(InputController& input);

    /**
     * Updates the enemy controller and ticks all AI-controlled players.
     * Uses dynamic_cast for virtual dispatch so any PlayerAI subclass is
     * handled correctly. Should only be called by the host.
     *
     * @param dt  Delta time in seconds since the last frame.
     */
    void updateEnemyAndAI(float dt);
    
    /**
     * Checks if the reset button was tapped and resets the scene if so.
     * Only fires when no icon is being dragged and a touch just ended.
     *
     * @param input  The active input controller from SceneLoader.
     */
    void handleResetButton(InputController& input);

    /**
     * Resolves a drag-and-drop release by classifying the release position
     * into a drop zone and triggering the appropriate action and glow effect.
     * Clears the active icon after resolution.
     *
     * @param input  The active input controller from SceneLoader.
     */
    void handleDropResolution(InputController& input);

    /**
     * Ticks down the glow timer each frame. Once expired, clears the active
     * glow action so the visual feedback stops rendering.
     *
     * @param dt  Delta time in seconds since the last frame.
     */
    void tickGlowTimer(float dt);

    /**
     * Tracks the current pointer position in scene coordinates for debug rendering.
     * Sets _hasDebugPointer to false when no touch is active.
     *
     * @param input  The active input controller from SceneLoader.
     */
    void updateDebugPointer(InputController& input);

    /**
     * Hit-tests inventory item widgets against the initial touch position.
     * If a touch begins on a widget, begins dragging it and stores the
     * offset so the icon doesn't snap its center to the finger.
     *
     * @param input  The active input controller from SceneLoader.
     */
    void handleDragInitiation(InputController& input);

    /**
     * Moves the active dragged icon to follow the finger each frame.
     * Applies the stored drag offset so movement feels natural.
     *
     * @param input  The active input controller from SceneLoader.
     */
    void handleDragTracking(InputController& input);
    
    /**
     * Logs the action that was resolved from a drag-and-drop gesture.
     *
     * Called after a successful drop zone classification to confirm which
     * action was triggered. Currently logs each case for debugging purposes —
     * replace CULog calls with real game logic as each action is implemented.
     *
     * @param action  The action to resolve, as classified by the drop zone hit-test.
     */
    void resolveAction(InputController::Action action);
    
    /**
     * Processes one frame of game logic.
     *
     * Accepts the SceneLoader's input controller directly so no copy or
     * secondary member is needed. Delegates to focused helper methods for
     * each responsibility:
     *  1. Reset button tap detection.
     *  2. Drag-and-drop release zone classification.
     *  3. Glow timer decay.
     *  4. Debug pointer tracking.
     *  5. Drag initiation hit-testing.
     *  6. Active icon position tracking.
     *  7. Item controller and inventory sync.
     *  8. Local player action dispatch.
     *  9. Enemy and AI update.
     *
     * @param dt     Delta time in seconds since the last frame.
     * @param input  The active input controller owned by SceneLoader.
     */
    void update(float dt, InputController& input);
    
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
    void render() override;
    
    /**
     * Resets the scene.
     */
    void reset() override;
    
    /** Create and return an item Widget with a given ItemInstance */
    std::shared_ptr<cugl::scene2::SceneNode> createItemWidget(const ItemInstance& item);
    
    /** Return the world position for an item widget's initial inventory position */
    cugl::Vec2 getInitialInventoryPosition() const;
    
    /** Sync player inventory and item widgets displayed on screen */
    void syncInventoryWidgets();

};

#endif /* __GAME_SCENE_H__ */
