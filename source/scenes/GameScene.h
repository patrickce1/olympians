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
#include "../TutorialController.h"
#include "../NetworkController.h"
#include "../NetworkMessage.h"
#include "../bosses/Gaia.h"
#include "../bosses/Cerberus.h"


/** Animation duration for floating popups to scale in, in seconds. */
static constexpr float FLOATING_POPUP_ANIM_IN  = 0.1f;
/** Animation duration for floating popups to fade out, in seconds. */
static constexpr float FLOATING_POPUP_ANIM_OUT = 0.2f;
/** Base font size the popup asset was baked at; used to derive display scale. */
static constexpr float FLOATING_POPUP_BASE_FONT_SIZE = 48.0f;

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

/**
 * Represents a corroded item animation that removes the item after completion.
 * Unlike consumed items, corroded items are removed from inventory AFTER animation finishes.
 */
struct CorrodedItemAnimation {
    /** Transient visual node shown while the corrode animation plays. */
    std::shared_ptr<cugl::scene2::SceneNode> node;

    /** Elapsed animation time in seconds. */
    float elapsed = 0.0f;

    /** Duration of the initial pop (scale-up) phase in seconds. */
    float popDuration = 0.0f;

    /** Peak scale reached at the end of the pop phase. */
    float popScale = 1.0f;

    /** Total animation duration in seconds (pop + decay). */
    float duration = 0.0f;

    /** Scale at animation start (typically 1.0). */
    float startScale = 1.0f;

    /** Target scale at animation end. */
    float endScale = 0.001f;

    /** Item ID to remove from inventory after animation completes. */
    ItemInstance::ItemId itemId;
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

    /** Base item value before multipliers, for popup display. */
    float baseValue = 0.0f;

    /** House-role and affinity multiplier, excluding any upgrade streak bonus, for popup display. */
    float houseAffinityMultiplier = 1.0f;

    /** Upgrade streak multiplier applied by effects such as the mallet, for popup display. */
    float upgradeMultiplier = 1.0f;
    
    /** Reserved for future use: originally stored itemId for deferred calculation (now pre-calculated). */
    ItemInstance::ItemId itemId = 0;

    /** Scene-space position where the popup should appear at damage resolution. */
    cugl::Vec2 popupPosition;

    /** Enemy effects to send alongside deferred damage when a non-host client resolves the hit. */
    std::vector<EnemyEffectMessage> enemyEffects;

    /** Definition ID of the consumed attack item, used for host-authoritative damage resolution. */
    std::string itemDefID;

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
    std::string id;             /** Animation identifier (e.g., "cyclops_idle_animation") */
    std::string texture;        /** Texture asset key */
    int frameCount;             /** Total number of frames in the animation */  
    float frameDuration;        /** Duration of each frame in seconds */
    int frameRows;              /** Number of rows in the sprite sheet */


    // Loop configuration
    int loopStartFrame = -1;    /** First frame of the loop range. Frames before this are a one-shot intro (-1 = no loop, play linearly) */
    int loopEndFrame = -1;      /** Last frame of the loop range. Frames after this are a one-shot outro (-1 = loop until end) */
    int damageFrame = -1;       /** Frame index when damage events fire (-1 = fire at loop end or last frame) */
    std::string sound;          /** Sound key to play when damageFrame is reached (empty = no sound) */

    // Position and scale customization
    float positionX = 196.5f;   /** Screen X position for this animation */
    float positionY = 120.0f;   /** Screen Y position for this animation */
    float scale = 0.92f;        /** Scale multiplier for this animation */
    float offsetX = 0.0f;       /** X offset from base position */
    float offsetY = 0.0f;       /** Y offset from base position */
};


/**
 * Tracks the Gaia vine overlay animation displayed over both ally icons.
 *
 * The animation has two phases:
 * - Forward (growth): follows enemy ATTACK_3 buildup progress (stateTime / buildUpTime)
 * - Reverse (retraction): runs locally using currentTime and dt, independent of enemy state
 *
 * Duration is initialized from the ATTACK_3 buildUpTime but is then used as a
 * standalone timeline for both forward and reverse playback.
 *
 * Sheet layout: 3 rows x 4 cols, 12 frames total.
 */
struct GaiaVineAnimation {
    std::shared_ptr<cugl::scene2::SpriteNode> leftNode;
    std::shared_ptr<cugl::scene2::SpriteNode> rightNode;
    int frameCount = 12;
    int currentFrame = -1;
    float duration = 0.5f;     // default value for now, should match the build up time
    float currentTime = 0.0f;  // keeps track of how long we've been in the state for
    bool reversing = false; // if the attack got cancelled or it finished and we have new neighbors
};

/**
 * Data for a single popup in a sequence.
 * General-purpose for any game event: damage, heals, buffs, status effects, health popups, etc.
 */
struct FloatingPopupData {
    std::string text;                               /** The text to display in the popup. */
    float fontSize = 32.0f;                         /** Font size for this popup. Scaled relative to FLOATING_POPUP_BASE_FONT_SIZE. */
    cugl::Color4 color = cugl::Color4::WHITE;       /** Foreground (fill) color of the text. */
    cugl::Color4 strokeColor = cugl::Color4::BLACK; /** Outline (stroke) color drawn behind the text. Defaults to black. */
    float delaySeconds = 0.0f;                      /** Seconds to wait after createFloatingPopup() is called before this popup spawns. */
    float displayDuration = 2.0f;                   /** Seconds the popup holds at full opacity before fading out. */
    cugl::Vec2 positionOffset = cugl::Vec2::ZERO;   /** Additional offset applied on top of the base screen position. */
    bool playSound = true;                          /** If true, plays the popup_ding sound when this popup spawns. */
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
        HOST_DISCONNECTED
    };
protected:
#pragma mark - Scene Graph Nodes

    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;

    /** Network controller. Responsible for sending networking messages and process messages sent
     * over the network. */
    std::shared_ptr<NetworkController> _network;
    
    /** The animation controller dedicated to the dialogue UI.
        This timeline manages the playback of slide transitions for the dialogue box.
        By using a dedicated timeline, dialogue animations can be updated or
        interrupted independently of other game world animations.
     */
    std::shared_ptr<cugl::ActionTimeline> _tutorialTimeline;

    /** Audio controller. Manages all audio playback (music and sound effects). */
    AudioController* _audio;

    /** The root scene node for this scene graph. */
    std::shared_ptr<cugl::scene2::SceneNode> _scene;

    /** The node representing the main game area (top half of screen). */
    std::shared_ptr<cugl::scene2::SceneNode> _gameArea;

    /** The node representing the attack interaction area (the red zone). */
    std::shared_ptr<cugl::scene2::PolygonNode> _attackArea;
    
    /** The node representing the left support interaction area (the green zone on the left). */
    std::shared_ptr<cugl::scene2::SceneNode> _supportLeftArea;
    
    /** The node representing the right support interaction area (the green zone on the right). */
    std::shared_ptr<cugl::scene2::SceneNode> _supportRightArea;
    
    /** The node representing the left pass interaction area (the blue zone on the inventory left). */
    std::shared_ptr<cugl::scene2::SceneNode> _passLeftArea;
    
    /** The node representing the right pass interaction area (the blue zone on the inventory right). */
    std::shared_ptr<cugl::scene2::SceneNode> _passRightArea;

    /** The node representing the boss character in the scene. */
    std::shared_ptr<cugl::scene2::SceneNode> _bossNode;

    /** Whether the boss is allowed to perform attacks; Specific to the tutorial. */
    bool _tutorialBossCanAttack = false;
    
    /** UI slot used to display left teammate's avatar. */
    std::shared_ptr<cugl::scene2::PolygonNode> _leftPlayerSlot;
    
    /** UI slot used to display right teammate's avatar. */
    std::shared_ptr<cugl::scene2::PolygonNode> _rightPlayerSlot;

    /** The player's inventory UI container node. */
    std::shared_ptr<cugl::scene2::SceneNode> _inventory;

    /** Maps ItemId to the on-screen widget node representing that item. */
    std::unordered_map<ItemInstance::ItemId, std::shared_ptr<cugl::scene2::SceneNode>> _itemWidgets;

    /** Set of ItemIds currently corroding (prevents scale updates during corrosion animation). */
    std::unordered_set<ItemInstance::ItemId> _corrodingItemIds;

    /** Player slot whose corrosive animations are still running (-1 if none).
     *  Stays set until all corroding animations complete so pass zones and
     *  item spawning remain blocked for the full visual duration. */
    int _corrosiveVisualTarget = -1;

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
    
    /** Zones used for pass on screen. . */
    std::vector<std::pair<InputController::Action, cugl::Rect>> _passZones;

    /** The reset button node. */
    std::shared_ptr<cugl::scene2::SceneNode> _resetBtn;
    
    /** The boss health bar */
    std::shared_ptr<cugl::scene2::ProgressBar> _bossHealthBar;
    
    /** The boss health bar icon */
    std::shared_ptr<cugl::scene2::PolygonNode> _bossHealthBarIcon;
     
    /** The boss health bar text showing amount of health left */
    std::shared_ptr<cugl::scene2::Label> _bossHealthBarText;
    
    /** The name of boss on top of boss health bar */
    std::shared_ptr<cugl::scene2::Label> _bossName;
    
    /** The player's health bar*/
    std::shared_ptr<cugl::scene2::ProgressBar> _playerHealthBar;
    
    /** The player's health bar text showing amount of health left */
    std::shared_ptr<cugl::scene2::Label> _playerHealthBarText;
    
    /** The player's health bar glow representing the current effect applied on the player */
    std::shared_ptr<cugl::scene2::PolygonNode> _playerHealthBarGlow;
    
    /** The player's shield bar under the actual health bar */
    std::shared_ptr<cugl::scene2::ProgressBar> _playerHealthBarShield;
    
    /** The player's name label showing username */
    std::shared_ptr<cugl::scene2::Label> _playerName;
    
    /** The player's name label showing house name  */
    std::shared_ptr<cugl::scene2::Label> _playerHouseName;
    
    /** UI slot used to display player's avatar in inventory. */
    std::shared_ptr<cugl::scene2::PolygonNode> _localPlayerSlot;
    
    /** Left teammate username label */
    std::shared_ptr<cugl::scene2::Label> _leftPlayerName;

    /** Right teammate username label */
    std::shared_ptr<cugl::scene2::Label> _rightPlayerName;
    
    /** The left player's label showing house name  */
    std::shared_ptr<cugl::scene2::Label> _leftPlayerHouse;
    
    /** The right player's label showing house name  */
    std::shared_ptr<cugl::scene2::Label> _rightPlayerHouse;
    
    /** Left teammate's health bar*/
    std::shared_ptr<cugl::scene2::ProgressBar> _leftPHealthBar;
    
    /** Right teammate's health bar*/
    std::shared_ptr<cugl::scene2::ProgressBar> _rightPHealthBar;
    
    /** The left player's shield bar under the actual health bar */
    std::shared_ptr<cugl::scene2::ProgressBar> _leftPHealthShield;
    
    /** The right player's shield bar under the actual health bar */
    std::shared_ptr<cugl::scene2::ProgressBar> _rightPHealthShield;
    
    /** Slots already demoted to Easy AI this session; prevents re-demoting each frame. */
    std::unordered_set<int> _slotsDemotedToAI;
    
    /** The sprite node representing the boss character frame in the scene based on the spritesheets. */
    std::shared_ptr<cugl::scene2::SceneNode> _bossSprite;
    
    /** The scene node representing the animated special effects to be populated in the scene based on the spritesheets. */
    std::shared_ptr<cugl::scene2::SceneNode> _specialEffectsLayer;
    
    /** The Current zone to highlight*/
    std::string _tutorialHighlightZone = "none";
    
    /** The Current zone that can be dropped on. Should match the above*/
    InputController::Action _allowedTutorialZone = InputController::Action::NONE;

    /** Whether support zones may be disabled*/
    bool _tutorialDisableSupportZones = false;
    /** The respective tooltip from the item being held down. */
    std::shared_ptr<cugl::scene2::PolygonNode> _tooltipNode;
    
    /** The timer for holding an item to acrivate tooltip */
    float _holdTimer = 0.0f;
    
    /** The amount of seconds before the tooltip appears */
    float _holdThreshold = 0.6f;
    
    /** The world-coordinate pixels before the tooltip is dismissed */
    float _tooltipMoveLimit = 20.0f;
    
    /** The world position when drag begins */
    Vec2 _holdAnchorPos = Vec2::ZERO;

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
    /** Active corrode animations for items being destroyed by corrosion. */
    std::vector<CorrodedItemAnimation> _corrodedItemAnimations;
    /** Vector of currently active item use animations. Multiple animations can play concurrently. */
    std::vector<ItemUseAnimation> _activeItemUseAnimations;

#pragma mark - Floating Popup State

    /**
     * Tracks one live floating-text popup animation.
     * Advances through three phases: scale-in, display, then fade-out.
     */
    struct FloatingPopupAnimation {
        /** Container scene node holding the outline copies and colored label. */
        std::shared_ptr<cugl::scene2::SceneNode> node;
        /** Time elapsed in the current animation, in seconds. */
        float elapsed = 0.0f;
        /** Duration of the scale-in phase, in seconds. */
        float animationInDuration = 0.1f;
        /** Duration the popup holds at full scale before fading, in seconds. */
        float displayDuration = 2.0f;
        /** Duration of the fade-out phase, in seconds. */
        float animationOutDuration = 0.2f;
        /** Target scale derived from the popup's font size relative to the base font size. */
        float displayScale = 1.0f;
        enum AnimationPhase { ANIM_IN, ANIM_DISPLAY, ANIM_OUT };
        AnimationPhase phase = ANIM_IN;
    };

    /**
     * A popup that has been queued but not yet spawned.
     * Spawned once its delay timer elapses in updatePopupAnimations().
     */
    struct PendingFloatingPopup {
        /** Visual and timing data for this popup. */
        FloatingPopupData data;
        /** Final screen-space position (base position + positionOffset already applied). */
        cugl::Vec2 position;
        /** Delay in seconds before this popup spawns. */
        float spawnTime;
        /** Time elapsed since the popup was queued, in seconds. */
        float elapsed = 0.0f;
    };

    /** Delayed popup for stun damage that resolves when the stun effect triggers. */
    struct PendingStunDamagePopup {
        /** Raw stun damage before item and enemy multipliers are applied. */
        float amount = 0.0f;
        /** House-role and affinity multiplier captured when the stun item was used. */
        float houseAffinityMultiplier = 1.0f;
        /** Upgrade streak multiplier captured when the stun item was used. */
        float upgradeMultiplier = 1.0f;
        /** Remaining delay before the stun damage popup appears. */
        float delay = 0.0f;
        /** Player slot credited with the stun damage. */
        int playerIndex = 0;
        /** Screen-space position where the popup should appear. */
        cugl::Vec2 position = cugl::Vec2::ZERO;
    };

    /** Vector of currently active floating popup animations. */
    std::vector<FloatingPopupAnimation> _activeFloatingPopups;
    
    /** Vector of pending floating popups that have been queued but not yet spawned. */
    std::vector<PendingFloatingPopup> _pendingFloatingPopups;

    /** Vector of stun damage popups waiting for their stun delay to elapse. */
    std::vector<PendingStunDamagePopup> _pendingStunDamagePopups;

#pragma mark - Tutorial Dialogue
    /** The root node of the dialogue UI, used for animations and visibility. Specific to tutorial */
    std::shared_ptr<cugl::scene2::SceneNode> _tutorialDialogueBox;
    
    /** The label component inside the dialogue box that displays the actual text. Specific to tutorial */
    std::shared_ptr<cugl::scene2::Label> _tutorialDialogueLabel;
    
    /** The target on-screen position where the dialogue box rests when active. Specific to tutorial */
    cugl::Vec2 _tutorialDialogueBoxPos;
    
    /** Buffer to hold the next string to display while the box is performing its "slide out" transition. Specific to tutorial*/
    std::string _tutorialPendingDialogueText = "";
    
    /** Timer to track the transition delay between sliding out old dialogue and sliding in the new message. Specific to tutorial */
    float _tutorialDialogueOutTimer = 0.0f;
    
    /** Flag indicating the dialogue box is currently offscreen and ready to perform the "slide in" animation. Specific to tutorial*/
    bool _tutorialDialogueWaitingToSlideIn = false;

#pragma mark - Ressurection Tracking
    /** Tracks a client-predicted resurrection until the authoritative host snapshot catches up. */
    struct PendingResurrectionSync {
        /** Party slots that were dead when the resurrection item was used locally. */
        std::vector<int> playerSlots;
        /** Health each revived slot should be restored to. */
        float reviveHealth = 0.0f;
        /** Total regen to arm on each revived slot. */
        float regenAmount = 0.0f;
        /** Regen duration to arm on each revived slot. */
        float regenDuration = 0.0f;
        /** Whether there is an active pending resurrection prediction. */
        bool active = false;
    };

    /** Client-side predicted resurrection state waiting for host confirmation. */
    PendingResurrectionSync _pendingResurrectionSync;

    /** Tracks one client-predicted timed party effect until the authoritative host snapshot catches up. */
    struct PendingPartyEffectSync {
        /** The effect being predicted. */
        ItemDef::EffectType effectType = ItemDef::EffectType::Educate;
        /** Party slots that should receive the effect. */
        std::vector<int> playerSlots;
        /** Effect duration to apply until host state arrives. */
        float duration = 0.0f;
        /** Primary effect magnitude to apply until host state arrives. */
        float magnitude = 0.0f;
        /** Whether there is an active pending party-effect prediction. */
        bool active = false;
    };

    /** Client-side predicted party-effect states waiting for host confirmation. */
    std::vector<PendingPartyEffectSync> _pendingPartyEffectSyncs;

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

    struct CerberusHeadOffset {
        float offsetX    = 0.0f;
        float offsetY    = 0.0f;
        float phaseOffset = 0.0f;
    };

    struct CerberusAnimConfig {
        std::string bodyAnimId;
        CerberusHeadOffset headOffsets[4];  // 0=front, 1=right, 2=back, 3=left
    };

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

    CerberusAnimConfig _cerberusAnimConfig;
    std::shared_ptr<cugl::scene2::SpriteNode> _cerberusBodySprite;
    /** Second body sprite inserted above all heads; shown only when facing away (direction=2). */
    std::shared_ptr<cugl::scene2::SpriteNode> _cerberusBodySpriteTop;
    /** Per-animation-key sprite sets for all cerberus head animations (one SpriteNode per head). */
    std::unordered_map<std::string, std::array<std::shared_ptr<cugl::scene2::SpriteNode>, 4>> _cerberusHeadSpritesByAnim;
    /** headAnimationKey of the IDLE state — fallback for non-participating heads. */
    std::string _cerberusIdleHeadAnimKey;
    /** Last enemy state seen; used to detect transitions and reset head timers. */
    EnemyLoader::State _cerberusLastState = EnemyLoader::State::IDLE;
    /** Guards per-head damage sound so it fires once per attack, not every frame. */
    bool _cerberusSoundFired[4] = {false, false, false, false};
    float _cerberusHeadAnimTime[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float _cerberusBodyAnimTime = 0.0f;
    /** Per-head committed animation key — persists until the animation completes, even across state transitions. */
    std::string _cerberusHeadActiveAnimKey[4];
    /** BuildUpTime saved when each head's attack animation was committed; needed for correct two-phase frame math after state transitions. */
    float _cerberusHeadAnimBuildUpTime[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    /** Last direction for which head z-order was applied; -1 forces a reorder on first frame. */
    int _cerberusLastDirection = -1;
    
    /** Active Gaia vine overlay animation, if any. Empty when no animation is playing. */
    std::optional<GaiaVineAnimation> _gaiaVineAnim;

#pragma mark - Controllers

    /** Manages item spawning, timers, and the item definition database. */
    ItemController _itemController;

    /** Drives enemy behaviour and resolves enemy attacks against players. */
    EnemyController _enemyController;

   /** Defines the tutorial actions*/
   TutorialController _tutorialController;

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
    
#pragma mark - Tutorial
    /** Whether we are currently doing the tutorial with the respective boss, Circe. **/
    bool _isTutorial;

 #pragma mark - Gaia Variables
    /* RNG for host - authoritative slot shuffling during gameplay. **/
    std::mt19937 _rng;

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
     * Returns the current status of the game and whether or not the player wants to go back to a different scene.
     *
     * @return The current Status value (PLAYING, WON, LOST, or HOST_DISCONNECTED).
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
     * @param assets             The loaded asset manager.
     * @param networkController  The network controller shared across all scenes.
     * @param audio              The audio controller for playing music and sound effects.
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
     * @param itemId  The id of the item being handled.
     * @return true if the attack was successfully applied, false otherwise.
     */
    bool handleAttack(ItemInstance::ItemId itemId);

    /**
     * Handles the local player dropping a support item on the left ally zone.
     * Applies the dragged support item to the left neighbour.
     *
     * @param itemId  The id of the item being handled.
     * @return true if the support was successfully applied, false otherwise.
     */
    bool handleSupportLeft(ItemInstance::ItemId itemId);

    /**
     * Handles the local player dropping a support item on the right ally zone.
     * Applies the dragged support item to the right neighbour.
     *
     * @param itemId  The id of the item being handled.
     * @return true if the support was successfully applied, false otherwise.
     */
    bool handleSupportRight(ItemInstance::ItemId itemId);

    /**
     * Passes the dragged item to the left neighbour.
     *
     * @param itemId  The id of the item being handled.
     * @return true if the pass was successfully initiated, false otherwise.
     */
    bool handlePassLeft(ItemInstance::ItemId itemId);

    /**
     * Passes the dragged item to the right neighbour.
     *
     * @param itemId  The id of the item being handled.
     * @return true if the pass was successfully initiated, false otherwise.
     */
    bool handlePassRight(ItemInstance::ItemId itemId);

    /**
     * Dispatches the resolved drop-zone action to the appropriate handler
     * and resets the input action afterwards.
     * No-op if the local player is not alive.
     *
     * @param action  The drop-zone action resolved from the input controller.
     * @param itemId  The id of the item being handled.
     * @return true if the action was handled successfully, false otherwise.
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
     * Updates the enemy health bar’s color based on its current status effects.
     *
     * This method is called every frame and adjusts the bar’s color from its
     * default (red) to reflect conditions such as stun, charm (“loved”), or
     * other active effects. The `dt` parameter allows for smooth color
     * transitions if needed.
     *
     * @param dt The time elapsed since the last frame (in seconds).
     */
    void updateEnemyHealthBarEffect(float dt);
    
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
     * Orchestrates all body and head sprite updates for Cerberus each frame.
     *
     * @param dt               Elapsed time in seconds since the last frame.
     * @param localPlayerIndex Slot index of the local player, used to determine facing direction.
     */
    void updateCerberusAnimation(float dt, int localPlayerIndex);

    /**
     * Reorders Cerberus head sprites in _bossSprite so the head facing the local
     * player renders on top. Called whenever the facing direction changes.
     *
     * @param direction  New facing direction (0=front, 1=right, 2=back, 3=left).
     */
    void reorderCerberusHeads(int direction);

    /**
     * Advances cerberus body and head animation timers. Pauses all timers while stunned.
     * During IDLE, scales elapsed time by the frantic speed multiplier so animations
     * visually speed up as Cerberus's health drops.
     *
     * @param dt        Elapsed time since the last frame in seconds.
     * @param enemy     The enemy whose stun and state are checked.
     * @param cerberus  The Cerberus instance queried for the frantic speed multiplier.
     */
    void advanceCerberusAnimationTimers(float dt, const std::shared_ptr<Enemy>& enemy, const std::shared_ptr<Cerberus>& cerberus);

    /**
     * Commits a new head attack animation to all participating heads when a non-idle
     * state is first entered. For attack_3, redirects to a side head if the front head
     * is knocked. Has no effect on idle transitions (those resolve lazily per-head).
     *
     * @param currentState  The state that was just entered.
     * @param stateDef      Definition of the new state (may be null).
     * @param direction     Current facing direction (0-3).
     * @param cerberus      The Cerberus instance queried for head-knock state.
     */
    void handleCerberusStateTransition(EnemyLoader::State currentState, const EnemyLoader::StateDef* stateDef, int direction, const std::shared_ptr<Cerberus>& cerberus);

    /**
     * Sets the correct frame and visibility for the Cerberus body sprites
     * based on the current facing direction.
     *
     * @param direction  Current facing direction (0=front, 1=right, 2=back, 3=left).
     */
    void updateCerberusBodySprite(int direction);

    /**
     * Updates frame, position, scale, visibility, and damage sound for a single
     * Cerberus head sprite.
     *
     * @param headIndex               Sprite index (0-3) of the head to update.
     * @param isVisible               False for the hidden back-position head.
     * @param direction               Current facing direction (0-3).
     * @param globalXShift            Lateral shift applied to all heads for directional perspective.
     * @param enemy                   The enemy for target-index and frame-counter updates.
     * @param cerberus                The Cerberus instance queried for head-knock and love state.
     * @param outFrameCounterUpdated  Set to true once the first attacking head drives the frame counter.
     */
    void updateSingleCerberusHead(int headIndex, bool isVisible, int direction, float globalXShift, const std::shared_ptr<Enemy>& enemy, const std::shared_ptr<Cerberus>& cerberus, bool& outFrameCounterUpdated);

    /**
     * Creates sprite nodes only for the selected enemy's animations.
     * Called from setActive(true) once the enemy is known.
     *
     * @param enemyId  The enemy identifier (e.g. "cyclops", "cerberus").
     * @return true if all animations were successfully initialized, false on error
     */
    bool initializeEnemyAnimations(const std::string& enemyId);

    /**
     * Creates and Z-orders all Cerberus body and head sprite nodes within _bossSprite.
     *
     * Extracts the body sprite placed by the registry loop, then (for Cerberus only)
     * allocates four head instances per animation set and interleaves them with the body
     * in draw order: back → body → right → left → front → body-top.
     *
     * @param enemyId  The enemy identifier; head setup only runs when this equals "cerberus".
     */
    void initializeCerberusAnimationSprites(const std::string& enemyId);

    /**
     * Destroys all pre-created enemy animation sprite nodes and resets related state.
     * Called from setActive(false) and before loading a new enemy in setActive(true).
     */
    void destroyEnemyAnimations();

    /**
     * Configures Cerberus-specific animation state after sprites have been created by
     * initializeEnemyAnimations(). Performs three tasks in order:
     *   1. Positions and scales the body sprites using the registry entry for the body animation.
     *   2. Reads per-head offsets and phase offsets from the enemy's customData JSON and applies
     *      them to every head sprite in _cerberusHeadSpritesByAnim.
     *   3. Derives the idle head animation key from the enemy's IDLE state definition and
     *      initialises _cerberusHeadActiveAnimKey / _cerberusHeadAnimBuildUpTime for all heads.
     *
     * After this call the body and head sprites are correctly placed but still invisible;
     * the caller is responsible for resetting timers and hiding sprites for a clean start.
     *
     * @param enemy  The Cerberus enemy instance to read customData and state definitions from.
     */
    void configureCerberusAnimationState(const std::shared_ptr<Enemy>& enemy);

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
     * Forces the enemy into the first attack state, targeting a specific player slot.
     * If a valid target slot is provided, the enemy will face that player before
     * attacking.
     *
     * @param targetSlot The 0-based slot index of the player the enemy should face. If out of bounds, no change in direction.
     */
    void triggerBossAttack(int targetSlot);
    
    /**
     * Forces the enemy into a defense state,  targeting a specific player slot.
     * If a valid target slot is provided, the enemy will face that player before
     * entering the defense move.
     *
     * @param targetSlot The 0-based slot index of the player the enemy should face. If out of bounds, no change in direction.
     */
    void triggerBossDefense(int targetSlot);
    
    /**
     * Calculates which animation frame to display based on state time and animation phase.
     * Handles three phases: optional intro (plays once), loop (cycles for buildUpTime), optional outro (plays once).
     * Animations with loopStartFrame < 0 play all frames linearly once.
     *
     * @param stateTime   Elapsed time in the current state (seconds)
     * @param buildUpTime How long the loop phase runs before transitioning to outro (-1 = loop forever)
     * @return The frame index within the animation row (0-indexed)
     */
    int calculateAnimationFrame(float stateTime, float buildUpTime = -1.0f) const;

    /**
     * Ensures the frame index is within valid bounds.
     * Clamps negative frames to 0 and frames beyond frameCount to frameCount-1.
     *
     * @param frameInRow The frame index to validate
     * @return The clamped frame index
     */
    int validateFrameIndex(int frameInRow) const;

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
     * @param playerHurtEnabled   If false, suppresses player hurt/heal sounds (e.g. during tutorial sequences)
     */
    void playHealthAndDamageSounds(float playerHealthBefore, float enemyHealthBefore, bool playerHurtEnabled = true);
    
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
    void updateAllPlayersAndEnemyHealthUI(float dt);
    
    /**
     * Updates the local player's progress bar with the current effects that have been applied
     * onto them.
     *
     * @param dt Delta time in seconds
     */
    void updatePlayerHealthBarEffect(float dt);
    
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
     * Handles tooltip visibility during drag: after holding long enough,
     * shows the tooltip (once) and keeps it aligned with the dragged item.
     *
     * @param dt  Delta time in seconds.
     */
    void handleTooltipVisibility(float dt);

    /**
     * Processes all the passMessages inside of the vector, putting the correct items in the player's inventory.
     * Marks received items as passes so they bypass inventory limits and spawn from sides.
     * If we are the host, it will also give the correct items to the AI.
     * Intended usage: get the pass message vector from the network controller and pass into this function.
     *
     * @param passes  The vector of PassMessage objects received from the network controller.
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

    /** Custom method called inside of handleItemSpawn that is used specifically for the Gaia boss
      * If gaia is supposed to spawn a rock in a player's inventory, the host sends the appropriate message to the players
      * Clients handle the logic for unwrapping the networked Gaia spawn messages inside of this method as well
      */
    void handleGaiaSpawn();

    /**
     * Checks if Cerberus's corrosive debuff should drain an item from the affected player.
     * If the drain timer has elapsed, removes a random item from the target player's inventory.
     * Host handles this authoritative logic; clients receive updates via game state broadcasts.
     */
    void handleCorrosiveDrain();

    /** HOST ONLY. Custom method used by Gaia. This creates a new ordering for the players.
      * This new ordering is sent to the GameState to be applied to the local machine.
      * This also broadcasts the new ordering over the network for clients to apply respectively as well
      */
    void handleGaiaScramble();

    /** Checks if we are in a state where 
      * the house and names of the current player's neighbors should be concealed
      *
      * @return     true if we should conceal neighbor house and name
      */
    bool gaiaShouldConcealIdentity();

    /**
     * Spawns items for the local player every frame, and for all AI-controlled
     * players if this machine is the host. AI item spawning is host-only since
     * the host is the authoritative source for all AI state. Should be off if 
     * playing Tutorial.
     *
     * @param dt  Delta time in seconds.
     */
    void handleItemSpawn(float dt);
    
    /**
     * Enable or disable boss activity. Tutorial Specific.
     * @param active What the boss should be set to in terms of activity.
     */
    void setTutorialBossActive(bool active);
    
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
     * Represents the animation to slide the dialogue in from the side of the screen in the tutorial only.
     */
    void slideDialogueIn();
    
    /**
     * Represents the animation to slide the dialogue out to the side of the screen in the tutorial only.
     */
    void slideDialogueOut();
    
    /**
     * Shows the dialogue box with the specified message.
     *
     * @param message  The text string to display inside the dialogue box.
     */
    void showDialogue(const std::string& message);
    
    /**
     * Retracts the dialogue box.
     */
    void hideDialogue();
        
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
     * Spawns a Gaia vine SpriteNode over both ally icon widgets.
     * Creates two SpriteNodes from the 4x3 sprite sheet, positions each over
     * the left/right icon's playerIcon node, and adds them as children so they
     * render in the same coordinate space as the icon. Called once on ATTACK_3
     * state entry
     */
    void startGaiaVineAnimation();

    /**
     * Advances the Gaia vine overlay animation.
     *
     * When boss is in ATTACK_3, we use the enemy current state time to dermine the progress of the animation
     *
     * If the boss is not in ATTACK_3 but the animation is still active, it means it is reversing.
     * Reversal ticks down its timer using dt and _gaiaVineAnim -> currentTime
     *
     * Animation done when the reversal part of the animation is done (because a reversal is guaranteed)
     *
     * @param dt  Delta time in seconds (used for reversal)
     */
    void updateGaiaVineAnimation(float dt);

    /**
     * Handles Gaia vine animation triggers based on enemy state changes.
     *
     * This function starts the vine animation when Gaia enters ATTACK_3,
     * and initiates the reverse (retraction) phase when Gaia leaves ATTACK_3
     *
     * IMPORTANT:
     * - This is purely visual and does not affect gameplay logic.
     * - Forward animation follows enemy buildup progress (stateTime / buildUpTime).
     * - Reverse animation is handled locally using currentTime and dt.
     * - Must be called once per frame before updateGaiaVineAnimation().
     *
     * Behavior summary:
     * - Enter ATTACK_3 -> start vine growth animation.
     * - Exit ATTACK_3 or Aphrodite love effect -> trigger reverse animation.
     * - Reverse completes -> animation cleans itself up in update.
     */
    void detectGaiaAnimationTriggers();
    
    /**
     * Returns whether there are any active item use animations currently playing.
     * Used to defer game-over checks until animations complete.
     *
     * @return true if there are active animations, false otherwise
     */
    bool hasActiveItemAnimations() const { return !_activeItemUseAnimations.empty(); }

    /**
     * Creates a sequence of animated text popups at a screen location.
     * Each popup animates in (scale+fade), displays, then fades out.
     * Popups with non-zero delaySeconds are spawned after the specified delay.
     *
     * @param screenPosition  On-screen position where popups appear
     * @param popups          Sequence of FloatingPopupData defining each popup
     */
    void createFloatingPopup(
        const cugl::Vec2& screenPosition,
        const std::vector<FloatingPopupData>& popups
    );

    /**
     * Queues damage popups for any stun effects in an enemy-effect batch.
     *
     * Each popup uses the effect's configured delay so the visual appears when
     * the stun damage is expected to resolve.
     *
     * @param enemyEffects The enemy effects produced by an item use.
     * @param position Screen-space position where stun damage popups should appear.
     * @param houseAffinityMultiplier House-role and affinity multiplier used to resolve stun damage.
     * @param upgradeMultiplier Upgrade streak multiplier used to resolve stun damage.
     */
    void scheduleStunDamagePopups(const std::vector<EnemyEffectMessage>& enemyEffects, const cugl::Vec2& position, float houseAffinityMultiplier, float upgradeMultiplier);

    /**
     * Spawns a floating popup showing the heal amount when Gaia's rock is used on the boss.
     *
     * @param dropPos    The screen-space position where the popup should appear.
     * @param healAmount The amount of health restored to the boss.
     */
    void handleGaiaRockPopup(cugl::Vec2 dropPos, float healAmount);


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

    /**
     * Immediately builds the scene-graph nodes for one floating popup and adds it to
     * the active animation list. Creates a container node sized to the text bounds,
     * adds 8 black outline copies at cardinal and diagonal offsets, then adds the
     * colored label on top. All children are anchored to the container center.
     *
     * @param data      Visual and timing parameters for the popup, including fill and stroke color.
     * @param position  Screen-space center position for the popup.
     */
    void spawnSingleFloatingPopup(const FloatingPopupData& data, const cugl::Vec2& position);

    /**
     * Advances all pending and active floating popups by one frame.
     * Pending popups are spawned once their delay timer elapses.
     * Active popups animate through scale-in, display, and fade-out phases, then
     * are removed from the scene graph when complete.
     *
     * @param dt  Delta time in seconds.
     */
    void updatePopupAnimations(float dt);

    /**
     * Advances delayed stun damage popups and spawns any whose delay elapsed.
     *
     * @param dt Delta time in seconds.
     */
    void updateStunDamagePopups(float dt);

    /**
     * Returns the screen-space drop position of the given item's physics body.
     * Falls back to the viewport center (55% height) when no body is found.
     *
     * @param itemId  The item instance whose body position to resolve.
     * @return        Screen-space position to anchor popups at.
     */
    cugl::Vec2 resolveItemDropPosition(ItemInstance::ItemId itemId) const;

    /**
     * Builds the ordered popup sequence for an attack item use.
     *
     * Produces a 3-entry sequence when the side multiplier is neutral (≈1.0):
     *   base damage (grey) → house multiplier (yellow) → final damage (color-coded)
     *
     * Produces a 5-entry sequence when a meaningful side multiplier is present:
     *   base → upgrade mult? → house mult? → pre-enemy damage → side mult? → final damage
     *
     * @param baseValue          Item's raw base damage.
     * @param houseAffinityMultiplier House-role and affinity multiplier.
     * @param upgradeMultiplier  Upgrade streak multiplier, if any.
     * @param sideMultiplier     Enemy side multiplier for the attacking player.
     * @param preSideDamage      Damage after house multiplier, before side multiplier.
     * @param finalDamage        Damage after all multipliers applied.
     * @param valueFontSize      Font size for value popups (base, pre-enemy, final).
     * @param multiplierFontSize Base font size for multiplier popups (house, side).
     * @return Ordered list of FloatingPopupData for the sequence.
     */
    std::vector<FloatingPopupData> buildAttackDamagePopups(
        float baseValue, float houseAffinityMultiplier, float upgradeMultiplier, float sideMultiplier,
        float preSideDamage, float finalDamage,
        float valueFontSize, float multiplierFontSize
    ) const;

    /**
     * Builds the popup sequence shown when Cerberus's drain-shield absorbs and reverses
     * incoming damage into a heal. Shows the raw hit, the negative drain multiplier badge,
     * and the resulting heal amount.
     *
     * @param preSideDamage    Damage before the side multiplier (what the player would have dealt).
     * @param sideMultiplier   The negative side multiplier (e.g. -0.8).
     * @param valueFontSize    Base font size for the damage/heal values.
     * @param multiplierFontSize Base font size for the multiplier badge.
     * @return Ordered list of FloatingPopupData for the sequence.
     */
    std::vector<FloatingPopupData> buildCerberusDefenseHealPopup(
        float preSideDamage, float sideMultiplier,
        float valueFontSize, float multiplierFontSize
    ) const;

    /**
     * Builds the ordered popup sequence for a heal support item use.
     *
     * Returns a 3-entry sequence when a house multiplier is active:
     *   base heal (grey) → multiplier (yellow) → final heal (green)
     *
     * Returns a 1-entry sequence when the multiplier is neutral (≈1.0):
     *   final heal (green)
     *
     * @param baseValue     Item's raw base heal value.
     * @param resolvedHeal  Final resolved heal after house/affinity multipliers.
     * @param def           Item definition used to detect additional support effects such as regen.
     * @param shouldShowEffectPopup Whether effect-specific popups should be shown.
     * @param charmActive   Whether charm should modify effect-specific popup values.
     * @return Ordered list of FloatingPopupData for the sequence.
     */
    std::vector<FloatingPopupData> buildHealPopups(float baseValue, float resolvedHeal,
                                                   const std::shared_ptr<const ItemDef>& def,
                                                   bool shouldShowEffectPopup, bool charmActive) const;

    /**
     * Fires visual popups for any shield or barrier effects on a support item.
     * Shield effects show "[X.X]" in cyan; barrier effects show "[XX%]" in purple,
     * where the percentage is the damage reduction (e.g. multiplier 0.5 → "50%").
     * Must be called before useItemById so shield-only items (which return magnitude=0)
     * still produce a popup.
     *
     * @param def      The item definition whose effects to scan.
     * @param dropPos  Screen-space position where popups appear.
     * @param shouldShowEffectPopup  Whether the effect popup should appear or not.
     * @param hasHealingPopup Whether a primary heal popup will also be shown for this item use.
     * @param charmActive Whether charm should modify effect-specific popup values.
     */
    void spawnDefensiveEffectPopups(const std::shared_ptr<const ItemDef>& def, const cugl::Vec2& dropPos,
                                    bool shouldShowEffectPopup, bool hasHealingPopup, bool charmActive);

    /**
     * Handles the shared ally-target branch for attack items and returns whether it fully resolved the item use.
     *
     * Applies any client-side pending resurrection cache needed to mask stale host snapshots,
     * broadcasts ally-target support effects to the host on non-host clients, and early-outs
     * the attack pipeline when the item is configured to target all allies instead of the enemy.
     *
     * @param itemId The item instance ID being used.
     * @param def The item definition that controls attack target routing and effects.
     * @param local The local player performing the attack.
     * @param resolvedMagnitude The resolved attack magnitude returned by `useItemById`.
     * @param shouldApplyEffects Whether the item's configured effects should be dispatched.
     * @return True if the item targeted all allies and was fully handled here; false if enemy-target attack handling should continue.
     */
    bool handleAllyTargetAttack(ItemInstance::ItemId itemId, const std::shared_ptr<const ItemDef>& def, Player* local, float resolvedMagnitude, bool shouldApplyEffects);

    /**
     * Reapplies a pending client-side resurrection after stale host snapshots, until host sync catches up.
     *
     * Used only on non-host clients after `GameState::networkUpdate()` so a just-used
     * resurrection item is not visually reverted by an older authoritative snapshot.
     */
    void applyPendingResurrectionSync();

    /**
     * Reapplies pending client-side timed party effects after stale host snapshots, until host sync catches up.
     */
    void applyPendingPartyEffectSyncs();

    /**
     * Applies queued frenzy support effects to item spawning and inventories.
     *
     * @param supportEffects Support-effect messages received during the current network update.
     */
    void processFrenzyEffects(const std::vector<SupportEffectMessage>& supportEffects);

    /**
     * Applies a frenzy item-spawn override and clears every player's inventory.
     *
     * @param itemInterval New item spawn interval while frenzy is active.
     * @param duration Duration of the frenzy override in seconds.
     */
    void applyFrenzyEffect(float itemInterval, float duration);

    /**
     * Synchronizes local frenzy state from the latest host snapshot.
     *
     * @param itemInterval Host-authoritative item spawn interval.
     * @param duration Remaining host-authoritative frenzy duration.
     */
    void syncFrenzyEffect(float itemInterval, float duration);

    /**
     * Applies queued or requested forge effects using host-authoritative seeds.
     *
     * @param forgeEffects  The forge effect messages received during the current network update.
     */
    void processForgeEffects(const std::vector<ForgeEffectMessage>& forgeEffects);

    /**
     * Redefines existing local item instances for forge and refreshes any visible widgets.
     *
     * @param chance  Chance in [0, 1] that each rare item upgrades to divine.
     * @param seed    Deterministic base seed used to derive per-player forge rolls.
     */
    void applyForgeEffect(float chance, int seed);

    /**
     * Plays the item's defined use sound, or the generic "support" sound if none is set.
     *
     * @param def  The item definition.
     */
    void playSupportItemSound(const std::shared_ptr<const ItemDef>& def);
    
#pragma mark - Inventory UI

    /**
     * Creates a scene-node widget for the given item and adds it to the
     * inventory container. Texture is chosen by item type (attack or support).
     *
     * @param item  The item instance to represent.
     * @return      The new widget node, or nullptr if assets were missing.
     */
    std::shared_ptr<cugl::scene2::SceneNode> createItemWidget(const ItemInstance& item);

    /**
     * Returns a random valid inventory position for a newly spawned item widget.
     *
     * @param widgetSize  The size of the item widget, used to keep it within bounds.
     * @return            A random position within the inventory area.
     */
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
     * @return        The newly created BoxObstacle body registered in the physics world.
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

    /**
     * Smoothly animates each item widget's scale towards its current target.
     *
     * @param dt  Delta time in seconds.
     */
    void updateItemWidgetScales(float dt);

    /**
     * Spawns a short-lived shrinking ghost visual for a consumed item.
     *
     * @param sourceWidget  The scene node of the consumed item, used as the animation source.
     * @param itemDef       The item definition used to select the correct ghost texture.
     */
    void spawnConsumedItemAnimation(const std::shared_ptr<cugl::scene2::SceneNode>& sourceWidget,
                                    const std::shared_ptr<const ItemDef>& itemDef);

    /**
     * Advances and cleans up active consumed-item ghost animations.
     *
     * @param dt  Delta time in seconds.
     */
    void updateConsumedItemAnimations(float dt);

    /**
     * Updates active corrode animations and removes items when animation completes.
     *
     * @param dt  Delta time in seconds.
     */
    void updateCorrodedItemAnimations(float dt);

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

    /**
        * Helper function to spawn an item within the tutorial
        * @param defID       The id of the item to spawn
        * @param passDirection the nature in which the item should spawn. (0 = Spawn, 1 = Passed from left, 2 = Passed from right).
        */
    void spawnTutorialItem(const std::string& defId, int passDirection);
    
    /** Sync player inventory and item widgets displayed on screen */
    void syncInventoryWidgets();

    /**
     * Refreshes existing widget textures after item instances are redefined in place.
     */
    void refreshInventoryWidgetTextures();

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
     * Repositions the tooltip node above the currently dragged icon.
     * Must only be called while _draggedIcon and _tooltipNode are valid.
     */
    void updateTooltipPosition();

#pragma mark -
#pragma mark Tutorial

    /**
     * This method ensures that only the specified zone is visible at any given time,
     * effectively guiding the player's attention to a specific interaction area.
     * @param zone The string identifier for the area to highlight.
     * Accepted values: "attack", "left_support", "right_support", "pass_left", "pass_right".
     */
    void setTutorialHighlight(const std::string& zone);
    
    /**
     * Deactivates all tutorial highlights.
     * Resets the tutorial state to "none" and hides all highlight area nodes.
     */
    void clearTutorialHighlight();
    
    /**
     * Toggles the visibility of the support zone highlights.
     * Used during specific tutorial segments where support mechanics are either
     * introduced or restricted.
     * @param disable If true, hides support zones; if false, reveals them.
     */
    void setTutorialDisableSupportZonesVisibility(bool disable);
    
    /**
     * Sets a singular zone to be active.
     * @param zone The input zone that should be active, treating all others as inactive.
     */
    void setTutorialAllowedDropZone(InputController::Action zone);

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
     * Returns whether debug mode is currently enabled.
     *
     * @return true if debug overlays are active, false otherwise.
     */
    bool isDebugMode() const {return _debugMode; }

#pragma mark - Networking
    /**
     * Checks if any updates about the state of the game were sent over the network.
     * If we are a client, we update the state of the game to match the hosts' version and process any passes sent to us.
     * If we are the host, we process any attack, heal, effect, and pass messages, then tick timed player effects.
     * After doing so, we send out a new authoritative version of the game state as the host.
     *
     * @param dt  The elapsed time since the previous frame, in seconds.
     */
    void handleNetworkUpdates(float dt);
    
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
    
    /**
     * Returns a reference to the item controller owned by this scene.
     * Exposed so LobbyScene can pass it to assignMissingHousesForAI()
     * when the host presses Begin Quest.
     *
     * @return  A reference to the ItemController owned by this scene.
     */
    ItemController& getItemController() { return _itemController; }
};
#endif /* __GAME_SCENE_H__ */
