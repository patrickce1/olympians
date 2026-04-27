#include <cugl/cugl.h>
#include <algorithm>
#include <cmath>
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

/** Constant to define Box2D obstacle physics base unit */
constexpr float ITEM_SPEED_UNITS = 1.0f;

#pragma mark Sliding Item Physics Constants

/** Deceleration rate for sliding items per second (units/sec²) */
constexpr float ITEM_SLIDE_FRICTION_DECELERATION = 2500.0f;
/** Velocity threshold below which a sliding item is considered to have settled (units/sec) */
constexpr float ITEM_SLIDE_VELOCITY_SETTLE_THRESHOLD = 10.0f;
/** Duration of the snapback animation when a dropped item returns to inventory (seconds) */
constexpr float ITEM_SLIDE_SNAPBACK_ANIMATION_TIME = 0.3f;
/** Maximum speed cap for sliding items to prevent excessive velocities (units/sec) */
constexpr float ITEM_MOVEMENT_MAX_SPEED = 2000.0f;
// Use a nominal dt for velocity estimation to avoid frame-rate dependency
constexpr float VELOCITY_DT_ESTIMATE = 0.016f; // ~60fps estimate
//How much larger the item should be when picked up. So it should be .xx percent larger
constexpr float ITEM_PICKUP_SCALE = 1.12f;
//Base scaling of items (when not being held)
constexpr float ITEM_NORMAL_SCALE = 1.0f;
//Defines how fast should the item increase in size from ITEM_NORMAL_SCALE to ITEM_PICKUP_SCALE
constexpr float ITEM_SCALE_SPEED = 14.0f;
//Defines how long it should take for an item that has been used (through the means of passing, attacking, or supporting)
constexpr float ITEM_CONSUME_ANIMATION_DURATION = 0.12f;
//Defines how large the item is once it has been used. So it shrinks to this size.
constexpr float ITEM_CONSUME_END_SCALE = 0.15f;

#pragma mark HealthState

/**
 * @enum HealthState
 * Represents a player’s health condition for UI purposes.
 *
 * - FULL: Player has maximum health.
 * - HALF: Player has below 50% health.
 * - DEAD: Player has zero health.
 */
enum class HealthState { FULL, HALF, DEAD };

/**
 * Determines the health state of a player based on current and maximum health.
 *
 * @param current Current health value of the player.
 * @param max Maximum health value of the player.
 * @return HealthState corresponding to FULL, HALF, or DEAD.
 */
static HealthState getHealthState(float current, float max) {
    if (max <= 0 || current <= 0) return HealthState::DEAD;
    float ratio = current / max;
    if (ratio <= 0.5f) return HealthState::HALF;
    return HealthState::FULL;
}

/**
 * Broadcasts the resolved support effects of a used support item to the host.
 *
 * Support items are normalized into one or more `SupportEffectMessage` payloads
 * so the host can apply the same authoritative result to the target player.
 * Items without explicit effects are treated as direct heals using the provided
 * resolved magnitude. Shield and barrier items instead send their effect-specific
 * tuning values from the item definition together with the configured duration.
 *
 * @param network   The network controller used to send host-directed updates.
 * @param def   The item definition describing the support item's effects.
 * @param resolvedMagnitude   The resolved support magnitude calculated for this item use.
 * @param targetPlayerID     The 0-based slot index of the player receiving the effect.
 */
static void broadcastSupportEffects(NetworkController& network,
                                    const ItemDef& def,
                                    float resolvedMagnitude,
                                    int targetPlayerID) {
    for (const ItemDef::Effect& effect : def.getEffects()) {
        switch (effect.type) {
            case ItemDef::EffectType::Shield:
                network.broadcastSupportEffect(SupportEffectType::Shield,
                    effect.mitigation,
                    effect.duration,
                    targetPlayerID);
                break;
            case ItemDef::EffectType::Barrier:
                network.broadcastSupportEffect(SupportEffectType::Barrier,
                    effect.multiplier,
                    effect.duration,
                    targetPlayerID);
                break;
            case ItemDef::EffectType::Stun:
            case ItemDef::EffectType::Love:
            case ItemDef::EffectType::Vulnerable:
                break;
        }
    }
}

/**
 * Collects the resolved enemy-facing effects of an attack item.
 *
 * Attack items without explicit enemy effects return an empty list. Attack items
 * with explicit effects serialize those effect payloads so the host can apply the
 * same authoritative result and replicate it through snapshots.
 *
 * @param def                The item definition describing the attack item's effects.
 * @param resolvedMagnitude  The resolved attack magnitude calculated for this item use.
 * @param playerIndex    The index of the player applying the enemy effect.
 * @return   The collection of enemy effects to be applied this frame.
 */
static std::vector<EnemyEffectMessage> collectEnemyEffects(const ItemDef& def, float resolvedMagnitude, int playerIndex) {
    std::vector<EnemyEffectMessage> enemyEffects;
    for (const ItemDef::Effect& effect : def.getEffects()) {
        EnemyEffectMessage effectMsg;
        effectMsg.duration = effect.duration;
        effectMsg.playerIndex = playerIndex;
        effectMsg.applyToAllSides = false;

        switch (effect.type) {
            case ItemDef::EffectType::Stun:
                effectMsg.effectType = EnemyEffectType::Stun;
                effectMsg.magnitude = resolvedMagnitude;
                enemyEffects.push_back(effectMsg);
                break;
            case ItemDef::EffectType::Love:
                effectMsg.effectType = EnemyEffectType::Love;
                effectMsg.magnitude = resolvedMagnitude;
                enemyEffects.push_back(effectMsg);
                break;
            case ItemDef::EffectType::Vulnerable:
                effectMsg.effectType = EnemyEffectType::Vulnerable;
                effectMsg.magnitude = effect.multiplier;
                effectMsg.applyToAllSides = effect.applyToAllSides;
                enemyEffects.push_back(effectMsg);
                break;
            case ItemDef::EffectType::Shield:
            case ItemDef::EffectType::Barrier:
                break;
        }
    }
    return enemyEffects;
}

/** Sends all collected enemy-facing effects of an attack item to the host.
 *
 * @param network    The network to send the enemy effects over.
 * @param enemyEffects   The collection of enemy effects to send over the network.
 */
static void broadcastEnemyEffects(NetworkController& network, const std::vector<EnemyEffectMessage>& enemyEffects) {
    for (const EnemyEffectMessage& effectMsg : enemyEffects) {
        network.broadcastEnemyEffect(effectMsg.effectType,
            effectMsg.magnitude,
            effectMsg.duration,
            effectMsg.playerIndex,
            effectMsg.applyToAllSides);
    }
}

/**
 * Returns the texture name to use for a player icon based on health and house.
 *
 * @param state HealthState of the player.
 * @param houseName The player's house or class (used to select house-specific icons).
 * @return std::string The texture identifier corresponding to this health state and house.
 */
static std::string getHealthTexture(HealthState state, std::string houseID) {
    if (houseID.empty()) return "basicTeammateIcon";
    switch (state) {
        case HealthState::FULL: return houseID + "Regular";
        case HealthState::HALF: return houseID + "MidHealth";
        case HealthState::DEAD: return houseID + "Death";
    }
    return "basicTeammateIcon";
}

/**
 * Returns whether a damage blink should currently render its red tint.
 *
 * The timer counts down from the total blink duration to zero. We alternate the
 * red overlay on fixed cadence bands until the timer expires.
 */
static bool shouldShowDamageBlink(float timer, float interval) {
    if (timer <= 0.0f || interval <= 0.0f) {
        return false;
    }
    const int phase = static_cast<int>(timer / interval);
    return (phase % 2) == 0;
}

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
        _gameArea->setContentWidth(dimen.width);
        
        // Left and right teammate icon
        _leftPlayerSlot = std::dynamic_pointer_cast<scene2::PolygonNode>(
             _assets->get<scene2::SceneNode>("gameScene.gameArea.leftIcon.playerIcon.player"));
        
        _rightPlayerSlot = std::dynamic_pointer_cast<scene2::PolygonNode>(
             _assets->get<scene2::SceneNode>("gameScene.gameArea.rightIcon.playerIcon.player"));
        
        _leftPlayerName = std::dynamic_pointer_cast<scene2::Label>(
             _assets->get<scene2::SceneNode>("gameScene.gameArea.leftIcon.username"));
        
        _rightPlayerName = std::dynamic_pointer_cast<scene2::Label>(
             _assets->get<scene2::SceneNode>("gameScene.gameArea.rightIcon.username"));
        
        // This is the boss animation sprite container from the JSON, positioned exactly like the static sprite
        _bossSprite = std::dynamic_pointer_cast<scene2::SceneNode>((_gameArea->getChildByName("bossAnimationSpace")));
        
        // This is the special effects node, this is where all the animated effects will go.
        _specialEffectsLayer = scene2::SceneNode::allocWithBounds(dimen);
        _specialEffectsLayer->setContentSize(dimen);
        _specialEffectsLayer->setAnchor(cugl::Vec2::ANCHOR_CENTER);
        _specialEffectsLayer->setPosition(cugl::Vec2(dimen.width / 2.0f, dimen.height / 2.0f));
        _scene->addChild(_specialEffectsLayer);
        _supportLeftArea = _gameArea->getChildByName("supportLeft");
        _supportRightArea = _gameArea->getChildByName("supportRight");
    }
    
    if (_inventory) {
        auto invBG = _inventory->getChildByName<cugl::scene2::NinePatch>("background");
        invBG->setContentWidth(dimen.width);
        
        _playerHealthBar = std::dynamic_pointer_cast<scene2::ProgressBar>(
            _assets->get<scene2::SceneNode>("gameScene.inventory.playerHealth.healthBarFill"));
        
        _bossHealthBar = std::dynamic_pointer_cast<scene2::ProgressBar>(
               _assets->get<scene2::SceneNode>("gameScene.inventory.enemyHealth.healthFill"));
        
        _bossName = std::dynamic_pointer_cast<scene2::Label>(
               _assets->get<scene2::SceneNode>("gameScene.inventory.bossName.label"));
        
        _playerName = std::dynamic_pointer_cast<scene2::Label>(
               _assets->get<scene2::SceneNode>("gameScene.inventory.playerInfo.playerName.label"));
        
        _playerHouseName = std::dynamic_pointer_cast<scene2::Label>(
               _assets->get<scene2::SceneNode>("gameScene.inventory.playerInfo.playerHouse.label"));
        
        _localPlayerSlot = std::dynamic_pointer_cast<scene2::PolygonNode>(
               _assets->get<scene2::SceneNode>("gameScene.inventory.playerIcon.player"));
    }
    
    addChild(_scene);
    return true;
}

/**
 * Initializes the Box2D physics world to support physics objects in the scene space.
 *
 * @return true if the physics world was successfully created.
 */
bool GameScene::initPhysicsWorld() {
    // Expand bounds beyond screen to accommodate spawning and physics overflow
    // Must include side spawn positions (-10% to 110% of screen width)
    // and account for item body sizes
    cugl::Size screenSize = getSize();
    Rect worldBounds(-screenSize.width * 0.25f, -300.0f, 
                     screenSize.width * 1.5f, screenSize.height + 400.0f);
    _itemPhysicsWorld = cugl::physics2::ObstacleWorld::alloc(worldBounds, Vec2::ZERO);
    if (!_itemPhysicsWorld) {
        CULogError("Failed to create item physics world");
        return false;
    }
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
    if (!_gameState.init(_itemController, _assets)) {
        return false;
    }
    return true;
}

/**
 * Loads data-driven tuning values used by teammate blink UI.
 *
 * Missing or invalid fields leave the current defaults unchanged.
 */
void GameScene::initBlinkConfig() {
    if (!_assets) {
        return;
    }

    auto config = _assets->get<JsonValue>("gameSceneConfig");
    if (!config || !config->isObject()) {
        return;
    }

    auto blinkConfig = config->get("teammateBlink");
    if (!blinkConfig || !blinkConfig->isObject()) {
        return;
    }

    if (blinkConfig->has("duration") && blinkConfig->get("duration")->isNumber()) {
        _blinkDuration = std::max(0.0f, blinkConfig->getFloat("duration"));
    }
    if (blinkConfig->has("interval") && blinkConfig->get("interval")->isNumber()) {
        _blinkInterval = std::max(0.0f, blinkConfig->getFloat("interval"));
    }
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
    
    _attackZones = {{InputController::Action::DROP_BOSS, Rect(w * 0.05f, h * 0.45f, w * 0.9f, h * 0.40f)}};
    
    // Setup up texture node according to zone size
    _attackArea = PolygonNode::allocWithTexture(_assets->get<cugl::graphics::Texture>("attackZone"));
    _gameArea->addChild(_attackArea);
    Rect attackAreaRect = _attackZones[0].second;

    _attackArea->setAnchor(Vec2::ANCHOR_CENTER);
    _attackArea->setContentSize(attackAreaRect.size);
    _attackArea->setPosition(_gameArea->getSize()/2);
    _attackArea->setVisible(false);
    
    _supportZones = {
        {InputController::Action::DROP_ALLY_LEFT,  Rect(-w * 0.149f, h * 0.45f, w * 0.399f, h * 0.40f)},
        {InputController::Action::DROP_ALLY_RIGHT, Rect(w * 0.75f,   h * 0.45f, w * 0.399f, h * 0.40f)},
    };
      
    _inventoryZones = {
        {InputController::Action::NONE, Rect(w * 0.10f, 0, w * 0.80f, h * 0.35f)}
    };
    
    _passZones = {
        {InputController::Action::PASS_LEFT,  Rect(-w * 0.149f, 0, w * 0.18f, h * 0.35f)},
        {InputController::Action::PASS_RIGHT, Rect(w * 0.971f,   0, w * 0.18f, h * 0.35f)}
    };
}

/**
 * Initializes the background and boss images for the current game scene.
 *
 * This function sets the visual assets for both the background and the boss
 * based on the active enemy in the game state. It retrieves the enemy ID and
 * uses it to construct texture keys for the corresponding assets.
 */
void GameScene::initBackgroundAndBossImage() {
    if (!_network) return;
    
    auto boss = _gameState.getEnemy()->getId();
    auto backgroundImage = std::dynamic_pointer_cast<scene2::PolygonNode>( _gameArea->getChildByName("background"));
    backgroundImage->setTexture(_assets->get<cugl::graphics::Texture>(boss + "Background"));
    
    auto bossImage = std::dynamic_pointer_cast<scene2::PolygonNode>( _gameArea->getChildByName("bossIdle"));
    bossImage->setTexture(_assets->get<cugl::graphics::Texture>(boss));
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
bool GameScene::init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController, AudioController* audio) {
    if (assets == nullptr) {
        return false;
    }
    if (!Scene2::initWithHint(Size(0, SCENE_HEIGHT))) {
        return false;
    }

    _assets = assets;
    _network = networkController;
    _audio = audio;
    
    initBlinkConfig();

    if (!initSceneGraph()) {
        return false;
    }
    
    initInputZones();

    if (!initPhysicsWorld()) {
        return false;
    }

    if (!initGameSystems()) {
        return false;
    }
    
    _assets->loadDirectory("json/itemTextures.json");
    _assets->loadDirectory("json/itemAnimations.json");
    _assets->loadDirectory("json/houseInGameIcons.json");

    // Load animation registry from the already-registered enemyAnimations JSON asset
    loadAnimationRegistry();
    
    // Pre-create all animation sprites to eliminate runtime stuttering
    if (!initializeAllEnemyAnimations()) {
        CULogError("Failed to initialize enemy animations");
        return false;
    }
    
    // Pass animation registry to EnemyController for attack phase detection
    _enemyController.setAnimationRegistry(&_animationRegistry);

    /*since networking not initialized yet, just assume we are the host
    we recheck if we are player 0 whenever another scene transitions back into this one*/
    setLocalPlayer(0);
    _status = Status::PLAYING;
    setDebugMode(false);
    
    // Set player icon textures immediately (normally done in update, but we need them visible on first render)
    updatePlayerAndTeammateIcons(0.0f);
    
    setActive(false);
    return true;
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void GameScene::dispose() {
    if (_active || _scene || _itemPhysicsWorld) {
        // Clear all active animations before clearing the scene
        clearItemUseAnimations();
        
        removeAllChildren();
        _scene      = nullptr;
        _gameArea   = nullptr;
        _inventory  = nullptr;
        _attackArea = nullptr;
        _bossNode   = nullptr;
        _leftPlayerSlot = nullptr;
        _rightPlayerSlot = nullptr;
        _leftPlayerName = nullptr;
        _supportLeftArea = nullptr;
        _supportRightArea = nullptr;
        _rightPlayerName = nullptr;
        _bossHealthBar = nullptr;
        _playerHealthBar = nullptr;
        _playerName = nullptr;
        _bossName = nullptr;
        _playerHouseName = nullptr;
        _network = nullptr;
        _draggedIcon = nullptr;
        _enemyAnimationSpriteNodes.clear();
        _currentVisibleAnimationSprite = nullptr;
        _itemWidgets.clear();
        _itemWidgetScales.clear();
        _itemWidgetScaleTargets.clear();
        _consumedItemAnimations.clear();
        _activeFloatingPopups.clear();
        _pendingFloatingPopups.clear();
        _itemBodies.clear();
        if (_itemPhysicsWorld) {
            _itemPhysicsWorld->dispose();
            _itemPhysicsWorld = nullptr;
        }
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
    if (!_network || _network->checkConnection() != NetworkController::CONNECTED) return;

    const auto& slotToPlayer = _network->getNetworkedPlayers();
    const int totalSlots = (int)_gameState.getPlayers().size();

    for (int i = 0; i < totalSlots; i++) {
        auto pair = slotToPlayer.find(i);
        if (pair != slotToPlayer.end()) {
            _gameState.setRealPlayer(i, pair->second.username, pair->second.houseID);
        } else {
            _gameState.demoteToAI(i, _network->getAIHouse(i));
        }
    }

    // Log every slot so we can verify AI slots are actually PlayerAI at game start
    for (int i = 0; i < totalSlots; i++) {
        Player* player = _gameState.getPlayerBySlot(i);
        CULog("GameScene::updateNetworkOrder — slot %d: name='%s' house='%s' isAI=%s",
              i,
              player->getPlayerName().c_str(),
              player->getHouseName().c_str(),
              player->isAI() ? "true" : "false");
    }

    setLocalPlayer(_network->getLocalPlayerNumber());

    _leftPlayerName->setText(_gameState.getLocalPlayer()->getLeftPlayer()->getPlayerName());
    _rightPlayerName->setText(_gameState.getLocalPlayer()->getRightPlayer()->getPlayerName());

    _gameState.setEnemy(_network->getEnemy(), _assets);

    initBackgroundAndBossImage();
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
            updateNetworkOrder();
            
            // Re-initialize AI players after updateNetworkOrder() rebuilds
            // AI slots via demoteToAI(). demoteToAI() creates EasyPlayerAI
            // objects but cannot call init() since it has no ItemController.
            // Without this, _db is null and the AI crashes on first update.
            _gameState.initAI(_itemController);
            
            // Reset enemy animation state for clean start
            _enemyAnimationCurrentDirection = 0;
            
            // Hide animation sprite on scene reset
            if (_currentVisibleAnimationSprite) {
                _currentVisibleAnimationSprite->setVisible(false);
            }
        }
    }
}

/**
 * Resets the scene to its start-of-round state.
 * Clears all item widgets, cancels any active drag, resets the glow
 * effect, and clears every player's inventory via GameState::reset().
 */
void GameScene::reset() {
    _draggedIcon = nullptr;
    if (_draggedItemId != 0) {
        _itemWidgetScaleTargets[_draggedItemId] = ITEM_NORMAL_SCALE;
    }
    _draggedItemId = 0;
    _draggedItemDef = nullptr;
    _dragStartBodyPosition = Vec2::ZERO;
    _glowAction = InputController::Action::NONE;
    _status = Status::PLAYING;
    _glowTimer  = 0;
    _slotsDemotedToAI.clear();
    _itemWidgetScales.clear();
    _itemWidgetScaleTargets.clear();
    clearConsumedItemAnimations();

    // Clear any active animations before resetting
    clearItemUseAnimations();
    for (auto& popupAnim : _activeFloatingPopups) {
        if (popupAnim.node) popupAnim.node->removeFromParent();
    }
    _activeFloatingPopups.clear();
    _pendingFloatingPopups.clear();
    _itemController.reset();

    std::vector<ItemInstance::ItemId> itemIds;
    itemIds.reserve(_itemWidgets.size());
    for (const auto& [id, widget] : _itemWidgets) {
        itemIds.push_back(id);
    }
    for (ItemInstance::ItemId itemId : itemIds) {
        removeItemWidget(itemId);
    }

    // Delegate inventory clearing and health resetting to the model.
    _gameState.reset();
    resetTeammateBlinkState();
}

#pragma mark -
#pragma mark Player Assignment

/**
 * Assigns the local player slot for this machine.
 * Delegates to GameState::setLocalPlayer().
 */
void GameScene::setLocalPlayer(int assignedIndex) {
    _gameState.setLocalPlayer(assignedIndex);
    resetTeammateBlinkState();
}

#pragma mark -
#pragma mark Action Handlers

/**
 * Handles the local player dropping an attack item on the boss zone.
 * Applies the dragged attack item to the enemy.
 *
 *@param itemId  The id of the item being handled.
 */
bool GameScene::handleAttack(ItemInstance::ItemId itemId) {
    auto enemy    = _gameState.getEnemy();
    Player* local = _gameState.getLocalPlayer();
    if (!enemy || !local || itemId == 0) return false;

    for (const ItemInstance& item : local->getInventory()) {
        if (item.getId() != itemId) {
            continue;
        }

        auto def = _itemController.getDatabase().getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Attack) {
            // Play the item use sound if defined, otherwise play the attack sound
            const std::string& itemUseSound = def->getItemUseSound();
            if (!itemUseSound.empty()) {
                _audio->playSoundUnique(itemUseSound);
            } else {
                _audio->playSoundUnique("attack");
            }
            
            // Delegate to the appropriate attack handler based on animation config
            if (def->hasItemUseAnimation()) {
                return handleAnimatedAttack(itemId, item, def, local, enemy.get());
            } else {
                return handleImmediateAttack(itemId, item, def, local, enemy.get());
            }
        }
        return false;
    }
    return false;
}

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
bool GameScene::handleAnimatedAttack(ItemInstance::ItemId itemId, const ItemInstance& item,
                                      const std::shared_ptr<const ItemDef>& def,
                                      Player* local, Enemy* enemy) {
    const cugl::Vec2 dropPos = resolveItemDropPosition(itemId);
    // Calculate damage upfront for the animation
    const float resolvedMagnitude = local->useItemById(item.getId(), *enemy, _itemController.getDatabase());
    if (resolvedMagnitude < 0.0f) {
        return false;
    }

    const auto& animConfig = def->getItemUseAnimation();
    CULog("Player attacked enemy with item (animation queued, damage deferred to resolution: %.1f)",
          resolvedMagnitude);

    // Vec2::ZERO signals startItemUseAnimation to use the default viewport center.
    const cugl::Vec2 animPos = animConfig.centerOnDropLocation ? dropPos : cugl::Vec2::ZERO;
    const float baseValue      = def->getBaseValue();
    const float totalMultiplier = (baseValue > 0.0f) ? resolvedMagnitude / baseValue : 1.0f;
    const std::vector<EnemyEffectMessage> enemyEffects =
        (!_network->isHost()) ? collectEnemyEffects(*def, resolvedMagnitude, local->getPlayerNumber())
                              : std::vector<EnemyEffectMessage>{};

    startItemUseAnimation(animConfig, resolvedMagnitude, animPos, 0);
    if (!_activeItemUseAnimations.empty()) {
        _activeItemUseAnimations.back().popupPosition    = dropPos;
        _activeItemUseAnimations.back().baseValue        = baseValue;
        _activeItemUseAnimations.back().totalMultiplier  = totalMultiplier;
        _activeItemUseAnimations.back().enemyEffects     = enemyEffects;
    }

    return true;
}

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
bool GameScene::handleImmediateAttack(ItemInstance::ItemId itemId, const ItemInstance& item,
                                       const std::shared_ptr<const ItemDef>& def,
                                       Player* local, Enemy* enemy) {
    const cugl::Vec2 dropPos = resolveItemDropPosition(itemId);

    const float resolvedMagnitude = local->useItemById(item.getId(), *enemy, _itemController.getDatabase());
    if (resolvedMagnitude < 0.0f) {
        return false;
    }

    CULog("Player attacked enemy '%s' with item %llu (damage: %.1f, immediate)",
          enemy->getId().c_str(), (unsigned long long)itemId, resolvedMagnitude);

    if (!_network->isHost()) {
        _network->broadcastDamage(resolvedMagnitude, local->getPlayerNumber());
        broadcastEnemyEffects(*_network, collectEnemyEffects(*def, resolvedMagnitude, local->getPlayerNumber()));
    }
    if (_network->isHost() && _audio) {
        _audio->playSoundUnique("enemy_hurt");
        CULog("Host: Attack caused enemy damage, playing enemy_hurt sound");
    }

    const float baseValue      = def->getBaseValue();
    const float totalMultiplier = (baseValue > 0.0f) ? resolvedMagnitude / baseValue : 1.0f;
    const float sideMultiplier  = enemy->getSideMultiplier(local->getPlayerNumber());
    const float finalDamage     = resolvedMagnitude * sideMultiplier;
    createFloatingPopup(dropPos, buildAttackDamagePopups(
        baseValue, totalMultiplier, sideMultiplier, resolvedMagnitude, finalDamage, 26.0f, 17.0f));

    return true;
}

/**
 * Handles the local player dropping a support item on the left ally zone.
 *
 *@param itemId  The id of the item being handled.
 */
bool GameScene::handleSupportLeft(ItemInstance::ItemId itemId) {
    Player* local  = _gameState.getLocalPlayer();
    Player* target = local ? local->getLeftPlayer() : nullptr;
    if (!local || !target || !target->isAlive() || itemId == 0) return false;

    for (const ItemInstance& item : local->getInventory()) {
        if (item.getId() != itemId) continue;

        auto def = _itemController.getDatabase().getDef(item.getDefId());
        if (!def || def->getType() != ItemDef::Type::Support) return false;

        const cugl::Vec2 dropPos = resolveItemDropPosition(itemId);

        // Shield/barrier popups must fire before useItemById because shield-only
        // items return 0 and would be filtered by the magnitude guard below.
        spawnDefensiveEffectPopups(def, dropPos);

        const float resolvedMagnitude = local->useItemById(item.getId(), *target, _itemController.getDatabase());
        if (resolvedMagnitude < 0.0f) return false;

        if (!_network->isHost()) {
            _network->broadcastHeal(resolvedMagnitude, target->getPlayerNumber());
            broadcastSupportEffects(*_network, *def, resolvedMagnitude, target->getPlayerNumber());
        }
            
        playSupportItemSound(def);
        CULog("handleSupportLeft: Healing teammate (%.1f)", resolvedMagnitude);
        
        if (resolvedMagnitude == 0.0f) return true;
        createFloatingPopup(dropPos, buildHealPopups(def->getBaseValue(), resolvedMagnitude));
        return true;
    }
    return false;
}

/**
 * Handles the local player dropping a support item on the right ally zone.
 *
 *@param itemId  The id of the item being handled.
 */
bool GameScene::handleSupportRight(ItemInstance::ItemId itemId) {
    Player* local  = _gameState.getLocalPlayer();
    Player* target = local ? local->getRightPlayer() : nullptr;
    if (!local || !target || !target->isAlive() || itemId == 0) return false;

    for (const ItemInstance& item : local->getInventory()) {
        if (item.getId() != itemId) continue;

        auto def = _itemController.getDatabase().getDef(item.getDefId());
        if (!def || def->getType() != ItemDef::Type::Support) return false;

        const cugl::Vec2 dropPos = resolveItemDropPosition(itemId);

        // Shield/barrier popups must fire before useItemById because shield-only
        // items return 0 and would be filtered by the magnitude guard below.
        spawnDefensiveEffectPopups(def, dropPos);

        const float resolvedMagnitude = local->useItemById(item.getId(), *target, _itemController.getDatabase());
        if (resolvedMagnitude < 0.0f) return false;

        if (!_network->isHost()) {
            _network->broadcastHeal(resolvedMagnitude, target->getPlayerNumber());
            broadcastSupportEffects(*_network, *def, resolvedMagnitude, target->getPlayerNumber());
        }
        playSupportItemSound(def);
        CULog("handleSupportRight: Healing teammate (%.1f)", resolvedMagnitude);
        
        if (resolvedMagnitude == 0.0f) return true;
        createFloatingPopup(dropPos, buildHealPopups(def->getBaseValue(), resolvedMagnitude));
        return true;
    }
    return false;
}

/**
 * Passes the dragged item in the local player's inventory to the left neighbour.
 *
 *@param itemId  The id of the item being handled.
 */
bool GameScene::handlePassLeft(ItemInstance::ItemId itemId) {
    Player* local  = _gameState.getLocalPlayer();
    Player* target = local ? local->getLeftPlayer() : nullptr;
    if (!local || !target || itemId == 0) return false;

    // For real players, verify they're still in the networked players list
    if (!target->isAI()) {
        int targetSlot = target->getPlayerNumber();
        const auto& slotToPlayer = _network->getNetworkedPlayers();
        if (slotToPlayer.find(targetSlot) == slotToPlayer.end()) {
            CULog("Cannot pass to player %d: player slot out of range", targetSlot);
            return false;
        }
    }

    // Check if target player is in disconnected slots
    const auto& disconnected = _network->getDisconnectedSlots();
    if (std::find(disconnected.begin(), disconnected.end(), target->getPlayerNumber()) != disconnected.end()) {
        CULog("Cannot pass to player %d: player is disconnected", target->getPlayerNumber());
        return false;
    }

    for (const ItemInstance& item : local->getInventory()) {
        if (item.getId() != itemId) continue;

        // Capture defId BEFORE removing the item
        std::string defId = item.getDefId();
        local->removeItemById(itemId);
        
        if (!target->isAI()) {
            CULog("Passing left to a real player with the number %d", target->getPlayerNumber());
        }
        else {
            CULog("Passing left to player AI player with number %d", target->getPlayerNumber());
        }

        _network->broadcastPass(defId, target->getPlayerNumber(), 1);  // Direction 1 = left
        _audio->playSoundUnique("whoosh");

        return true;
    }
    return false;
}

/**
 * Passes the dragged item in the local player's inventory to the right neighbour.
 *
 *@param itemId  The id of the item being handled.
 */
bool GameScene::handlePassRight(ItemInstance::ItemId itemId) {
    Player* local  = _gameState.getLocalPlayer();
    Player* target = local ? local->getRightPlayer() : nullptr;
    if (!local || !target || itemId == 0) return false;

    // For real players, verify they're still in the networked players list
    if (!target->isAI()) {
        int targetSlot = target->getPlayerNumber();
        const auto& slotToPlayer = _network->getNetworkedPlayers();
        if (slotToPlayer.find(targetSlot) == slotToPlayer.end()) {
            CULog("Cannot pass to player %d: player slot out of range", targetSlot);
            return false;
        }
    }

    // Check if target player is in disconnected slots
    const auto& disconnected = _network->getDisconnectedSlots();
    if (std::find(disconnected.begin(), disconnected.end(), target->getPlayerNumber()) != disconnected.end()) {
        CULog("Cannot pass to player %d: player is disconnected", target->getPlayerNumber());
        return false;
    }

    for (const ItemInstance& item : local->getInventory()) {
        if (item.getId() != itemId) continue;

        // Capture defId BEFORE removing the item
        std::string defId = item.getDefId();
        local->removeItemById(itemId);
        
        if (!target->isAI()) {
            CULog("Passing right to a real player with the number %d", target->getPlayerNumber());
        }
        else {
            CULog("Passing right to player AI player with number %d", target->getPlayerNumber());
        }

        _network->broadcastPass(defId, target->getPlayerNumber(), 2);  // Direction 2 = right
        _audio->playSoundUnique("whoosh");
        return true;
    }
    return false;
}

/**
* Processes all the passMessages inside of the vector, putting the correct items in the player's inventory
* If we are the host, it will also give the correct items to the AI
* Intended usage: get the pass message vector from the network controller and pass into this function
*/
void GameScene::processNetworkedPasses(std::vector<PassMessage> passes) {
    for (const PassMessage& pass : passes) {
        Player* receiver = _gameState.getPlayerById(pass.playerID);
        if (!receiver) continue;
        
        // Add item to inventory and get its unique ID
        ItemInstance::ItemId itemId = _itemController.giveItemByID(receiver, pass.itemID);
        if (itemId == 0) continue;
        
        // Track it as a passed item so it bypasses inventory limits and spawns from side
        _passedItemIds.insert(itemId);
        
        // Set pass direction on the item for animation
        auto& inventory = const_cast<std::vector<ItemInstance>&>(receiver->getInventory());
        for (auto& item : inventory) {
            if (item.getId() == itemId) {
                item.setPassDirection(pass.passDirection);
                break;
            }
        }
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
 *
 *@param action  The action the dragged action corresponds to.
 *@param itemId  The id of the item being handled.
 */
bool GameScene::handlePlayerActions(InputController::Action action, ItemInstance::ItemId itemId) {
    Player* local = _gameState.getLocalPlayer();
    if (!local) return false;

    switch (action) {
        case InputController::Action::DROP_BOSS:
            if (!local->isAlive()) return false;
            return handleAttack(itemId);
        case InputController::Action::DROP_ALLY_LEFT:
            if (!local->isAlive()) return false;
            return handleSupportLeft(itemId);
        case InputController::Action::DROP_ALLY_RIGHT:
            if (!local->isAlive()) return false;
            return handleSupportRight(itemId);
        case InputController::Action::DROP_INVALID:
            return false;
        case InputController::Action::PASS_LEFT:
            return handlePassLeft(itemId);
        case InputController::Action::PASS_RIGHT:
            return handlePassRight(itemId);
        default:
            return false;
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

    // Track player and enemy health before any updates to detect damage
    auto player = _gameState.getLocalPlayer();
    // Only track health if local player is not AI (AI players shouldn't hear their own hurt sounds)
    float playerHealthBefore = (player && !dynamic_cast<PlayerAI*>(player)) ? player->getCurrentHealth() : 0.0f;
    float enemyHealthBefore = enemy->getCurrentHealth();

    _enemyController.update(dt, enemy, _gameState.getPlayers());

    // Update AI players - this is when they attack the boss AND heal teammates
    for (auto& player : _gameState.getPlayers()) {
        if (auto* ai = dynamic_cast<PlayerAI*>(player.get())) {
            ai->update(dt, *enemy, _itemController);
        }
    }
    
    // Play sounds for LOCAL player and enemy health changes after all updates
    playHealthAndDamageSounds(playerHealthBefore, enemyHealthBefore);
}

/**
 * Hides the enemy animation sprite and shows the static fallback sprite.
 * 
 * Sets visibility on both the animation sprite node and the container,
 * then reveals the static sprite as a fallback. Called when animation
 * metadata is unavailable or the enemy is dead.
 */
void GameScene::hideEnemyAnimationAndShowStatic() {
    // Hide all animation sprites
    if (_currentVisibleAnimationSprite) {
        _currentVisibleAnimationSprite->setVisible(false);
        _currentVisibleAnimationSprite = nullptr;
    }
    if (_bossSprite) {
        _bossSprite->setVisible(false);
    }
    
    // Show static sprite as fallback
    if (_gameArea) {
        auto staticSprite = _gameArea->getChildByName("bossIdle");
        if (staticSprite) {
            staticSprite->setVisible(true);
        }
    }
}

/**
 * Initializes the enemy animation sprite node with the given animation metadata.
 * 
 * Allocates texture from disk, creates a SpriteNode with the correct layout,
 * configures scale/anchor/position based on viewport size, and adds it to the
 * scene hierarchy. Called once when animation metadata first becomes available.
 *
 * @param animationEntry  The animation metadata containing texture path and frame info
 * @return true if sprite node was successfully initialized, false on error
 */
bool GameScene::initializeAllEnemyAnimations() {
    // Ensure we have the animation container
    if (!_bossSprite) {
        CULogError("Boss animation space not found");
        return false;
    }
    
    // Create sprite node for each animation in the registry
    for (const auto& [animationId, animationEntry] : _animationRegistry) {
        // Allocate texture from file
        auto texture = cugl::graphics::Texture::allocWithFile(animationEntry.texture);
        if (!texture) {
            CULogError("Failed to allocate texture: %s", animationEntry.texture.c_str());
            return false;
        }
        
        // Create SpriteNode with layout: frameRows rows × frameCount columns
        // The total frame count is frameCount * frameRows (all directions × frames per direction)
        auto spriteNode = cugl::scene2::SpriteNode::allocWithSheet(
            texture,
            animationEntry.frameRows,                                    // rows (4 directions)
            animationEntry.frameCount,                                   // columns (frames per direction)
            animationEntry.frameCount * animationEntry.frameRows         // total frames
        );
        
        if (!spriteNode) {
            CULogError("Failed to allocate SpriteNode for animation: %s", animationId.c_str());
            return false;
        }
        
        // Configure scale based on viewport size
        // Smaller screens (phones) get lower scale ~0.90, larger screens (iPad) get higher scale ~0.93
        cugl::Size viewportSize = getSize();
        float scale = 0.90f + (viewportSize.height - 800.0f) * 0.00005f;
        scale = std::clamp(scale, 0.80f, 0.95f);
        spriteNode->setScale(scale);
        
        // Calculate single frame dimensions from texture and grid layout
        float frameWidth = texture->getWidth() / animationEntry.frameCount;
        float frameHeight = texture->getHeight() / animationEntry.frameRows;
        spriteNode->setContentSize(cugl::Size(frameWidth, frameHeight));
        
        // Configure anchor and position (lower center of screen)
        spriteNode->setAnchor(cugl::Vec2(0.5f, 0.5f));
        spriteNode->setPosition(cugl::Vec2(196.5f, 120.0f));
        spriteNode->setVisible(false);  // All start invisible
        
        // Add to scene hierarchy
        _bossSprite->addChild(spriteNode);
        
        // Store in map for later access
        _enemyAnimationSpriteNodes[animationId] = spriteNode;
    }
    
    _bossSprite->setVisible(true);
    return true;
}

/**
 * Switches the visible animation sprite by hiding the current one and showing the new one.
 * 
 * Fast O(1) operation that just changes visibility and resets animation timing.
 * All sprites are pre-created, so this avoids runtime texture loading.
 * Applies animation-specific position and scale settings.
 *
 * @param animationId  The animation ID to make visible
 */
void GameScene::switchVisibleAnimation(const std::string& animationId) {
    // Find the sprite for this animation
    auto spriteNodeIter = _enemyAnimationSpriteNodes.find(animationId);
    if (spriteNodeIter == _enemyAnimationSpriteNodes.end()) {
        CULogError("Animation not found: %s", animationId.c_str());
        return;
    }
    
    auto newSprite = spriteNodeIter->second;
    if (!newSprite) {
        CULogError("Animation sprite is null: %s", animationId.c_str());
        return;
    }
    
    // Hide the previous animation sprite if there is one
    if (_currentVisibleAnimationSprite && _currentVisibleAnimationSprite != newSprite) {
        _currentVisibleAnimationSprite->setVisible(false);
    }
    
    // Show the new animation sprite
    newSprite->setVisible(true);
    _currentVisibleAnimationSprite = newSprite;
    
    // Apply animation-specific position and scale
    newSprite->setPosition(cugl::Vec2(_currentAnimationEntry.positionX + _currentAnimationEntry.offsetX,
                                      _currentAnimationEntry.positionY + _currentAnimationEntry.offsetY));
    newSprite->setScale(_currentAnimationEntry.scale);
    
    // Set initial frame (direction 0, frame 0)
    newSprite->setFrame(0);
    
    CULog("Switched to animation: %s", animationId.c_str());
}

/**
 * Updates the current animation frame for direction and elapsed time.
 * 
 * Animation behavior:
 * - Looping animations: frameInRow cycles continuously through all frames
 * - Buildup/Attack animations:
 *   * Buildup phase (stateTime < buildup_duration): loops frames 0 to (buildupFrameCount-1)
 *   * Attack phase (stateTime >= buildup_duration): plays frames buildupFrameCount to end once (clamped)
 *   * Damage triggers when frameInRow >= damageFrame (only once per state)
 * 
 * Direction is recalculated each frame from local player perspective.
 *
 * @param dt                The elapsed time in seconds since last frame
 * @param localPlayerIndex  The local player's index (0-3) for direction calculation
 */
/**
 * Ensures the frame index is within valid bounds.
 * Clamps negative frames to 0 and frames beyond frameCount to frameCount-1.
 *
 * @param frameInRow The frame index to validate
 * @return The clamped frame index
 */
int GameScene::validateFrameIndex(int frameInRow) const {
    if (frameInRow < 0) {
        return 0;
    }
    if (frameInRow >= _currentAnimationEntry.frameCount) {
        return _currentAnimationEntry.frameCount - 1;
    }
    return frameInRow;
}

/**
 * Calculates the frame index during the buildup phase of an animation.
 * Buildup frames loop until the buildup duration elapses.
 *
 * @param stateTime The time elapsed in the current state (seconds)
 * @param buildupDuration The total duration of the buildup phase (seconds)
 * @param buildupFrames Number of frames in the buildup phase
 * @return The looping frame index within the buildup frames
 */
int GameScene::calculateBuildupFrame(float stateTime, float buildupDuration, int buildupFrames) const {
    float frameFloat = stateTime / _currentAnimationEntry.frameDuration;
    return (int)(frameFloat) % buildupFrames;
}

/**
 * Calculates the frame index during the attack phase of an animation.
 * Attack frames play sequentially without looping, clamped to the final frame.
 *
 * @param stateTime The time elapsed in the current state (seconds)
 * @param buildupDuration The total duration of the buildup phase (seconds)
 * @param buildupFrames Number of frames in the buildup phase
 * @return The attack phase frame index (clamped to last attack frame)
 */
int GameScene::calculateAttackFrame(float stateTime, float buildupDuration, int buildupFrames) const {
    float timeSinceAttackStart = stateTime - buildupDuration;
    float frameFloat = timeSinceAttackStart / _currentAnimationEntry.frameDuration;
    int framesIntoAttack = (int)(frameFloat);
    int totalAttackFrames = _currentAnimationEntry.frameCount - buildupFrames;
    
    // Clamp to last attack frame (no looping)
    if (framesIntoAttack >= totalAttackFrames) {
        framesIntoAttack = totalAttackFrames - 1;
    }
    
    return buildupFrames + framesIntoAttack;
}

/**
 * Calculates which animation frame should be displayed based on state time and animation phase.
 * Handles both buildup/attack animations and simple looping animations.
 *
 * @param stateTime The time elapsed in the current state (seconds)
 * @return The frame index within the animation row (0-indexed)
 */
int GameScene::calculateAnimationFrame(float stateTime) const {
    int buildupFrames = _currentAnimationEntry.buildupFrameCount;
    
    // Ensure buildupFrameCount is valid
    if (buildupFrames < 0) buildupFrames = _currentAnimationEntry.frameCount;
    if (buildupFrames > _currentAnimationEntry.frameCount) buildupFrames = _currentAnimationEntry.frameCount;
    
    // Check if this animation has distinct buildup and attack phases
    if (buildupFrames < _currentAnimationEntry.frameCount) {
        // Buildup/Attack animation: buildup loops for buildUpTime, then attack plays through
        auto enemy = _gameState.getEnemy();
        const auto* stateDef = enemy ? enemy->getCurrentStateDef() : nullptr;
        float buildupDuration = stateDef ? stateDef->buildUpTime : (buildupFrames * _currentAnimationEntry.frameDuration);
        
        if (stateTime < buildupDuration) {
            return calculateBuildupFrame(stateTime, buildupDuration, buildupFrames);
        } else {
            return calculateAttackFrame(stateTime, buildupDuration, buildupFrames);
        }
    } else {
        // Simple looping animation - no attack phase, just loop all frames
        float frameFloat = stateTime / _currentAnimationEntry.frameDuration;
        return (int)(frameFloat) % _currentAnimationEntry.frameCount;
    }
}

/**
 * Updates the enemy animation frame based on the current state time and direction.
 * Calculates the correct frame to display for both looping and buildup/attack animations,
 * @param dt                The elapsed time in seconds since last frame
 * @param localPlayerIndex  The local player's index (0-3) for direction calculation
 * 
 */
void GameScene::updateEnemyAnimationFrame(float dt, int localPlayerIndex) {
    auto enemy = _gameState.getEnemy();
    if (!enemy || !_currentVisibleAnimationSprite) {
        return;
    }
    
    // Safety check: ensure animation entry has valid data
    if (_currentAnimationEntry.frameCount <= 0 || _currentAnimationEntry.frameDuration <= 0) {
        CULog("WARN: Invalid animation entry: frameCount=%d, frameDuration=%.3f", 
              _currentAnimationEntry.frameCount, _currentAnimationEntry.frameDuration);
        return;
    }
    
    // Show sprites and hide static fallback
    _currentVisibleAnimationSprite->setVisible(true);
    if (_bossSprite) {
        _bossSprite->setVisible(true);
    }
    if (_gameArea) {
        auto staticSprite = _gameArea->getChildByName("bossIdle");
        if (staticSprite) {
            staticSprite->setVisible(false);
        }
    }
    
    // Calculate direction the enemy should face
    int newDirection = EnemyController::calculateDirection(enemy->getTargetIndex(), localPlayerIndex);
    newDirection = (newDirection < 0 || newDirection > 3) ? 0 : newDirection;  // Safety clamp
    _enemyAnimationCurrentDirection = newDirection;
    
    // Calculate which animation frame should be displayed based on state time
    float stateTime = enemy->getStateTime();
    int frameInRow = calculateAnimationFrame(stateTime);
    frameInRow = validateFrameIndex(frameInRow);
    
    // Check if damage should trigger at this frame
    if (_currentAnimationEntry.damageFrame >= 0 && 
        frameInRow >= _currentAnimationEntry.damageFrame && 
        !_enemyAttackDamageDealtThisState) {
        _enemyAttackDamageDealtThisState = true;
    }
    
    // Calculate the linear frame index for the sprite sheet
    // Sheet layout: [direction_row][frameCount]
    int linearFrame = (_enemyAnimationCurrentDirection * _currentAnimationEntry.frameCount) + frameInRow;
    
    // Safety clamp linear frame to valid range
    int maxFrame = _currentAnimationEntry.frameCount * _currentAnimationEntry.frameRows - 1;
    linearFrame = (linearFrame > maxFrame) ? maxFrame : linearFrame;
    linearFrame = (linearFrame < 0) ? 0 : linearFrame;
    
    // Tell the enemy what frame is currently being displayed
    enemy->setCurrentAnimationFrame(frameInRow);
    
    // Set the frame on the sprite
    _currentVisibleAnimationSprite->setFrame(linearFrame);
}

/**
 * Checks if the current enemy attack animation has completed.
 * Returns true when an attack animation (with buildup phase) has finished playing
 * all attack frames (after buildup). Returns false for looping animations or
 * while buildup/attack is still in progress.
 * 
 * @return true if attack animation is complete, false otherwise
 */
bool GameScene::isEnemyAttackAnimationComplete() const {
    // Must have buildup < frameCount to be an attack animation
    if (_currentAnimationEntry.buildupFrameCount >= _currentAnimationEntry.frameCount) {
        return false;  // Not an attack animation (it's a looping animation)
    }
    
    auto enemy = _gameState.getEnemy();
    if (!enemy) return false;
    
    // Calculate total attack animation duration
    float buildupDuration = _currentAnimationEntry.buildupFrameCount * _currentAnimationEntry.frameDuration;
    int attackFrameCount = _currentAnimationEntry.frameCount - _currentAnimationEntry.buildupFrameCount;
    float attackDuration = attackFrameCount * _currentAnimationEntry.frameDuration;
    float totalDuration = buildupDuration + attackDuration;
    
    // Attack is complete if the enemy's state time has exceeded total animation duration
    return enemy->getStateTime() >= totalDuration;
}

/**
 * Updates enemy animation and directional facing based on target.
 * 
 * Each frame:
 *   1. Validates enemy exists
 *   2. Retrieves current state definition and animation metadata
 *   3. If metadata missing: hides animation sprite, shows static fallback
 *   4. If metadata exists:
 *      - Initializes sprite node on first frame (texture load, SpriteNode creation)
 *      - Updates sprite frame based on direction and elapsed time
 * 
 * Direction is computed locally per player from the enemy's target index,
 * so each player sees the correct enemy direction from their perspective.
 *
 * @param dt                Elapsed time in seconds for this frame
 * @param localPlayerIndex  The local player's index (0-3) for calculating relative direction
 */
void GameScene::updateEnemyAnimation(float dt, int localPlayerIndex) {
    auto enemy = _gameState.getEnemy();
    
    // If no enemy, hide animation
    if (!enemy) {
        hideEnemyAnimationAndShowStatic();
        return;
    }
    
    // Get current state definition
    const auto* stateDef = enemy->getCurrentStateDef();
    if (!stateDef || stateDef->animationKey.empty()) {
        hideEnemyAnimationAndShowStatic();
        return;
    }
    
    // Look up animation metadata
    auto animLookup = _animationRegistry.find(stateDef->animationKey);
    if (animLookup == _animationRegistry.end()) {
        CULog("WARN: No animation metadata found for state: %s", stateDef->animationKey.c_str());
        hideEnemyAnimationAndShowStatic();
        return;
    }
    
    // Update current animation entry before switching
    _currentAnimationEntry = animLookup->second;
    
    // Check if animation state has changed
    if (_currentAnimationId != stateDef->animationKey) {
        _currentAnimationId = stateDef->animationKey;
        _enemyAttackDamageDealtThisState = false;
        
        CULog("State animation changed to: %s", stateDef->animationKey.c_str());
        switchVisibleAnimation(stateDef->animationKey);
    }
    
    // Ensure sprite exists
    if (!_currentVisibleAnimationSprite) {
        CULogError("Animation sprite not initialized");
        hideEnemyAnimationAndShowStatic();
        return;
    }
    
    // Update animation frame for this frame
    updateEnemyAnimationFrame(dt, localPlayerIndex);
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
 * Updates the player and teammate UI icons to reflect their current health.
 */
void GameScene::updatePlayerAndTeammateIcons(float dt) {
    auto localPlayer = _gameState.getLocalPlayer();
    if (!localPlayer) return;

    // Given each player and their respective slot, set the texture depending on their health state.
    auto applyTexture = [&](auto slot, auto player) {
        if (!slot || !player) return;
        slot->setTexture(_assets->get<cugl::graphics::Texture>(
            getHealthTexture(
                getHealthState(player->getCurrentHealth(), player->getMaxHealth()),
                             player->getHouseName()
            ))
        );
        slot->setScale(0.5f);
    };

    applyTexture(_localPlayerSlot, localPlayer);
    applyTexture(_leftPlayerSlot,  localPlayer->getLeftPlayer());
    applyTexture(_rightPlayerSlot, localPlayer->getRightPlayer());
    updateTeammateBlink(_leftPlayerSlot, localPlayer->getLeftPlayer(),
                        _lastLeftPlayerHealth, _leftPlayerDamageBlinkTimer, _leftPlayerHealBlinkTimer, dt);
    updateTeammateBlink(_rightPlayerSlot, localPlayer->getRightPlayer(),
                        _lastRightPlayerHealth, _rightPlayerDamageBlinkTimer, _rightPlayerHealBlinkTimer, dt);
}

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
void GameScene::updateTeammateBlink(const std::shared_ptr<cugl::scene2::PolygonNode>& slot,
                                    Player* player,
                                    float& lastHealth,
                                    float& damageBlinkTimer,
                                    float& healBlinkTimer,
                                    float dt) {
    if (!slot || !player) return;

    const float currentHealth = player->getCurrentHealth();
    const bool hasPriorSnapshot = lastHealth >= 0.0f;
    const bool isAlive = player->isAlive();
    bool startedNewBlink = false;

    if (hasPriorSnapshot && isAlive) {
        if (currentHealth < lastHealth) {
            damageBlinkTimer = _blinkDuration;
            healBlinkTimer = 0.0f;
            startedNewBlink = true;
        } else if (currentHealth > lastHealth) {
            healBlinkTimer = _blinkDuration / 2;
            damageBlinkTimer = 0.0f;
            startedNewBlink = true;
        }
    }

    if (!startedNewBlink) {
        if (damageBlinkTimer > 0.0f) {
            damageBlinkTimer = std::max(0.0f, damageBlinkTimer - dt);
        }
        if (healBlinkTimer > 0.0f) {
            healBlinkTimer = std::max(0.0f, healBlinkTimer - dt);
        }
    }

    if (!isAlive) {
        damageBlinkTimer = 0.0f;
        healBlinkTimer = 0.0f;
        slot->setColor(Color4(255, 255, 255, 255));
    } else if (healBlinkTimer > 0.0f) {
        slot->setColor(Color4(176, 224, 176, 255));
    } else if (damageBlinkTimer > 0.0f && shouldShowDamageBlink(damageBlinkTimer, _blinkInterval)) {
        slot->setColor(Color4(224, 160, 160, 255));
    } else {
        slot->setColor(Color4(255, 255, 255, 255));
    }

    lastHealth = currentHealth;
}

/**
 * Resynchronises teammate blink state with the current local player.
 */
void GameScene::resetTeammateBlinkState() {
    _leftPlayerDamageBlinkTimer = 0.0f;
    _rightPlayerDamageBlinkTimer = 0.0f;
    _leftPlayerHealBlinkTimer = 0.0f;
    _rightPlayerHealBlinkTimer = 0.0f;
    _lastLeftPlayerHealth = -1.0f;
    _lastRightPlayerHealth = -1.0f;

    if (_leftPlayerSlot) {
        _leftPlayerSlot->setColor(Color4(255, 255, 255, 255));
    }
    if (_rightPlayerSlot) {
        _rightPlayerSlot->setColor(Color4(255, 255, 255, 255));
    }

    Player* localPlayer = _gameState.getLocalPlayer();
    if (!localPlayer) return;

    if (Player* leftPlayer = localPlayer->getLeftPlayer()) {
        _lastLeftPlayerHealth = leftPlayer->getCurrentHealth();
    }
    if (Player* rightPlayer = localPlayer->getRightPlayer()) {
        _lastRightPlayerHealth = rightPlayer->getCurrentHealth();
    }
}

/**
 * Checks whether the reset button was tapped and calls reset() if so.
 */
void GameScene::handleResetButton(InputController& input) {
    if (!isDebugMode()){ return; }
    if (!input.touchEnded() || _draggedIcon || !_resetBtn) return;

    Vec2 touchPosScreen = screenToWorldCoords(input.getTouchStart());
    if (_resetBtn->getBoundingBox().contains(touchPosScreen)) {
        CULog("Reset button tapped!");
        reset();
    }
}

/**
 * Initiates sliding for a released item by calculating velocity and starting animation.
 * Used when an item is dropped on an invalid zone or outside any zone.
 *
 * @param itemId  The ID of the item to start sliding
 */
void GameScene::slideReleasedItem(ItemInstance::ItemId itemId) {
    auto body = _itemBodies.find(itemId);
    if (body != _itemBodies.end() && body->second) {
        Vec2 currentPos = body->second->getPosition();
        Vec2 dropVelocity = (currentPos - _dragPreviousFrameItemBodyPos) / VELOCITY_DT_ESTIMATE;
        
        // Clamp drop velocity to maximum speed cap
        float speed = dropVelocity.length();
        if (speed > ITEM_MOVEMENT_MAX_SPEED) {
            dropVelocity = dropVelocity.normalize() * ITEM_MOVEMENT_MAX_SPEED;
        }
        
        startItemSliding(itemId, dropVelocity, ItemInstance::SlideOriginType::SLIDE_FROM_DROP);
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
    if (!_draggedIcon || !input.touchEnded()) return;

    // 1. Determine which drop zone the item was released into
    Vec2 releaseWorld = screenToWorldCoords(input.getReleasePosition());
    InputController::Action finalAction = InputController::Action::DROP_INVALID;

    for (const auto& pair : _inputZones) {
        if (pair.second.contains(releaseWorld)) {
            finalAction = pair.first;
            break;
        }
    }

    if (finalAction != InputController::Action::NONE) {
        if (handlePlayerActions(finalAction, _draggedItemId)) {
            // 2. Item was successfully used (action succeeded)
            // 3. Trigger glow effect on the activated zone
            _glowAction = finalAction;
            _glowTimer  = _glowDuration;
            if (_draggedIcon) {
                const bool isUseAction =
                    (finalAction == InputController::Action::DROP_BOSS ||
                     finalAction == InputController::Action::DROP_ALLY_LEFT ||
                     finalAction == InputController::Action::DROP_ALLY_RIGHT);
                if (isUseAction) {
                    auto consumedDef = _draggedItemDef ? _draggedItemDef : getHeldItemDef(_draggedItemId);
                    spawnConsumedItemAnimation(_draggedIcon, consumedDef);
                }
                _draggedIcon->setVisible(false);
            }
        } else {
            // Item action failed - slide the item back
            slideReleasedItem(_draggedItemId);
            if (_draggedIcon) {
                _draggedIcon->setVisible(true);
            }
            _audio->playSoundUnique("deselect");
        }
    } else {
        // If no zone was detected, slide the item
        slideReleasedItem(_draggedItemId);
        if (_draggedIcon) {
            _draggedIcon->setVisible(true);
        }
        _audio->playSoundUnique("deselect");

    }

    _draggedIcon = nullptr;
    if (_draggedItemId != 0) {
        _itemWidgetScaleTargets[_draggedItemId] = ITEM_NORMAL_SCALE;
    }
    _draggedItemId = 0;
    _dragStartBodyPosition = Vec2::ZERO;
    _draggedItemDef = nullptr;
    _dragPreviousFrameItemBodyPos = Vec2::ZERO;
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
    if (_draggedIcon || !input.isDragging()) return;

    Vec2 touchPosScreen = screenToWorldCoords(input.getTouchStart());

    for (auto& [id, widget] : _itemWidgets) {
        if (!widget) continue;
        if (widget->getBoundingBox().contains(touchPosScreen)) {

            _audio->playSoundUnique("select");

            _draggedIcon = widget;
            _draggedItemId = id;
            _itemWidgetScaleTargets[id] = ITEM_PICKUP_SCALE;
            _dragOffset = widget->getPosition() - touchPosScreen;

            // Bring item to front of render order when picked up
            if (_inventory) {
                _inventory->removeChild(widget);
                _inventory->addChild(widget);
            }

            auto body = _itemBodies.find(id);
            if (body != _itemBodies.end() && body->second) {
                _dragStartBodyPosition = body->second->getPosition();
            } else {
                Size widgetSize = widget->getContentSize();
                _dragStartBodyPosition = widget->getPosition() + Vec2(widgetSize.width * 0.5f, widgetSize.height * 0.5f);
            }

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
    if (!_draggedIcon || (!input.isTouching() && !input.isMouseDown())) return;

    Vec2 dragScene = screenToWorldCoords(input.getDragPos());
    Vec2 widgetPosition = dragScene + _dragOffset;
    auto body = _itemBodies.find(_draggedItemId);
    if (body != _itemBodies.end() && body->second) {
        _dragPreviousFrameItemBodyPos = body->second->getPosition(); // Store current position for velocity calculation
        Size widgetSize = _draggedIcon->getContentSize();
        Vec2 center = widgetPosition + Vec2(widgetSize.width * 0.5f, widgetSize.height * 0.5f);
        body->second->setPosition(center);
        body->second->setLinearVelocity(Vec2::ZERO);
    }
}

/**
 * Checks if any updates about the state of the game were sent over the network.
 * If we are a client, we update the state of the game to match the hosts' version and process any passes sent to us.
 * If we are the host, we process any attack, heal, effect, and pass messages, then tick timed player effects.
 * After doing so, we send out a new authoritative version of the game state as the host.
 *
 * @param dt  The elapsed time since the previous frame, in seconds.
 */
void GameScene::handleNetworkUpdates(float dt) {
    /*Networking pull cycle*/
    _network->getNetworkUpdates();
    
    // If we are a client and the host dropped, kick back to the setup flow
    // before any game logic runs this frame. LobbyScene does the same check.
    if (_network->wasHostDisconnected() && !_network->isHost()) {
        _network->disconnect();
        _status = Status::HOST_DISCONNECTED;
        return;
    }

    // Track player and enemy health before updates to detect changes
    auto player = _gameState.getLocalPlayer();
    float playerHealthBefore = player ? player->getCurrentHealth() : 0.0f;
    float enemyHealthBefore = _gameState.getEnemy()->getCurrentHealth();

    if (_network->isHost()) {
        // handle incoming attack/heal messages from clients
        _gameState.attackUpdates(_network->getAttackUpdates());
        _gameState.healUpdates(_network->getHealUpdates());
        _gameState.supportEffectUpdates(_network->getSupportEffectUpdates());
        _gameState.enemyEffectUpdates(_network->getEnemyEffectUpdates());

        for (auto& player : _gameState.getPlayers()) {
            if (player) {
                player->updateEffects(dt);
            }
        }

        // broadcast authoritative state to all clients
        _network->broadcastGameState(_gameState);
    }
    else {
        // clients just apply the latest state from host
        _gameState.networkUpdate(_network->getStateUpdate());
        refreshTeammateNameLabels();
    }
    
    // Play sounds for LOCAL player and enemy health changes after all updates
    playHealthAndDamageSounds(playerHealthBefore, enemyHealthBefore);
    
    // Check if we won or lost (common to both host and client)
    if (_network->isHost()) {
        if (_gameState.didWin()) {
            _network->broadcastWonGame();
            _status = Status::WON;
            CULog("We won!");
        } else if (_gameState.didLose()) {
            _network->broadcastLostGame();
            _status = Status::LOST;
            CULog("We lost!");
        }
    } else {
        if (_network->checkGameWon()) {
            _status = Status::WON;
            CULog("We won!");
        } else if (_network->checkGameLost()) {
            _status = Status::LOST;
            CULog("We lost!");
        }
    }

    processNetworkedPasses(_network->getPassUpdates());
}

/** 
 * Plays appropriate hurt/heal sounds based on changes in player and enemy health.
 * Should be called after processing all enemy and AI updates, so we capture all 
 * health changes in one place and avoid playing multiple overlapping sounds for the same health change.
 */
void GameScene::playHealthAndDamageSounds(float playerHealthBefore, float enemyHealthBefore) {
    auto player = _gameState.getLocalPlayer();
    auto enemy = _gameState.getEnemy();
    
    // Only play sounds for non-AI local players
    if (player && !dynamic_cast<PlayerAI*>(player)) {
        if (player->getCurrentHealth() < playerHealthBefore && _audio) {
            std::string soundKey = player->isFemaleHouse() ? "player_hurt" : "player_hurt_deep";
            _audio->playSoundUnique(soundKey);
        } else if (player->getCurrentHealth() > playerHealthBefore && _audio) {
            _audio->playSoundUnique("player_heal");
        }
    }
    
    if (enemy->getCurrentHealth() < enemyHealthBefore && _audio) {
        _audio->playSoundUnique("enemy_hurt");
    }
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

#pragma mark Sliding Items Physics

/**
 * Initializes a sliding item with the given velocity and origin type.
 * Marks the item as sliding and configures its state based on origin.
 *
 * @param itemId        The ID of the item to start sliding
 * @param velocity      Initial velocity vector (units/sec)
 * @param origin        The SlideOriginType indicating where the slide came from
 */
void GameScene::startItemSliding(ItemInstance::ItemId itemId, const cugl::Vec2& velocity, ItemInstance::SlideOriginType origin) {
    auto player = _gameState.getLocalPlayer();
    if (!player) return;

    // Find the item in the player's inventory
    auto& inventory = const_cast<std::vector<ItemInstance>&>(player->getInventory());
    ItemInstance* item = nullptr;
    for (auto& invItem : inventory) {
        if (invItem.getId() == itemId) {
            item = &invItem;
            break;
        }
    }
    
    if (!item) return;

    // Configure sliding state based on origin
    item->setSliding(true);
    item->setSlideVelocity(velocity);
    item->setSlideOrigin(origin);

    // Set zone-interaction capability based on origin
    switch (origin) {
        case ItemInstance::SlideOriginType::SLIDE_FROM_DROP:
            // Dropped items can interact with zones immediately
            item->setCanInteractWithZones(true);
            break;
        case ItemInstance::SlideOriginType::SLIDE_FROM_SPAWN:
            // Spawned items cannot interact with zones until settled
            item->setCanInteractWithZones(false);
            if (_audio) {
                _audio->playSoundUnique("whoosh");
            }
            break;
        case ItemInstance::SlideOriginType::SLIDE_FROM_PASS:
            // Passed items cannot interact with zones until settled
            item->setCanInteractWithZones(false);
            if (_audio) {
                _audio->playSoundUnique("whoosh");
            }
            break;
    }

    _slidingItems.insert(itemId);
}

#pragma mark -
#pragma mark Sliding Items Physics

/**
 * Updates friction deceleration for a sliding item and its body position.
 * Called each frame to slow down items based on ITEM_SLIDE_FRICTION_DECELERATION.
 *
 * @param item       The item instance to update.
 * @param itemBody   The Box2D body representing the item.
 * @param dt         Delta time in seconds.
 * @return           true if the item is still sliding (speed > threshold), false if settled.
 */
bool GameScene::updateItemFriction(ItemInstance* item, std::shared_ptr<cugl::physics2::BoxObstacle> itemBody, float dt) {
    cugl::Vec2 velocity = item->getSlideVelocity();
    float speed = velocity.length();
    
    if (speed > ITEM_SLIDE_VELOCITY_SETTLE_THRESHOLD) {
        // Apply friction deceleration: reduce speed by deceleration * dt
        // Clamp to prevent reversing direction
        float newSpeed = std::max(0.0f, speed - ITEM_SLIDE_FRICTION_DECELERATION * dt);
        if (newSpeed > 0.0f) {
            velocity = velocity.normalize() * newSpeed;
        } else {
            velocity = cugl::Vec2::ZERO;
        }
        item->setSlideVelocity(velocity);

        // Update body position
        cugl::Vec2 newPos = itemBody->getPosition() + velocity * dt;
        itemBody->setPosition(newPos);

        // Only clamp natural spawned items to inventory bounds during slide
        // Passed items should animate in from outside without clamping
        if (item->getSlideOrigin() == ItemInstance::SlideOriginType::SLIDE_FROM_SPAWN) {
            clampItemToBounds(itemBody);
        }
        
        return true; // Still sliding
    }
    
    return false; // Settled
}

/**
 * Checks if an item is in a matching interaction zone.
 * Iterates through all input zones and checks if the item position falls within
 * a zone and if its type matches the zone's expected type (Attack↔DROP_BOSS, Support↔DROP_ALLY_*).
 *
 * @param itemPos  The item's current world position
 * @param itemDef  The item definition containing type information
 * @return         true if the item is in a valid matching zone, false otherwise
 */
bool GameScene::isItemInMatchingZone(const cugl::Vec2& itemPos, const std::shared_ptr<ItemDef>& itemDef) {
    if (!itemDef) return false;
    
    for (auto& [action, zone] : _inputZones) {
        if (!zone.contains(itemPos)) continue;
        
        // Check for type matching
        if (action == InputController::Action::DROP_BOSS && itemDef->getType() == ItemDef::Type::Attack) {
            return true;
        }
        if ((action == InputController::Action::DROP_ALLY_LEFT || action == InputController::Action::DROP_ALLY_RIGHT) 
            && itemDef->getType() == ItemDef::Type::Support) {
            return true;
        }
    }
    return false;
}

/**
 * Initiates a snapback animation for an item returned to inventory.
 * Retrieves the item's widget (or uses default size), calculates a random
 * target position in the inventory, and creates a snapback animation entry.
 *
 * @param itemId   The ID of the item to snapback
 * @param fromPos  The item's current world position (animation start point)
 */
void GameScene::initiateSnapbackAnimation(ItemInstance::ItemId itemId, const cugl::Vec2& fromPos) {
    auto widget = _itemWidgets[itemId];
    cugl::Size widgetSize = widget ? widget->getContentSize() : cugl::Size(50, 50);
    cugl::Vec2 randomTarget = getRandomInventoryPosition(widgetSize);
    
    SnapbackAnimation anim;
    anim.startPos = fromPos;
    anim.targetPos = randomTarget;
    anim.progress = 0.0f;
    _snapbackAnimations[itemId] = anim;
}

/**
 * Handles settlement logic for dropped items.
 * Checks if the item is within inventory bounds, then in matching interaction zones,
 * and finally initiates snapback if neither condition is met.
 *
 * @param item       The item instance that has settled.
 * @param itemBody   The Box2D body representing the item.
 * @param itemId     The ID of the item.
 * @return           true if the item should be removed from sliding set, false if animating/processing.
 */
bool GameScene::handleSettledItemDrop(ItemInstance* item, std::shared_ptr<cugl::physics2::BoxObstacle> itemBody, ItemInstance::ItemId itemId) {
    // Check if item is within inventory bounds
    bool inInventoryBounds = false;
    if (_inventory) {
        inInventoryBounds = _inventory->getBoundingBox().contains(itemBody->getPosition());
    }
    
    if (inInventoryBounds) {
        // In bounds, just settle
        item->setSliding(false);
        return true;
    }
    
    // Out of bounds - check if it's in a matching interaction zone
    cugl::Vec2 itemPos = itemBody->getPosition();
    auto itemDef = _itemController.getDatabase().getDef(item->getDefId());
    
    if (isItemInMatchingZone(itemPos, itemDef)) {
        // Item is in a matching zone; enable zone interaction and let it be processed next frame
        item->setCanInteractWithZones(true);
        item->setSliding(false);
        return false; // Keep in sliding set to be processed by zone interaction logic
    }
    
    // Not in any valid zone and outside inventory - snapback to inventory
    item->setCanInteractWithZones(false); // Prevent zone interactions during snapback
    item->setSliding(false);
    initiateSnapbackAnimation(itemId, itemPos);
    return false; // Don't remove yet; snapback animation will handle it
}

/**
 * Handles settlement logic for spawned items.
 * Enables zone interactions once the item has settled from its spawn/pass.
 *
 * @param item   The spawned item that has settled.
 * @param itemId The ID of the item.
 * @return       true (always removed from sliding set after settlement).
 */
bool GameScene::handleSpawnedItemSettled(ItemInstance* item, ItemInstance::ItemId itemId) {
    // Spawned/passed item settled, now zone-interactive
    item->setCanInteractWithZones(true);
    item->setSliding(false);
    return true; // Always remove from sliding set
}

/**
 * Dispatches settlement handling based on item origin type.
 * Returns whether the item should be removed from the sliding set.
 *
 * @param item     The settled item to handle.
 * @param itemBody The Box2D body representing the item.
 * @param itemId   The ID of the item.
 * @return         true if the item should be removed from sliding set, false if still animating (snapback).
 */
bool GameScene::handleSettledItem(ItemInstance* item, std::shared_ptr<cugl::physics2::BoxObstacle> itemBody, ItemInstance::ItemId itemId) {
    switch (item->getSlideOrigin()) {
        case ItemInstance::SlideOriginType::SLIDE_FROM_DROP:
            return handleSettledItemDrop(item, itemBody, itemId);
        case ItemInstance::SlideOriginType::SLIDE_FROM_SPAWN:
            return handleSpawnedItemSettled(item, itemId);
        case ItemInstance::SlideOriginType::SLIDE_FROM_PASS:
            return handleSpawnedItemSettled(item, itemId);
    }
    return true; // Default: remove from sliding set
}

/**
 * Checks if a settled item should be removed due to being off-screen.
 * Only applies to spawned and passed items; dropped items are exempted.
 *
 * @param item     The item instance to check.
 * @param itemBody The Box2D body representing the item.
 * @return         true if the item is off-screen and should be removed.
 */
bool GameScene::shouldRemoveOffscreenItem(ItemInstance* item, std::shared_ptr<cugl::physics2::BoxObstacle> itemBody) {
    // Only check off-screen for spawn/pass items
    if (item->getSlideOrigin() == ItemInstance::SlideOriginType::SLIDE_FROM_DROP) {
        return false; // Dropped items never removed for being off-screen
    }
    
    // Only check after item has settled
    if (item->isSliding()) {
        return false;
    }
    
    // Check if position is off-screen
    return !isItemInVisibleArea(itemBody->getPosition());
}

/**
 * Updates all sliding items each frame, applying friction and checking boundaries.
 * Handles settlement and snapback animations for dropped items.
 *
 * @param dt  Delta time in seconds.
 */
void GameScene::updateSlidingItems(float dt) {
    auto player = _gameState.getLocalPlayer();
    if (!player) return;

    auto& inventory = const_cast<std::vector<ItemInstance>&>(player->getInventory());
    std::vector<ItemInstance::ItemId> itemsToRemove;

    for (auto itemId : _slidingItems) {
        // Find the item in inventory
        ItemInstance* item = nullptr;
        for (auto& invItem : inventory) {
            if (invItem.getId() == itemId) {
                item = &invItem;
                break;
            }
        }
        
        if (!item || !item->isSliding()) {
            itemsToRemove.push_back(itemId);
            continue;
        }

        auto itemBody = _itemBodies[itemId];
        if (!itemBody) {
            itemsToRemove.push_back(itemId);
            continue;
        }

        // Update friction and check if still sliding
        bool stillSliding = updateItemFriction(item, itemBody, dt);
        
        if (!stillSliding) {
            // Item has settled; handle based on origin type
            bool shouldRemove = handleSettledItem(item, itemBody, itemId);
            if (shouldRemove) {
                itemsToRemove.push_back(itemId);
            }
        }

        // Check for off-screen removal
        if (shouldRemoveOffscreenItem(item, itemBody)) {
            itemsToRemove.push_back(itemId);
        }
    }

    // Clean up settled or off-screen items
    for (auto itemId : itemsToRemove) {
        _slidingItems.erase(itemId);
    }
}

/**
 * Updates snapback animations for dropped items returning to inventory.
 * Smoothly interpolates item positions back to their original inventory locations.
 * Supports multiple simultaneous snapbacks.
 *
 * @param dt  Delta time in seconds.
 */
void GameScene::updateSnapbackAnimations(float dt) {
    std::vector<ItemInstance::ItemId> completedAnimations;

    for (auto& [itemId, anim] : _snapbackAnimations) {
        anim.progress += dt / ITEM_SLIDE_SNAPBACK_ANIMATION_TIME;

        if (anim.progress >= 1.0f) {
            // Animation complete
            auto itemBody = _itemBodies[itemId];
            if (itemBody) {
                itemBody->setPosition(anim.targetPos);
            }

            // Find and update the item from player inventory
            auto player = _gameState.getLocalPlayer();
            if (player) {
                auto& inventory = const_cast<std::vector<ItemInstance>&>(player->getInventory());
                for (auto& invItem : inventory) {
                    if (invItem.getId() == itemId) {
                        invItem.setSliding(false);
                        break;
                    }
                }
            }

            _slidingItems.erase(itemId);
            completedAnimations.push_back(itemId);
        } else {
            // Interpolate position
            auto itemBody = _itemBodies[itemId];
            if (itemBody) {
                // Use cubic-out easing for smooth animation
                float progress = anim.progress;
                float eased = 1.0f - (1.0f - progress) * (1.0f - progress) * (1.0f - progress);
                
                cugl::Vec2 pos = anim.startPos + (anim.targetPos - anim.startPos) * eased;
                itemBody->setPosition(pos);
            }
        }
    }

    // Remove completed animations
    for (auto itemId : completedAnimations) {
        _snapbackAnimations.erase(itemId);
    }
}

/**
 * Processes zone interactions for zone-interactive sliding items.
 * When an item is in a matching zone, triggers the appropriate action and marks the item as used.
 * Called once per frame after sliding velocity updates.
 */
void GameScene::processZoneInteractionsForSlidingItems() {
    auto player = _gameState.getLocalPlayer();
    if (!player) return;

    auto& inventory = const_cast<std::vector<ItemInstance>&>(player->getInventory());
    std::unordered_set<ItemInstance::ItemId> itemsToRemove;

    for (auto itemId : _slidingItems) {
        // Find the item in inventory
        ItemInstance* item = nullptr;
        for (auto& invItem : inventory) {
            if (invItem.getId() == itemId) {
                item = &invItem;
                break;
            }
        }
        
        // Skip if: no item, can't interact, or is a passed item
        if (!item || !item->canInteractWithZones() || item->getSlideOrigin() == ItemInstance::SlideOriginType::SLIDE_FROM_PASS) {
            continue;
        }

        auto itemBody = _itemBodies[itemId];
        if (!itemBody) continue;

        cugl::Vec2 itemPos = itemBody->getPosition();
        auto itemDef = _itemController.getDatabase().getDef(item->getDefId());
        if (!itemDef) continue;

        // Check against all zones for a match
        for (auto& [action, zone] : _inputZones) {
            if (!zone.contains(itemPos)) continue;
            
            // Verify type matching (attack items → boss zones, support items → ally zones)
            if (!isItemActionMatch(action, itemDef->getType())) {
                continue;
            }
            
            // First time hitting a matching zone - trigger the action immediately
            if (handlePlayerActions(action, itemId)) {
                const bool isUseAction =
                    (action == InputController::Action::DROP_BOSS ||
                        action == InputController::Action::DROP_ALLY_LEFT ||
                        action == InputController::Action::DROP_ALLY_RIGHT);
                if (isUseAction) {
                    auto widgetIt = _itemWidgets.find(itemId);
                    if (widgetIt != _itemWidgets.end() && widgetIt->second) {
                        spawnConsumedItemAnimation(widgetIt->second, itemDef);
                        widgetIt->second->setVisible(false);
                    }
                }
                markItemAsUsed(itemId);
                itemsToRemove.insert(itemId);
            }
            break;
        }
    }
    // Remove items after iteration completes to avoid iterator invalidation
    for (auto itemId : itemsToRemove) {
        _slidingItems.erase(itemId);
    }
}

/**
 * Checks if an item type matches an action zone type.
 *
 * @param action  The zone action type
 * @param itemType The type of item
 * @return true if the item can be used in this zone
 */
bool GameScene::isItemActionMatch(InputController::Action action, ItemDef::Type itemType) const {
    switch (action) {
        case InputController::Action::DROP_BOSS:
            return itemType == ItemDef::Type::Attack;
        case InputController::Action::DROP_ALLY_LEFT:
        case InputController::Action::DROP_ALLY_RIGHT:
            return itemType == ItemDef::Type::Support;
        case InputController::Action::PASS_LEFT:
        case InputController::Action::PASS_RIGHT:
            return true;  // Pass zones accept any item type
        default:
            return false;
    }
}

/**
 * Clamps a passed item's position to the inventory zone bounds.
 * Prevents passed items from sliding outside the valid inventory area.
 *
 * @param itemBody      The Box2D body to clamp
 */
void GameScene::clampItemToBounds(std::shared_ptr<cugl::physics2::BoxObstacle> itemBody) {
    if (!itemBody || !_inventory) return;

    // Get inventory bounds
    cugl::Rect inventoryBounds = _inventory->getBoundingBox();

    // Clamp item position to inventory bounds
    cugl::Vec2 pos = itemBody->getPosition();
    pos.x = std::max(inventoryBounds.getMinX(), std::min(inventoryBounds.getMaxX(), pos.x));
    pos.y = std::max(inventoryBounds.getMinY(), std::min(inventoryBounds.getMaxY(), pos.y));
    itemBody->setPosition(pos);
}

/**
 * Checks if an item's position is within visible screen bounds.
 *
 * @param position      The screen position to check
 * @return true if position is within visible area, false otherwise
 */
bool GameScene::isItemInVisibleArea(const cugl::Vec2& position) {
    cugl::Size screenSize = getSize();
    cugl::Rect screenBounds(0.0f, 0.0f, screenSize.width, screenSize.height);
    return screenBounds.contains(position);
}

/**
 * Updates the visibility of all drop zones based on the current interaction.
 *
 * This function evaluates which drop zones should be visible at the current moment
 * (e.g., during drag-and-drop interactions or based on item/type compatibility)
 * and toggles their visibility accordingly.
 */
void GameScene::updateDropZoneVisibility(){
    if (_draggedItemId != 0) {
        
        // Render attack/support zones based on item type
        auto itemDef = getHeldItemDef(_draggedItemId);
        
        if (itemDef) {
            if (itemDef->getType() == ItemDef::Type::Attack) {
                // Render attack zones when holding attack item
                _attackArea->setVisible(true);
            } else {
                // Render support zones when holding heal/support item
                _supportLeftArea->setVisible(true);
                _supportRightArea->setVisible(true);
            }
        }
    } else {
        _attackArea->setVisible(false);
        _supportLeftArea->setVisible(false);
        _supportRightArea->setVisible(false);
    }
}

#pragma mark -
#pragma mark Update

/**
 * Processes one frame of game logic.
 */
void GameScene::update(float dt, InputController& input) {
    if (!_active) return;
    
    if (_network->isHost()) {
        _network->broadcastHostsCurrentScene(1);
    }

    handleResetButton(input);
    handlePlayerInput(input);
    input.resetAction();

    if (input.touchEnded()) {
        input.resetAction();
    }

    handleNetworkUpdates(dt);
    handleDisconnectedPlayers();

    handleItemSpawn(dt);
    updateEnemyAnimation(dt, _network->getLocalPlayerNumber());
    updateEnemyAndAI(dt);
    updateDropZoneVisibility();

    // Update sliding items before physics world update
    updateSlidingItems(dt);
    updateSnapbackAnimations(dt);
    updateItemUseAnimations(dt);
    updatePopupAnimations(dt);

    tickGlowTimer(dt);
    updateDebugPointer(input);
    handleDragInitiation(input);
    handleDragTracking(input);

    if (_itemPhysicsWorld) {
        _itemPhysicsWorld->update(dt);
    }
    
    processZoneInteractionsForSlidingItems();
    
    syncInventoryWidgets();
    updateItemWidgetScales(dt);
    updateConsumedItemAnimations(dt);
    syncItemWidgetsToBodies();

    _network->clearQueues();
    updatePlayerAndEnemyHealthUI(dt);
    updatePlayerAndTeammateIcons(dt);
}

#pragma mark -
#pragma mark Inventory UI

/** Creates a scene-node widget for the given item and adds it to the inventory container. */
std::shared_ptr<SceneNode> GameScene::createItemWidget(const ItemInstance& item) {
    auto itemDef = _itemController.getDatabase().getDef(item.getDefId());
    if (!itemDef) return nullptr;
    
    const std::string textureKey = itemDef->getIconKey();

    auto texture = _assets->get<cugl::graphics::Texture>(textureKey);
    if (!texture) return nullptr;

    auto widget = PolygonNode::allocWithTexture(texture);
    widget->setContentSize(Size(100, 100));
    widget->setAnchor(Vec2::ANCHOR_BOTTOM_LEFT);
    widget->setScale(ITEM_NORMAL_SCALE);
    widget->setName("item_" + std::to_string((unsigned long long)item.getId()));
    _inventory->addChild(widget);
    return widget;
}

/** Return a random in-bounds inventory position for a newly spawned item widget */
cugl::Vec2 GameScene::getRandomInventoryPosition(const cugl::Size& widgetSize) const {
    const cugl::Size inventorySize = _inventory->getContentSize();
    Size dimen = getSize();
    float w = dimen.width;

    // Constrain horizontally with inward margin from edges (not at the very edges)
    const float horizontalMargin = 50.0f;  // Distance from each side
    const float minX = w * 0.15f + horizontalMargin;
    const float maxX = minX + std::max(0.0f, inventorySize.width - widgetSize.width - (2.0f * horizontalMargin));

    // Constrain vertically, avoiding the bottom part of inventory (top 70% only)
    const float minY = inventorySize.height * 0.3f;  // Avoid bottom 30%
    const float maxY = std::max(minY, inventorySize.height - widgetSize.height - 20.0f);

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<float> xDist(minX, std::max(minX, maxX));
    std::uniform_real_distribution<float> yDist(minY, std::max(minY, maxY));

    return cugl::Vec2(xDist(rng), yDist(rng));
}

/**
 * Returns a spawn position for a passed item based on which side it came from.
 * Items spawn at the side edge horizontally (at pass zone height).
 *
 * @param passDirection  0 for none, 1 for passed from left, 2 for passed from right
 * @return               The spawn position in world coordinates
 */
cugl::Vec2 GameScene::getPassSpawnPosition(int passDirection) const {
    cugl::Size screenSize = getSize();
    float x = screenSize.width * 0.5f;  // Default to center
    float y = screenSize.height * 0.2f;  // Spawn at middle pass zone height
    
    if (passDirection == 1) {
        // Passed from left (sender on left) - receiver sees it from their right
        x = screenSize.width * 1.1f;
    } else if (passDirection == 2) {
        // Passed from right (sender on right) - receiver sees it from their left
        x = -screenSize.width * 0.1f;
    }
    
    return cugl::Vec2(x, y);  // At side edge, pass zone height
}

/** Creates and registers the Box2D body for an item widget.
 *
 * @param itemId  The ItemInstance for which the item body is created.
 * @param widget  The widget to attach the physics body to.
 */
std::shared_ptr<cugl::physics2::BoxObstacle> GameScene::createItemBody(
    ItemInstance::ItemId itemId,
    const std::shared_ptr<SceneNode>& widget) {
    if (!_itemPhysicsWorld || !widget) {
        return nullptr;
    }

    Size widgetSize = widget->getContentSize();
    Vec2 center = widget->getPosition() + Vec2(widgetSize.width * 0.5f, widgetSize.height * 0.5f);
    auto body = cugl::physics2::BoxObstacle::alloc(center, widgetSize);
    if (!body) {
        CULogError("Failed to create item body for %llu", (unsigned long long)itemId);
        return nullptr;
    }

    body->setName("item_body_" + std::to_string((unsigned long long)itemId));
    body->setPhysicsUnits(ITEM_SPEED_UNITS);
    body->setBodyType(b2_kinematicBody);
    body->setSensor(true);
    body->setLinearVelocity(Vec2::ZERO);
    _itemPhysicsWorld->addObstacle(body);
    _itemBodies[itemId] = body;
    return body;
}

/** Updates all inventory widgets so they exactly match their body positions. */
void GameScene::syncItemWidgetsToBodies() {
    std::vector<ItemInstance::ItemId> staleIds;

    for (auto& [itemId, body] : _itemBodies) {
        auto widget = _itemWidgets.find(itemId);
        if (!body || widget == _itemWidgets.end() || !widget->second) {
            staleIds.push_back(itemId);
            continue;
        }

        Size widgetSize = widget->second->getContentSize();
        Vec2 bodyPosition = body->getPosition();
        Vec2 widgetPosition = bodyPosition - Vec2(widgetSize.width * 0.5f, widgetSize.height * 0.5f);
        widget->second->setPosition(widgetPosition);
    }

    for (ItemInstance::ItemId itemId : staleIds) {
        removeItemWidget(itemId);
    }
}

/** Removes the widget and its Box2D body for the given item.
 *
 * @param itemId  The itemId representing the ItemInstance to be removed.
 */
void GameScene::removeItemWidget(ItemInstance::ItemId itemId) {
    auto widget = _itemWidgets.find(itemId);
    if (widget != _itemWidgets.end()) {
        if (_draggedIcon == widget->second) {
            _draggedIcon = nullptr;
            if (_draggedItemId != 0) {
                _itemWidgetScaleTargets[_draggedItemId] = ITEM_NORMAL_SCALE;
            }
            _draggedItemId = 0;
            _dragStartBodyPosition = Vec2::ZERO;
        }
        if (widget->second && _inventory) {
            _inventory->removeChild(widget->second);
        }
        _itemWidgets.erase(widget);
    }

    auto body = _itemBodies.find(itemId);
    if (body != _itemBodies.end()) {
        if (body->second && _itemPhysicsWorld) {
            b2World* world = _itemPhysicsWorld->getWorld();
            if (world && body->second->getBody()) {
                body->second->deactivatePhysics(*world);
            }
            _itemPhysicsWorld->removeObstacle(body->second);
        }
        _itemBodies.erase(body);
    }

    _itemWidgetScales.erase(itemId);
    _itemWidgetScaleTargets.erase(itemId);
    
    // Clean up pass tracking to prevent memory leak
    _passedItemIds.erase(itemId);
}

/**
 * Smoothly interpolates each item widget's scale toward its target scale.
 * Uses a frame-rate independent lerp factor based on the elapsed timestep
 * and ITEM_SCALE_SPEED. Snaps to the target if within a small epsilon to
 * avoid floating point drift. Initializes any widget with no tracked scale
 * to ITEM_NORMAL_SCALE.
 *
 * @param dt  The time elapsed since the last update, in seconds.
 */
void GameScene::updateItemWidgetScales(float dt) {
    if (_itemWidgets.empty()) return;

    const float lerpFactor = std::min(1.0f, dt * ITEM_SCALE_SPEED);
    for (const auto& [itemId, widget] : _itemWidgets) {
        if (!widget) continue;

        auto current = _itemWidgetScales.find(itemId);
        if (current == _itemWidgetScales.end()) {
            _itemWidgetScales[itemId] = ITEM_NORMAL_SCALE;
            current = _itemWidgetScales.find(itemId);
        }

        float targetScale = ITEM_NORMAL_SCALE;
        auto target = _itemWidgetScaleTargets.find(itemId);
        if (target != _itemWidgetScaleTargets.end()) {
            targetScale = target->second;
        }

        float newScale = current->second + (targetScale - current->second) * lerpFactor;
        if (std::abs(targetScale - newScale) < 0.001f) {
            newScale = targetScale;
        }

        current->second = newScale;
        widget->setScale(newScale);
    }
}

/*
 * Marks an item as used (consumed by an action).
 * Removes the visual widget and physics body from the scene.
 * Item remains in inventory until deferred damage is applied and animation completes.
 * Prevents the item from being respawned during animation playback.
 *
 * @param itemId  The ID of the item that was used
 */
void GameScene::markItemAsUsed(ItemInstance::ItemId itemId) {
    // Remove widget from inventory display
    auto widget = _itemWidgets.find(itemId);
    if (widget != _itemWidgets.end() && widget->second && _inventory) {
        _inventory->removeChild(widget->second);
        _itemWidgets.erase(widget);
    }
    
    // Remove physics body from world
    auto body = _itemBodies.find(itemId);
    if (body != _itemBodies.end() && body->second && _itemPhysicsWorld) {
        _itemPhysicsWorld->removeObstacle(body->second);
        _itemBodies.erase(body);
    }
}

/**
 * Spawns a ghost animation of a consumed item, fading and scaling it out
 * from the source widget's position. Creates a temporary polygon node using
 * the item's icon texture, anchors it to the center of the source widget,
 * and registers it as an active consumed item animation. Does nothing if
 * any required reference is null or the item's texture cannot be found.
 *
 * Spent/Consumed is the usage of an item through the means of passing, supporting, or attacking.
 * We are creating a clone of it that represents the visual, but not physical version of it.
 *
 * @param sourceWidget  The widget representing the consumed item's position and scale.
 * @param itemDef       The item definition used to look up the icon texture.
 */
void GameScene::spawnConsumedItemAnimation(const std::shared_ptr<SceneNode>& sourceWidget,
                                           const std::shared_ptr<const ItemDef>& itemDef) {
    if (!sourceWidget || !itemDef || !_inventory || !_assets) return;

    auto texture = _assets->get<cugl::graphics::Texture>(itemDef->getIconKey());
    if (!texture) return;

    auto ghost = PolygonNode::allocWithTexture(texture);
    if (!ghost) return;

    //So that it doesn't shrink to the anchor in bottom left, set the center on the item.
    cugl::Rect sourceBounds = sourceWidget->getBoundingBox();
    cugl::Vec2 sourceCenter = sourceBounds.origin + cugl::Vec2(sourceBounds.size.width * 0.5f,sourceBounds.size.height * 0.5f);

    ghost->setAnchor(cugl::Vec2::ANCHOR_CENTER);
    ghost->setContentSize(sourceWidget->getContentSize());
    ghost->setPosition(sourceCenter);
    ghost->setScale(sourceWidget->getScaleX());
    _inventory->addChild(ghost);

    ConsumedItemAnimation anim;
    anim.node = ghost;
    anim.elapsed = 0.0f;
    anim.duration = ITEM_CONSUME_ANIMATION_DURATION;
    anim.startScale = sourceWidget->getScaleX();
    anim.endScale = ITEM_CONSUME_END_SCALE;
    _consumedItemAnimations.push_back(anim);
}

/**
 * Updates the visual "ghost" animations for items that have been used or activated.
 * As an item is spent (consumed) from the inventory, a temporary ghost node is
 * animated by scaling it from its initial size to its final 'disappearance' scale.
 * Spent/Consumed is the usage of an item through the means of passing, supporting, or attacking.
 *
 * Calculates the scale interpolation over the item's use duration.
 * Cleans up the scene graph by detaching nodes once the effect is finished.
 * Validates durations and node pointers to prevent memory errors.
 *
 * @param dt  The time elapsed since the last update, in seconds.
 */
void GameScene::updateConsumedItemAnimations(float dt) {
    if (_consumedItemAnimations.empty()) return;

    for (auto& anim : _consumedItemAnimations) {
        if (!anim.node || anim.duration <= 0.0f) continue;

        anim.elapsed += dt;
        float t = std::min(1.0f, anim.elapsed / anim.duration);
        float scale = anim.startScale + (anim.endScale - anim.startScale) * t;
        anim.node->setScale(scale);
    }

    _consumedItemAnimations.erase(
        std::remove_if(_consumedItemAnimations.begin(), _consumedItemAnimations.end(),
                       [&](const ConsumedItemAnimation& anim) {
                           if (!anim.node) return true;
                           bool finished = anim.elapsed >= anim.duration;
                           if (finished && _inventory) {
                               _inventory->removeChild(anim.node);
                           }
                           return finished;
                       }),
        _consumedItemAnimations.end());
}

/**
 * Clears all active consumed item animations, detaching each ghost node
 * from the inventory scene graph and emptying the animation list.
 */
void GameScene::clearConsumedItemAnimations() {
    if (_inventory) {
        for (const auto& anim : _consumedItemAnimations) {
            if (anim.node) {
                _inventory->removeChild(anim.node);
            }
        }
    }
    _consumedItemAnimations.clear();
}
/*
 * Checks if an item is currently playing an animation.
 * Iterates through active animations to find if the given itemId is animating.
 *
 * @param itemId The ID of the item to check
 * @return true if the item has an active animation, false otherwise
 */
bool GameScene::isItemAnimating(ItemInstance::ItemId itemId) const {
    for (const auto& activeAnim : _activeItemUseAnimations) {
        if (activeAnim.itemId == itemId) {
            return true;
        }
    }
    return false;
}

/** Helper function to spawn an item widget from a given position with animation.
 *
 * @param item       The ItemInstance to spawn
 * @param spawnPos   The world position to spawn from
 * @param slideOrigin The origin type of the slide (SPAWN or PASS)
 */
void GameScene::_spawnItemFromPosition(const ItemInstance& item, cugl::Vec2 spawnPos, ItemInstance::SlideOriginType slideOrigin) {
    ItemInstance::ItemId id = item.getId();
    
    auto widget = createItemWidget(item);
    if (!widget) return;
    
    // Move newly picked up item to front so it appears on top visually
    _inventory->removeChild(widget);
    _inventory->addChild(widget);
    
    widget->setPosition(spawnPos);
    _itemWidgets.emplace(id, widget);
    _itemWidgetScales[id] = ITEM_NORMAL_SCALE;
    _itemWidgetScaleTargets[id] = ITEM_NORMAL_SCALE;
    createItemBody(id, widget);
    
    auto itemBody = _itemBodies[id];
    if (!itemBody) return;
    
    itemBody->setPosition(spawnPos);
    
    // Pick a random target position in the inventory
    cugl::Size widgetSize = widget->getContentSize();
    cugl::Vec2 targetPos = getRandomInventoryPosition(widgetSize);
    
    // Calculate direction and distance to target
    cugl::Vec2 direction = targetPos - spawnPos;
    float distance = direction.length();
    
    // Calculate velocity magnitude needed to reach target with deceleration
    float velocityMagnitude = 0.0f;
    if (distance > 0.0f) {
        velocityMagnitude = std::sqrt(2.0f * ITEM_SLIDE_FRICTION_DECELERATION * distance);
        velocityMagnitude = std::min(velocityMagnitude, ITEM_MOVEMENT_MAX_SPEED);
    }
    
    cugl::Vec2 spawnVelocity = (distance > 0.0f) ? direction.normalize() * velocityMagnitude : cugl::Vec2::ZERO;
    startItemSliding(id, spawnVelocity, slideOrigin);
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
            // Skip items currently animating - they should not be respawned
            if (isItemAnimating(id)) {
                continue;
            }
            
            // Check if this is a passed item (by tracking set OR passDirection metadata)
            bool isPassedItem = (_passedItemIds.find(id) != _passedItemIds.end()) ||
                               (item.getPassDirection() != 0);
            cugl::Size screenSize = getSize();
            cugl::Vec2 spawnPos;
            
            if (isPassedItem) {
                // PASSED ITEMS: Always spawn from side, no limit checks
                spawnPos = getPassSpawnPosition(item.getPassDirection());
                _spawnItemFromPosition(item, spawnPos, ItemInstance::SlideOriginType::SLIDE_FROM_PASS);
            } else {
                // NATURAL SPAWNS: ItemController already rejected if inventory was full.
                // Just spawn from center-bottom.
                spawnPos = cugl::Vec2(screenSize.width * 0.5f, -50.0f);
                _spawnItemFromPosition(item, spawnPos, ItemInstance::SlideOriginType::SLIDE_FROM_SPAWN);
            }
        }
    }

    std::vector<ItemInstance::ItemId> removedIds;
    for (const auto& [itemId, widget] : _itemWidgets) {
        if (liveIds.find(itemId) == liveIds.end()) {
            removedIds.push_back(itemId);
        }
    }
    for (ItemInstance::ItemId itemId : removedIds) {
        removeItemWidget(itemId);
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
void GameScene::renderDropZonesDebug(cugl::graphics::SpriteBatch* batch) {
    batch->setColor(Color4(0, 255, 0, 255));
    
    // Only render zones if holding an item
    if (_draggedItemId != 0) {
        // Always render pass and inventory zones when holding any item
        for (const auto& [action, zone] : _passZones) {
            Path2 path(zone);
            batch->outline(path, Vec2::ZERO, Affine2::IDENTITY);
        }
        
        // Render attack/support zones based on item type
        auto itemDef = getHeldItemDef(_draggedItemId);
        if (itemDef) {
            if (itemDef->getType() == ItemDef::Type::Attack) {
                // Render attack zones when holding attack item
                for (const auto& [action, zone] : _attackZones) {
                    Path2 path(zone);
                    batch->outline(path, Vec2::ZERO, Affine2::IDENTITY);
                }
            } else {
                // Render support zones when holding heal/support item
                for (const auto& [action, zone] : _supportZones) {
                    Path2 path(zone);
                    batch->outline(path, Vec2::ZERO, Affine2::IDENTITY);
                }
            }
        }
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

/** Draws a cyan outline around Box2D debug wireframes for inventory item bodies.
 *
 * @param batch  The active sprite batch.
 */
void GameScene::renderItemBodyDebug(cugl::graphics::SpriteBatch* batch) {
    batch->setTexture(nullptr);
    batch->setGradient(nullptr);
    batch->setColor(Color4(0, 255, 255, 200));

    for (auto& [itemId, body] : _itemBodies) {
        if (!body || _itemWidgets.find(itemId) == _itemWidgets.end()) continue;
        batch->drawMesh(body->getDebugMesh(), body->getGraphicsTransform());
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
        renderItemBodyDebug(batch.get());
        renderPointerDebug(batch.get());
    }
//    renderDropZonesDebug(batch.get());
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
    Player* local = _gameState.getLocalPlayer();
    
    // Dead players can only pass items or put them in inventory
    // They cannot attack or support
    if (local && !local->isAlive()) {
        _inputZones = _passZones;
        _inputZones.insert(_inputZones.end(), _inventoryZones.begin(), _inventoryZones.end());
    } else {
        // Alive players have access to all zones
        _inputZones = _attackZones;
        _inputZones.insert(_inputZones.end(), _supportZones.begin(), _supportZones.end());
        _inputZones.insert(_inputZones.end(), _passZones.begin(), _passZones.end());
        _inputZones.insert(_inputZones.end(), _inventoryZones.begin(), _inventoryZones.end());
    }
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
    const auto& slotToPlayer = _network->getNetworkedPlayers();

    for (const auto& player : _gameState.getPlayers()) {
        if (player->isAI()) continue;
        int slot = player->getPlayerNumber();
        if (slot == _network->getLocalPlayerNumber()) continue;

        auto pair = slotToPlayer.find(slot);
        if (pair != slotToPlayer.end()) {
            activeNetworkIDs[slot] = pair->second.networkID;
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

    // Snapshot state before overwriting
    float savedHealth    = player->getCurrentHealth();
    auto  savedInventory = player->getInventory();
    std::string savedHouse = player->getHouseName();

    // Delegate the actual demotion to GameState
    _gameState.demoteToAI(slot, savedHouse);

    // Restore health and inventory onto the new AI
    Player* newAI = _gameState.getPlayerBySlot(slot);
    auto* ai = dynamic_cast<EasyPlayerAI*>(newAI);
    if (ai) {
        ai->init(_itemController.getDatabase(), "json/playerAI.json");
    }

    newAI->setCurrentHealth(savedHealth);
    for (const ItemInstance& item : savedInventory) {
        newAI->addItem(item);
    }
}

/**
 * HOST and CLIENTS refresh. Updates the left and right teammate name labels to
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
        } else {
            // Capture the house before demoting — GameState still has it here.
            std::string house = "";
            Player* player = _gameState.getPlayerBySlot(slot);
            if (player) house = player->getHouseName();
            _gameState.demoteToAI(slot, house);
        }

        // Mark this slot as handled so we don't re-demote it next frame.
        _slotsDemotedToAI.insert(slot);

        // Step 2b: Both host and clients refresh the teammate name labels.
        refreshTeammateNameLabels();
        resetTeammateBlinkState();
    }
}

/**
 * Disposes and re-initialises the GameState for a fresh session.
 * Call this when aborting the lobby to clear all player house selections.
 */
void GameScene::resetGameState() {
    clearItemUseAnimations();
    _gameState.dispose();
    _gameState.init(_itemController, _assets);
}

/**
 * Calculates the effective damage value for an attack item.
 * 
 * Applies the following formula:
 *   damage = baseValue * (1 + houseRoleMultiplier) * affinityBonus
 * 
 * where:
 *   - baseValue is from the item definition
 *   - houseRoleMultiplier is looked up by player house (typically 0.0-0.5)
 *   - affinityBonus applies to rare/divine items matching player house (typically 1.0-1.5)
 * 
 * This function extracts damage calculation into a reusable utility, allowing
 * animated attacks to calculate damage upfront while deferring application to
 * the animation resolution frame. Non-animated attacks continue to use useItemById()
 * which combines calculation and application.
 * 
 * @param player     The attacking player (provides house name for multiplier lookup)
 * @param itemDef    The item definition (provides base value and affinity info)
 * @param database   The item database (provides house multipliers)
 * @return           Calculated damage magnitude (minimum 0.01f if calculated value <= 0)
 */
float GameScene::calculateItemDamage(const Player* player, const std::shared_ptr<const ItemDef>& itemDef, const ItemDatabase& database) {
    if (!player || !itemDef) return 0.0f;
    
    float houseRoleMultiplier = 0.0f;
    float affinityBonus = 1.0f;
    const auto* houseMultipliers = database.getHouseMultipliers(player->getHouseName());
    if (houseMultipliers) {
        houseRoleMultiplier = houseMultipliers->attack;
        // Affinity bonus only applies to rare/divine items when item affinity matches player house
        const bool affinityEligible = (itemDef->getRarity() == ItemDef::Rarity::Rare || 
                                       itemDef->getRarity() == ItemDef::Rarity::Divine);
        const bool affinityMatch = (itemDef->getHouseAffinity() == ItemDef::houseFromString(player->getHouseName(), ItemDef::House::None));
        if (affinityEligible && affinityMatch) {
            affinityBonus = houseMultipliers->affinityBonus;
        }
    }
    float resolvedMagnitude = itemDef->getBaseValue() * (1.0f + houseRoleMultiplier) * affinityBonus;
    if (resolvedMagnitude <= 0.0f) {
        resolvedMagnitude = 0.0f;
    }
    
    CULog("ItemDamageCalc: item='%s' playerHouse='%s' baseVal=%.3f * (1+%.3f) * %.3f = %.3f",
          itemDef->getId().c_str(),
          player->getHouseName().c_str(),
          itemDef->getBaseValue(),
          houseRoleMultiplier,
          affinityBonus,
          resolvedMagnitude);
    
    return resolvedMagnitude;
}

/**
 * Removes an item from a player's inventory by item instance ID.
 * 
 * Searches through the player's inventory for an item matching the given ID
 * and erases it from the inventory vector if found. Used to decouple item
 * removal from damage application, allowing animated attacks to consume the
 * item immediately while deferring damage to the animation keyframe.
 * 
 * @param player   The player whose inventory to modify (non-null)
 * @param itemId   The unique ID of the item instance to remove
 * @return         true if item was found and removed, false if not found or player is null
 */
bool GameScene::removeItemFromInventory(Player* player, ItemInstance::ItemId itemId) {
    if (!player) return false;
    
    auto& inventory = const_cast<std::vector<ItemInstance>&>(player->getInventory());
    for (auto itemIter = inventory.begin(); itemIter != inventory.end(); ++itemIter) {
        if (itemIter->getId() == itemId) {
            inventory.erase(itemIter);
            return true;
        }
    }
    return false;
}

/**
 * Queues an item use animation for display in the special effects layer.
 * 
 * Loads the sprite sheet texture, creates a SpriteNode to render it, and adds the
 * animation to the active queue for frame-by-frame updating. The animation tracks
 * elapsed time and advances sprite frames proportionally to animation progress.
 * 
 * Damage is applied later in updateItemUseAnimations() when the keyframe is reached.
 * This deferred application allows multiple systems to hook into the animation lifecycle.
 */
void GameScene::startItemUseAnimation(const ItemUseAnimationConfig& animConfig, float damageAmount,
                                       const cugl::Vec2& itemPos, ItemInstance::ItemId itemId) {
    // Load the sprite sheet texture
    auto texture = _assets->get<cugl::graphics::Texture>(animConfig.spriteSheetId);
    if (!texture) {
        CULog("ERROR: Failed to load sprite sheet texture: %s", animConfig.spriteSheetId.c_str());
        return;
    }
    
    // Create a SpriteSheet to manage frame layout
    auto spriteSheet = cugl::graphics::SpriteSheet::alloc(texture, animConfig.rows, animConfig.cols, animConfig.frameCount);
    if (!spriteSheet) {
        CULog("ERROR: Failed to allocate sprite sheet for animation");
        return;
    }
    
    auto frameSize = spriteSheet->getFrameSize();
    
    // Create a SpriteNode which is designed for sprite sheet animations
    auto node = cugl::scene2::SpriteNode::allocWithSheet(texture, animConfig.rows, animConfig.cols, animConfig.frameCount);
    if (!node) {
        CULog("ERROR: Failed to allocate SpriteNode for item use animation");
        return;
    }
    
    // Set initial frame to 0
    node->setFrame(0);
    
    // Position the node
    cugl::Vec2 position = itemPos;
    cugl::Size viewportSize = getSize();
    if (itemPos == cugl::Vec2::ZERO) {
        position = cugl::Vec2(viewportSize.width / 2.0f, viewportSize.height * 0.55f);
    }
    
    node->setPosition(position);
    node->setAnchor(cugl::Vec2(0.5f, 0.5f));
    
    // Scale animation to fit viewport width while maintaining aspect ratio
    // Use setScale instead of setContentSize to avoid distorting the texture
    float scale = viewportSize.width / frameSize.width;
    node->setScale(scale);
    
    // Add to special effects layer
    _specialEffectsLayer->addChild(node);
    
    // Create and queue the animation instance
    ItemUseAnimation anim;
    anim.spriteSheet = spriteSheet;
    anim.node = node;
    anim.basePosition = position;
    anim.frameSize = frameSize;
    anim.frameCount = animConfig.frameCount;
    anim.frameCols = animConfig.cols;
    anim.animationDuration = animConfig.animationDuration;
    anim.damageResolutionFrame = animConfig.damageResolutionFrame;
    anim.damageAmount = damageAmount;
    anim.itemId = itemId;
    anim.elapsedTime = 0.0f;
    anim.damageResolved = false;
    anim.currentFrameIndex = 0;  // Initialize to 0 since we set frame 0 above
    
    _activeItemUseAnimations.push_back(anim);
}

/**
 * Updates all active item use animations for one frame.
 * 
 * For each animation in _activeItemUseAnimations:
 *   1. Increments elapsedTime by dt
 *   2. Calculates frame index based on animation progress
 *   3. Updates sprite if frame changed (optimization: avoid redundant setFrame calls)
 *   4. At damageResolutionFrame: applies damage and broadcasts network message
 *   5. Cleans up animation when duration elapsed
 * 
 * CRITICAL NETWORK BEHAVIOR:
 *   - Host: Applies damage locally, broadcasts via next broadcastGameState() (tick-synchronized)
 *   - Client: Broadcasts damage message immediately (so host processes same frame it resolves)
 *   This ensures all machines apply damage at identical animation frames, preventing state desync.
 *
 * @param dt  Delta time in seconds (from game loop)
 */
void GameScene::updateItemUseAnimations(float dt) {
    if (_activeItemUseAnimations.empty()) {
        return;
    }
    
    std::vector<size_t> completedIndices;
    
    for (size_t i = 0; i < _activeItemUseAnimations.size(); ++i) {
        auto& activeAnim = _activeItemUseAnimations[i];

        // Advance time and derive the current frame from normalized progress.
        activeAnim.elapsedTime += dt;
        const float progress = std::min(1.0f, activeAnim.elapsedTime / activeAnim.animationDuration);
        int frameIndex = std::min(static_cast<int>(progress * activeAnim.frameCount), activeAnim.frameCount - 1);

        // Only call setFrame when the index actually changes to avoid redundant GPU uploads.
        if (frameIndex != activeAnim.currentFrameIndex) {
            activeAnim.currentFrameIndex = frameIndex;
            activeAnim.node->setFrame(frameIndex);
        }

        // Damage resolution fires exactly once when we hit or pass the keyframe.
        if (!activeAnim.damageResolved && frameIndex >= activeAnim.damageResolutionFrame) {
            activeAnim.damageResolved = true;

            if (activeAnim.damageAmount > 0.0f) {
                auto enemy = _gameState.getEnemy();
                if (enemy) {
                    const int   playerNum      = _gameState.getLocalPlayer()->getPlayerNumber();
                    const float sideMultiplier = enemy->getSideMultiplier(playerNum);
                    const float finalDamage    = activeAnim.damageAmount * sideMultiplier;

                    // Apply pre-calculated damage and show the popup sequence.
                    enemy->takeDamage(activeAnim.damageAmount, playerNum);
                    createFloatingPopup(activeAnim.popupPosition, buildAttackDamagePopups(
                        activeAnim.baseValue, activeAnim.totalMultiplier, sideMultiplier,
                        activeAnim.damageAmount, finalDamage, 22.0f, 14.0f));

                    // Non-hosts broadcast so the host applies it on the same frame.
                    if (_network && !_network->isHost()) {
                        _network->broadcastDamage(activeAnim.damageAmount, playerNum);
                        broadcastEnemyEffects(*_network, activeAnim.enemyEffects);
                    }
                }
            }

            // Host plays enemy_hurt immediately; clients hear it via network sync.
            if (_network && _network->isHost() && _audio) {
                _audio->playSoundUnique("enemy_hurt");
            }
        }

        // Queue for removal once the full animation duration has elapsed.
        if (activeAnim.elapsedTime >= activeAnim.animationDuration) {
            activeAnim.node->removeFromParent();
            completedIndices.push_back(i);
        }
    }

    // Erase in reverse order so earlier indices stay valid as we remove later ones.
    for (auto indexToRemove = completedIndices.rbegin(); indexToRemove != completedIndices.rend(); ++indexToRemove) {
        _activeItemUseAnimations.erase(_activeItemUseAnimations.begin() + *indexToRemove);
    }
}

/**
 * Loads the animation registry from enemyAnimations.json and populates _animationRegistry.
 * Parses the JSON array of animation entries and builds a map for O(1) lookup by animation ID.
 * Supports optional fields for attack phase configuration and position/scale customization.
 */
void GameScene::loadAnimationRegistry() {
    _animationRegistry.clear();
    
    // Load the animationRegistry array from enemyAnimations.json
    // The asset key is registered in assets.json as "enemyAnimations"
    auto json = _assets->get<cugl::JsonValue>("enemyAnimations");
    
    if (!json) {
        return;
    }
    
    auto registryArray = json->get("animationRegistry");
    if (!registryArray || !registryArray->isArray()) {
        return;
    }
    
    // Parse each animation entry
    for (int i = 0; i < registryArray->size(); i++) {
        auto entry = registryArray->get(i);
        if (!entry) continue;
        
        AnimationEntry anim;
        anim.id = entry->getString("id", "");
        anim.texture = entry->getString("texture", "");
        anim.frameCount = entry->getInt("frameCount", 0);
        anim.frameDuration = entry->getFloat("frameDuration", 0.1f);
        anim.frameRows = entry->getInt("frameRows", 1);
        
        // Parse attack phase configuration (optional)
        // Default buildupFrameCount to frameCount (no attack phase) if not specified
        anim.buildupFrameCount = entry->getInt("buildupFrameCount", anim.frameCount);
        anim.damageFrame = entry->getInt("damageFrame", -1);
        
        // Parse position and scale customization (optional, with defaults)
        anim.positionX = entry->getFloat("positionX", 196.5f);
        anim.positionY = entry->getFloat("positionY", 120.0f);
        anim.scale = entry->getFloat("scale", 0.92f);
        anim.offsetX = entry->getFloat("offsetX", 0.0f);
        anim.offsetY = entry->getFloat("offsetY", 0.0f);
        
        if (!anim.id.empty()) {
            _animationRegistry[anim.id] = anim;
        }
    }
}

/**
 * Clears all active item use animations, removing them from the scene graph.
 * Called when the game ends or resets to clean up any in-flight animations
 * and prevent orphaned sprite nodes from persisting after scene transitions.
 */
void GameScene::clearItemUseAnimations() {
    // Remove all animation nodes from the scene graph
    for (auto& activeAnimation : _activeItemUseAnimations) {
        if (activeAnimation.node) {
            activeAnimation.node->removeFromParent();
        }
    }
    
    // Clear the animation list
    _activeItemUseAnimations.clear();
}

#pragma mark Popup Helpers

/**
 * Maps a damage multiplier magnitude to a popup color.
 * Blue for sub-1.0, orange for 1.0–1.5, red for anything above 1.5.

 * @param damage The damage value to evaluate (after all multipliers applied)
 * @return The Color4 to use for the damage popup text
 */
static cugl::Color4 damageColor(float damage) {
    if (damage < 1.0f) return cugl::Color4(140, 180, 255, 255);
    if (damage < 2.5f) return cugl::Color4(255, 165,  40, 255);
    return cugl::Color4(255,  55,  55, 255);
}


/**
 * Returns the screen-space drop position of the given item's physics body.
 * Falls back to the viewport center (55% height) when no body is found.
 * @param itemId The ID of the item instance to find the body for
 * @return The center of the item in screen coordinates
 */
cugl::Vec2 GameScene::resolveItemDropPosition(ItemInstance::ItemId itemId) const {
    auto itemBody = _itemBodies.find(itemId);
    if (itemBody != _itemBodies.end() && itemBody->second) {
        return itemBody->second->getPosition();
    }
    cugl::Size viewSize = getSize();
    return cugl::Vec2(viewSize.width * 0.5f, viewSize.height * 0.55f);
}

/**
 * Builds the ordered popup sequence for an attack item use.
 * See header for full parameter and sequence documentation.
 * 
 * @param baseValue The original damage value from the item definition
 * @param totalMultiplier The combined multiplier from house role and affinity bonuses
 * @param sideMultiplier The enemy's side multiplier for the attacking player
 * @param preSideDamage The damage after applying house multipliers but before side multiplier
 * @param finalDamage The final damage after applying all multipliers
 * @param valueFontSize The base font size for damage values (house multiplier and final damage)
 * @param multiplierFontSize The base font size for multipliers (house and side)
 * @return A vector of FloatingPopupData structs defining the popup sequence
 */
std::vector<FloatingPopupData> GameScene::buildAttackDamagePopups(
    float baseValue, float totalMultiplier, float sideMultiplier,
    float preSideDamage, float finalDamage,
    float valueFontSize, float multiplierFontSize) const
{
    const bool hasSideMult = (std::abs(sideMultiplier - 1.0f) > 0.01f);

    // Format each value as a fixed one-decimal string.
    char baseText[32], houseText[32], preText[32], sideText[32], finalText[32];
    std::snprintf(baseText,  sizeof(baseText),  "-%.1f", baseValue);
    std::snprintf(houseText, sizeof(houseText), "%.1fx", totalMultiplier);
    std::snprintf(preText,   sizeof(preText),   "-%.1f", preSideDamage);
    std::snprintf(sideText,  sizeof(sideText),  "%.1fx", sideMultiplier);
    std::snprintf(finalText, sizeof(finalText), "-%.1f", finalDamage);

    // Log-scale the multiplier font size so larger multipliers get proportionally bigger text.
    const float houseLog    = 0.2f * std::log(std::max(1.0f, totalMultiplier));
    const float sideLog     = 0.2f * std::log(std::max(1.0f, sideMultiplier));
    const float combinedLog = 0.2f * std::log(std::max(1.0f, totalMultiplier * sideMultiplier));

    // Green for a bonus side, blue for a penalty side.
    const cugl::Color4 sideColor = (sideMultiplier >= 1.0f)
        ? cugl::Color4(150, 220,  80, 255)
        : cugl::Color4(120, 160, 255, 255);

    // If the enemy has a side multiplier on the local player's side
    if (hasSideMult) {
        return {
            {baseText,  valueFontSize,                                  cugl::Color4(160, 160, 160, 255), cugl::Color4::BLACK, 0.0f,  0.2f,  cugl::Vec2::ZERO,         true},
            {houseText, multiplierFontSize * (1.0f + houseLog),         cugl::Color4(244, 186,  51, 255), cugl::Color4::BLACK, 0.05f, 0.25f, cugl::Vec2(20.0f, 15.0f), false},
            {preText,   valueFontSize      * (1.0f + houseLog),         damageColor(preSideDamage),        cugl::Color4::BLACK, 0.3f,  0.15f, cugl::Vec2::ZERO,         true},
            {sideText,  multiplierFontSize * (1.0f + sideLog),          sideColor,                        cugl::Color4::BLACK, 0.35f, 0.2f,  cugl::Vec2(20.0f, 15.0f), false},
            {finalText, valueFontSize      * (1.0f + combinedLog),      damageColor(finalDamage),          cugl::Color4::BLACK, 0.55f, 0.5f,  cugl::Vec2::ZERO,         true},
        };
    }
    return {
        {baseText,  valueFontSize,                                  cugl::Color4(160, 160, 160, 255), cugl::Color4::BLACK, 0.0f,  0.15f, cugl::Vec2::ZERO,         true},
        {houseText, multiplierFontSize * (1.0f + houseLog),         cugl::Color4(244, 186,  51, 255), cugl::Color4::BLACK, 0.05f, 0.3f,  cugl::Vec2(20.0f, 15.0f), false},
        {finalText, valueFontSize      * (1.0f + houseLog),         damageColor(preSideDamage),        cugl::Color4::BLACK, 0.35f, 0.5f,  cugl::Vec2::ZERO,         true},
    };
}

/**
 * Builds the ordered popup sequence for a heal support item use.
 * See header for the full sequence description.
 *
 * @param baseValue     Item's raw base heal value from the item definition.
 * @param resolvedHeal  Final resolved heal after house/affinity multipliers.
 * @return Ordered list of FloatingPopupData for the sequence (1 or 3 entries).
 */
std::vector<FloatingPopupData> GameScene::buildHealPopups(float baseValue, float resolvedHeal) const {
    // Back-calculate the house multiplier from the resolved heal so we can show it in the sequence.
    const float totalMultiplier = (baseValue > 0.0f) ? resolvedHeal / baseValue : 1.0f;
    const float houseLog        = 0.2f * std::log(std::max(1.0f, totalMultiplier));

    char baseText[32], houseText[32], finalText[32];
    std::snprintf(baseText,  sizeof(baseText),  "+%.1f", baseValue);
    std::snprintf(houseText,  sizeof(houseText),  "%.1fx", totalMultiplier);
    std::snprintf(finalText, sizeof(finalText), "+%.1f", resolvedHeal);

    const cugl::Color4 healGreen(80, 220, 80, 255);

    // Only show the full sequence when the multiplier actually changed something.
    if (std::abs(totalMultiplier - 1.0f) > 0.01f) {
        return {
            {baseText,  26.0f,                    cugl::Color4(160, 160, 160, 255), cugl::Color4::BLACK, 0.0f,  0.15f, cugl::Vec2::ZERO,         true},
            {houseText, 17.0f*(1.0f+houseLog),    cugl::Color4(244, 186,  51, 255), cugl::Color4::BLACK, 0.05f, 0.3f,  cugl::Vec2(20.0f, 15.0f), false},
            {finalText, 26.0f*(1.0f+houseLog),    healGreen,                        cugl::Color4::BLACK, 0.35f, 0.5f,  cugl::Vec2::ZERO,         true},
        };
    }
    return {
        {finalText, 26.0f, healGreen, cugl::Color4::BLACK, 0.0f, 0.5f, cugl::Vec2::ZERO, true},
    };
}

/**
 * Fires visual popups for any shield or barrier effects on a support item.
 * Must be called before useItemById so shield-only items still produce a popup.
 *
 * @param def      The item definition whose effects to scan.
 * @param dropPos  Screen-space position where popups appear.
 */
void GameScene::spawnDefensiveEffectPopups(const std::shared_ptr<const ItemDef>& def,
                                   const cugl::Vec2& dropPos) {
    for (const auto& effect : def->getEffects()) {
        if (effect.type == ItemDef::EffectType::Shield && effect.mitigation > 0.0f) {
            char text[32];
            std::snprintf(text, sizeof(text), "[%.1f]", effect.mitigation);
            createFloatingPopup(dropPos, {{text, 26.0f, cugl::Color4(80, 200, 255, 255), cugl::Color4::BLACK, 0.0f, 0.5f, cugl::Vec2::ZERO, true}});
        } else if (effect.type == ItemDef::EffectType::Barrier && effect.multiplier < 1.0f) {
            char text[32];
            const float reductionPct = (1.0f - effect.multiplier) * 100.0f;
            std::snprintf(text, sizeof(text), "[%.0f%%]", reductionPct);
            createFloatingPopup(dropPos, {{text, 26.0f, cugl::Color4(180, 80, 255, 255), cugl::Color4::BLACK, 0.0f, 0.5f, cugl::Vec2::ZERO, true}});
        }
    }
}

/**
 * Plays the item's defined use sound, or the generic "support" sound if none is set.
 *
 * @param def  The item definition to read the sound key from.
 */
void GameScene::playSupportItemSound(const std::shared_ptr<const ItemDef>& def) {
    const std::string& soundKey = def->getItemUseSound();
    _audio->playSoundUnique(soundKey.empty() ? "support" : soundKey);
}

#pragma mark Popup Management

/**
 * Queues a sequence of popups to appear at a screen position.
 * Popups with a non-zero delaySeconds are held in the pending list until
 * their delay elapses in updatePopupAnimations().
 *
 * @param screenPosition  Base screen-space position (positionOffset from each popup is added on top).
 * @param popups          Ordered sequence of popup descriptors.
 */
void GameScene::createFloatingPopup(
    const cugl::Vec2& screenPosition,
    const std::vector<FloatingPopupData>& popups)
{
    if (popups.empty()) return;
    for (const auto& data : popups) {
        PendingFloatingPopup pending;
        pending.data = data;
        pending.position = screenPosition + data.positionOffset;
        pending.spawnTime = data.delaySeconds;
        pending.elapsed = 0.0f;
        _pendingFloatingPopups.push_back(pending);
    }
}

/**
 * Immediately builds and activates one floating popup.
 * Measures the label's text bounds, sizes the container to match so
 * ANCHOR_CENTER resolves correctly, then adds 8 black outline copies
 * (cardinal + diagonal offsets) followed by the colored label on top.
 * Registers the container in _activeFloatingPopups to begin animating.
 * 
 * @param data      Popup descriptor defining text, fill color, stroke color, font size, and timing.
 * @param position  Screen-space position to spawn the popup at (positionOffset already applied).
 * 
 */
void GameScene::spawnSingleFloatingPopup(const FloatingPopupData& data, const cugl::Vec2& position) {
    auto font = _assets->get<cugl::graphics::Font>("gamePin");
    if (!font) return;

    if (data.playSound && _audio) _audio->playSoundUnique("popup_ding");

    // displayScale maps the requested font size onto the base font size the asset was baked at.
    const float displayScale = data.fontSize / FLOATING_POPUP_BASE_FONT_SIZE;

    // Allocate the colored label first so we can measure its content size.
    auto label = cugl::scene2::Label::allocWithText(data.text, font);
    if (!label) return;
    label->setForeground(data.color);
    const cugl::Size textSize = label->getContentSize();
    const cugl::Vec2 center(textSize.width * 0.5f, textSize.height * 0.5f);

    // The container must be given an explicit content size matching the label so
    // ANCHOR_CENTER resolves to the true visual center, not (0,0).
    auto container = cugl::scene2::SceneNode::alloc();
    container->setContentSize(textSize);
    container->setAnchor(cugl::Vec2::ANCHOR_CENTER);
    container->setPosition(position);
    container->setScale(0.0f);

    // 8 black copies at cardinal + diagonal offsets produce a crisp outline.
    const float outlineOffset = 3.0f;
    const cugl::Vec2 outlineOffsets[] = {
        { outlineOffset,  0},             {-outlineOffset,  0},
        { 0,              outlineOffset}, { 0,             -outlineOffset},
        { outlineOffset,  outlineOffset}, {-outlineOffset,  outlineOffset},
        { outlineOffset, -outlineOffset}, {-outlineOffset, -outlineOffset}
    };
    for (const auto& offset : outlineOffsets) {
        auto outlineLabel = cugl::scene2::Label::allocWithText(data.text, font);
        if (outlineLabel) {
            outlineLabel->setAnchor(cugl::Vec2::ANCHOR_CENTER);
            outlineLabel->setPosition(center + offset);
            outlineLabel->setForeground(data.strokeColor);
            container->addChild(outlineLabel);
        }
    }

    label->setAnchor(cugl::Vec2::ANCHOR_CENTER);
    label->setPosition(center);
    container->addChild(label);

    _specialEffectsLayer->addChild(container);

    FloatingPopupAnimation popupAnim;
    popupAnim.node               = container;
    popupAnim.elapsed            = 0.0f;
    popupAnim.animationInDuration  = FLOATING_POPUP_ANIM_IN;
    popupAnim.displayDuration    = data.displayDuration;
    popupAnim.animationOutDuration = FLOATING_POPUP_ANIM_OUT;
    popupAnim.displayScale       = displayScale;
    popupAnim.phase              = FloatingPopupAnimation::ANIM_IN;

    _activeFloatingPopups.push_back(popupAnim);
}

/**
 * Advances all pending and active floating popups by one frame.
 * Pending popups are spawned once their delay timer elapses.
 * Active popups step through scale-in (with a slight overshoot), hold, then fade out.
 * @param dt Delta time in seconds (from game loop)
 */
void GameScene::updatePopupAnimations(float dt) {
    // Spawn any pending popups whose delay timer has elapsed.
    for (auto pendingEntry = _pendingFloatingPopups.begin(); pendingEntry != _pendingFloatingPopups.end(); ) {
        pendingEntry->elapsed += dt;
        if (pendingEntry->elapsed >= pendingEntry->spawnTime) {
            spawnSingleFloatingPopup(pendingEntry->data, pendingEntry->position);
            pendingEntry = _pendingFloatingPopups.erase(pendingEntry);
        } else {
            ++pendingEntry;
        }
    }

    // Advance each live popup through its three animation phases.
    for (auto popupEntry = _activeFloatingPopups.begin(); popupEntry != _activeFloatingPopups.end(); ) {
        popupEntry->elapsed += dt;
        const float totalDuration = popupEntry->animationInDuration
                                  + popupEntry->displayDuration
                                  + popupEntry->animationOutDuration;

        if (popupEntry->elapsed >= totalDuration) {
            popupEntry->node->removeFromParent();
            popupEntry = _activeFloatingPopups.erase(popupEntry);
            continue;
        }

        float scale, alpha;
        if (popupEntry->elapsed < popupEntry->animationInDuration) {
            // Phase IN: scale up with a brief overshoot for a punchy feel.
            popupEntry->phase = FloatingPopupAnimation::ANIM_IN;
            const float t = popupEntry->elapsed / popupEntry->animationInDuration;
            // Overshoot to 1.25× at t=0.6, settle to 1.0× by t=1.0
            const float overshoot = t < 0.6f ? (t / 0.6f) * 1.25f : 1.25f - (t - 0.6f) / 0.4f * 0.25f;
            scale = overshoot * popupEntry->displayScale;
            alpha = t;
        } else if (popupEntry->elapsed < popupEntry->animationInDuration + popupEntry->displayDuration) {
            // Phase DISPLAY: hold at full scale and full opacity.
            popupEntry->phase = FloatingPopupAnimation::ANIM_DISPLAY;
            scale = popupEntry->displayScale;
            alpha = 1.0f;
        } else {
            // Phase OUT: shrink slightly while fading to transparent.
            popupEntry->phase = FloatingPopupAnimation::ANIM_OUT;
            const float t = (popupEntry->elapsed - popupEntry->animationInDuration - popupEntry->displayDuration)
                          / popupEntry->animationOutDuration;
            scale = popupEntry->displayScale * (1.0f - t * 0.5f);
            alpha = 1.0f - t;
        }

        popupEntry->node->setScale(scale);
        popupEntry->node->setColor(cugl::Color4(255, 255, 255, (uint8_t)(alpha * 255)));
        ++popupEntry;
    }
}
