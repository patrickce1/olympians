// TutorialController.h
#ifndef __TUTORIAL_CONTROLLER_H__
#define __TUTORIAL_CONTROLLER_H__

#include <cugl/cugl.h>
#include <optional>
#include "InputController.h"

class GameScene;

/**
 The type of step within the tutorial.
 */
enum class StepType {
    SHOW_MESSAGE,
    WAIT_FOR_ACTION,
    SPAWN_ITEM,
    BOSS_ATTACK,
    BOSS_DEFEND,
    DELAY,
    END,
    UNKNOWN
};

/**
 * The atomic step description parsed from a JSON.
 * Each field is optional aside from type, which defines what this step will do.
 */
struct TutorialStep {
    StepType type = StepType::UNKNOWN;
    std::string text;
    InputController::Action action;
    std::string defId;
    float delay;
    int passDirection = 0;
    std::optional<bool> bossActive;
    std::optional<int> bossTarget;
};

class TutorialController {
public:
    
#pragma mark - Constructors

    /** Constructs an uninitialised TutorialController. Call init() before use. */
    TutorialController() = default;

    /**
     * Destroys the TutorialController.
     * Equivalent to calling dispose().
     */
    ~TutorialController() { dispose(); }
    
#pragma mark - Lifecycle

    /**
     * Initializes the TutorialController with a game scene and asset manager.
     *
     * @param scene   A pointer to the active GameScene. Must not be null.
     * @param assets  The shared asset manager used to load tutorial resources. Must not be null.
     *
     * @return true if initialization succeeded, false if either parameter is null.
     */
    bool init(GameScene* scene, const std::shared_ptr<cugl::AssetManager>& assets);

    /**
     * Releases all resources held by the TutorialController and resets it to
     * its default state.
     *
     * Clears the tutorial step sequence, nullifies scene and asset references,
     * and resets all playback state including the current index, timer, and
     * action-wait flag.
     */
    void dispose();
    
    /**
     * Loads tutorial instructions from the "tutorial" JSON asset.
     *
     * Retrieves the JSON value keyed "tutorial" from the asset manager,
     * parses it into the internal step sequence via parseSteps(), and
     * logs the number of steps loaded. Does nothing if the asset manager
     * is null or the asset is not found.
     */
    void loadInstructionsFromJson();

    /**
     * Loads tutorial instructions from a JSON file at the given path.
     *
     * @param path  The file path to the JSON tutorial definition.
     *
     * @return true if the file was successfully read and parsed, false if
     *         the file could not be opened or contained invalid JSON.
     */
    bool loadFromFile(const std::string& path);

    /**
     * Starts the tutorial from the beginning.
     *
     * Resets all playback state, deactivates the boss in the game scene,
     * and advances to the first step. Does nothing if the step sequence
     * is empty.
     */
    void start();

#pragma mark - State
    
    /** True if tutorial is currently running. */
    bool isActive() const { return _active; }

    /** Current step index (for debugging). */
    int getIndex() const { return _index; }

    /** True if waiting for player action (for debugging). */
    bool isWaiting() const { return _waitingForAction; }

    /**
     * Returns whether the given action would satisfy the current wait condition.
     *
     * @param action  The action to check against the current step's requirement.
     *
     * @return true if the tutorial is active, waiting for an action, and the
     *         current step accepts NONE (any action) or matches the given action.
     *         False otherwise.
     */
    bool isWaitingForActionMatch(InputController::Action action) const;
#pragma mark - Update
    /**
     * Updates the tutorial state for the current frame.
     *
     * Steps the internal timer by dt and advances to the next step once the
     * timer expires. WAIT_FOR_ACTION steps are skipped by the timer and may
     * only be advanced by handlePlayerAction(). Does nothing if the tutorial is
     * inactive or the current index is out of bounds.
     *
     * @param dt  Elapsed time in seconds since the last frame.
     */
    void update(float dt);

#pragma mark - Input
    /**
     * Notifies the tutorial that a player action has occurred.
     *
     * If the tutorial is active and waiting on a WAIT_FOR_ACTION step,
     * checks whether the given action satisfies the step's requirement.
     * A step with action NONE accepts any input. On a match, clears any
     * active highlight and drop zone restriction, then advances to the
     * next step. Mismatched actions are ignored.
     *
     * @param action  The action performed by the player.
     */
    void handlePlayerAction(InputController::Action action);
    
    /**
     * Dismisses the current tutorial message on player tap.
     *
     * Only advances the tutorial if the current step is a SHOW_MESSAGE step
     * with no auto-dismiss delay. Does nothing if the tutorial is inactive,
     * the index is out of bounds, the current step is not SHOW_MESSAGE, or
     * the step has a positive delay (in which case it auto-advances via the
     * timer instead).
     */
    void dismissMessage();
    
private:
    /** The active game scene the tutorial operates on. */
    GameScene* _gameScene = nullptr;

    /** Asset manager used to retrieve tutorial instruction data. */
    std::shared_ptr<cugl::AssetManager> _assets;

    /** Ordered sequence of steps defining the tutorial. */
    std::vector<TutorialStep> _steps;

    /** Index of the currently executing step. */
    int _index = 0;

    /** Whether the tutorial is currently running. */
    bool _active = false;

    /** Countdown timer in seconds for delay-based steps. */
    float _timer = 0.0f;

    /** Whether the tutorial is blocked waiting for a player action to advance. */
    bool _waitingForAction = false;
    
    /**
     * Advances through tutorial steps until one yields control back to the
     * update loop or the sequence is exhausted.
     *
     * Calls executeStep() on each step in order. If executeStep() returns
     * true, the step requires time or player input to complete and control
     * is returned to the caller. If it returns false, the step completed
     * immediately and the next step is processed. Deactivates the tutorial
     * once all steps have been executed.
     */
    void advanceStep();
    
    /**
     * Executes a single tutorial step, applying its effects to the game scene.
     *
     * Before dispatching, resets per-step state (timer and action-wait flag)
     * and applies the step's bossActive value to the scene if present.
     *
     * Step behavior by type:
     * - SHOW_MESSAGE:    Displays dialogue text for delay > 0.
     * - SPAWN_ITEM:      Spawns a tutorial item by defId and pass direction.
     *                    Completes immediately.
     * - WAIT_FOR_ACTION: Displays dialogue, highlights the relevant drop zone,
     *                    and restricts input until the expected action is performed.
     * - BOSS_ATTACK:     Triggers a boss attack on the target index.
     *                    Completes immediately.
     * - BOSS_DEFEND:     Triggers a boss defense on the target index, optionally
     *                    showing dialogue and waiting for a delay.
     * - END:             Hides dialogue, clears highlights, re-enables the boss,
     *                    and deactivates the tutorial.
     * - UNKNOWN/default: Hides dialogue and completes immediately.
     *
     * @param step  The tutorial step to execute.
     *
     * @return true if the step requires time or player input to complete
     *         (i.e. the caller should yield control), false if the step
     *         completed immediately and the next step should be processed.
     */
    bool executeStep(const TutorialStep& step);
     
    /**
     * Highlights the drop zone in the game scene corresponding to the given action.
     *
     * Maps each action to its associated highlight zone name
     * Does nothing if the game scene is null.
     *
     * @param action  The action whose corresponding zone should be highlighted.
     */
    void applyZoneHighlight(InputController::Action action);
    
    /**
     * Parses a JSON object into the internal tutorial step sequence.
     *
     * @param json  The JSON value containing the list of tutorial steps.
     */
    void parseSteps(const std::shared_ptr<cugl::JsonValue>& json);

    /**
     * Converts a string token to its corresponding StepType enum value.
     *
     * @param str  The string token to convert.
     * @return    The matching StepType, or a default if unrecognized.
     */
    StepType parseStepType(const std::string& str) const;

    /**
     * Converts a string token to its corresponding InputController::Action enum value.
     *
     * @param str  The string token to convert.
     * @return    The matching Action, or a default if unrecognized.
     */
    InputController::Action parseAction(const std::string& str) const;
};

#endif /* __TUTORIAL_CONTROLLER_H__ */
