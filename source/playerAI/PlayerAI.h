#ifndef __PLAYER_AI_H__
#define __PLAYER_AI_H__

#include <cugl/cugl.h>
#include "Player.h"
#include "Enemy.h"
#include "ItemController.h"
#include "ItemDatabase.h"

/**
 * Abstract base class for all AI difficulty levels.
 *
 * PlayerAI wraps a bot-controlled Player and drives it using a finite state
 * machine (FSM). The shared FSM loop and initialization logic live here,
 * while difficulty-specific decision-making is delegated to subclasses via
 * pure virtual methods.
 *
 * Subclasses (e.g. EasyPlayerAI, MediumPlayerAI) override evaluate() and the
 * action handlers to implement their own behavior. The base class handles the
 * think timer, state dispatch, and JSON config loading.
 *
 * Support and pass actions target only the left or right neighbor of the
 * controlled player, as set by Player::setLeftPlayer / setRightPlayer.
 *
 * Usage:
 *   1. Instantiate a concrete subclass (e.g. EasyPlayerAI)
 *   2. Call init() with a pointer to the bot's Player and a config path
 *   3. Call update() each frame from GameScene::update()
 *   4. Optionally read getState() for debugging or UI feedback
 *
 * All AI controllers can be stored as:
 *   std::vector<std::unique_ptr<PlayerAI>> _aiControllers;
 * allowing mixed difficulty levels in the same collection.
 */
class PlayerAI : public Player {
public:

    /**
     * The finite state machine states shared across all AI difficulty levels.
     *
     * IDLE        - No action to take; waiting for the next think cycle.
     * EVALUATE    - Actively assessing game context to determine the next state.
     * ATTACK      - Using an attack item on the enemy.
     * SUPPORT     - Using a support item to heal a neighbor.
     * PASS        - Passing an item to an adjacent neighbor.
     */
    enum class State {
        IDLE,
        EVALUATE,
        ATTACK,
        SUPPORT,
        PASS
    };

protected:

    /** The current FSM state of this AI controller. */
    State _state = State::IDLE;

    /** Accumulator tracking elapsed time since the last think cycle. */
    float _thinkTimer;

    /**
     * How often (in seconds) the AI re-evaluates game context and transitions states.
     * Loaded from JSON. Higher values produce slower, more deliberate bot behavior.
     */
    float _thinkInterval;

    /**
     * Weight driving how likely the AI is to prioritize attacking over supporting.
     * Range [0, 1]. Loaded from JSON personality config.
     */
    float _aggressionWeight;

    /**
     * Weight driving how likely the AI is to prioritize healing neighbors.
     * Range [0, 1]. Loaded from JSON personality config.
     */
    float _supportWeight;

    /**
     * The health ratio below which a neighbor is considered to need healing.
     * e.g. 0.5 means the AI will attempt to heal neighbors below 50% health.
     * Loaded from JSON.
     */
    float _healThreshold;

    /** Read-only reference to the item database for looking up item types and effects. */
    const ItemDatabase* _db = nullptr;

public:

    /**
     * Constructs a PlayerAI, forwarding all arguments to the Player base constructor.
     * Required because Player has no default constructor.
     *
     * @param characterId   The character ID as it appears in characters.json
     * @param playerNumber  The assigned player slot number
     * @param playerName    Display name for this player
     * @param loader        The CharacterLoader used to populate stats
     */
    PlayerAI(const std::string& characterId,
             int playerNumber,
             const std::string& playerName,
             const CharacterLoader& loader)
        : Player(characterId, playerNumber, playerName, loader) {}

    /**
     * Virtual destructor — required for safe polymorphic deletion of subclasses
     * stored as PlayerAI pointers.
     */
    virtual ~PlayerAI() = default;

    /**
     * Initializes the AI controller with an item database and JSON config.
     *
     * Loads shared parameters (thinkInterval, aggressionWeight, supportWeight,
     * healThreshold) from the config. Returns false if any field is missing.
     *
     * Subclasses that override init() should call PlayerAI::init() first to
     * ensure shared fields are populated before reading their own fields.
     *
     * @param db        Read-only reference to the item database for item lookups.
     * @param path      Path to the shared playerAI.json config file.
     * @return true if initialization succeeded, false otherwise.
     */
    virtual bool init(const ItemDatabase& db, const std::string& path);

    /**
     * Steps the AI forward by one frame.
     *
     * Accumulates dt into the think timer. Once the timer exceeds _thinkInterval,
     * calls evaluate() to determine the next FSM state, then dispatches to the
     * appropriate action handler (actAttack, actSupport, actPass). Resets the
     * timer after each think cycle.
     *
     * This method is not virtual — the FSM loop is identical for all difficulty
     * levels. Only evaluate() and the action handlers differ between subclasses.
     *
     * @param dt        Time elapsed since the last frame in seconds.
     * @param enemy     The current enemy, used as the attack target.
     * @param items     The ItemController, used to execute item actions.
     */
    void update(float dt, Enemy& enemy, ItemController& items);

    /**
     * Returns the current FSM state of this AI controller.
     * Useful for debugging and logging state transitions.
     */
    State getState() const { return _state; }

protected:

    /**
     * Evaluates the current game context and returns the most appropriate next state.
     *
     * Subclasses implement this to encode their difficulty-specific decision logic.
     * Neighbors are accessed directly via _player->getLeftPlayer() and getRightPlayer().
     *
     * @param enemy     The current enemy, used to assess attack viability.
     * @return the State the AI should transition to.
     */
    virtual State evaluate(const Enemy& enemy) = 0;

    /**
     * Executes an attack action against the enemy.
     *
     * Subclasses implement this to define how the AI selects and uses an attack
     * item. Easy AI may use a random item; harder AI may choose optimally.
     *
     * @param enemy     The enemy to attack.
     * @param items     The ItemController used to resolve the item action.
     */
    virtual void actAttack(Enemy& enemy, ItemController& items) = 0;

    /**
     * Executes a support action to heal an injured neighbor.
     *
     * Subclasses implement this to define how the AI selects a target and
     * a support item. Target must be the left or right neighbor.
     *
     * @param items     The ItemController used to resolve the item action.
     */
    virtual void actSupport(ItemController& items) = 0;

    /**
     * Executes a pass action to transfer an item to a neighbor.
     *
     * Subclasses implement this to define pass target and item selection.
     * Target must be the left or right neighbor.
     */
    virtual void actPass() = 0;
    
    bool isAI() const override { return true; }
};

#endif /* __PLAYER_AI_H__ */
