// TutorialController.h
#ifndef __TUTORIAL_CONTROLLER_H__
#define __TUTORIAL_CONTROLLER_H__

#include <cugl/cugl.h>
#include <optional>
#include "InputController.h"

class GameScene;


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

enum class ZoneType {
    LEFT_SUPPORT,
    RIGHT_SUPPORT,
    ATTACK,
    NONE
};
/**
 * The atomic step description parsed from a JSON.
 * Each field is optional aside from type, which defines what this step will do.
 */
struct TutorialStep {
    StepType type = StepType::UNKNOWN;
    std::string text;
    ZoneType zone = ZoneType::NONE;
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
     * Initialises the TutorialControler
     *
     * @return True if the TutorialControler loaded instructions correctly.
     */
    bool init(GameScene* scene, const std::shared_ptr<cugl::AssetManager>& assets);

    /**
     * Releases all resources held by this controller.
     *
     * Safe to call even if init() was never called.
     */
    void dispose();
    
    /** Load tutorial data from assets (called by iinit`). */
    void loadInstructionsFromJson();

    /**
     * Load tutorial data directly from a JSON file on disk using JsonReader.
     * Matches the pattern used by other loaders in the project (e.g. EnemyLoader).
     *
     * @param path  Filesystem path to the tutorial JSON file
     * @return true on success
     */
    bool loadFromFile(const std::string& path);

    /** Start the tutorial sequence. Inactive until this is called. */
    void start();

    /** Stop the tutorial immediately. */
    void stop();
    
#pragma mark - State

    /** True if tutorial is currently running. */
    bool isActive() const { return _active; }

    /** Current step index (for debugging). */
    int getIndex() const { return _index; }

    /** Current timer value (for debugging). */
    float getTimer() const { return _timer; }

    /** True if waiting for player action (for debugging). */
    bool isWaiting() const { return _waitingForAction; }
    
    /** True if the current `show_message` step is dismissible by tap. */
    bool isCurrentMessageDismissible() const;
    
    /** True if the current step is a show_message`. */
    bool isCurrentStepShowMessage() const;
    
    /** Returns true if currently waiting for a specific action and that action matches `action`. */
    bool isWaitingForActionMatch(InputController::Action action) const;

#pragma mark - Update
    /** Update tick; call from `GameScene::update`. */
    void update(float dt);

    /**
     * Allows the tutorial to optionally intercept and consume raw input.
     * Return true to indicate the tutorial consumed the input (GameScene
     * should skip normal input processing this frame).
     */
    bool handleTutorialInputIntercept(InputController& input);

#pragma mark - Input
    /**
     * Notify the controller that the player performed `action`.
     * This is used to advance `wait_for_action` steps. Caller should pass
     * the `InputController::Action` value observed by the scene.
     *
     * If wrong action performed, redo the sequeuence (NOTE TO SELF)
     */
    void onAction(InputController::Action action);
    
    /** Dismiss the current message step (user tapped). */
    void dismissMessage();
    
private:
    //The current gameScene for the tutorial.
    GameScene* _gameScene = nullptr;
    
    //A pointer to the assetmanager in order to retrieve instructions
    std::shared_ptr<cugl::AssetManager> _assets;
    
    //A vector of all the ioncoming instructions.
    std::vector<TutorialStep> _steps;
    
    //Current step index
    int _index = 0;
    
    //Whether the current sequence of instructions is being run
    bool _active = false;
    
    //Countdown for delay steps
    float _timer = 0.0f;
    
    //True if we are waiting for the player to perform an action in order to advance.
    bool _waitingForAction = false;
    
    //increments the step index and prepares the next instruction
    void advanceStep();
    
    //Loads the steps defined by the asset into the _steps vector.
    void parseSteps(const std::shared_ptr<cugl::JsonValue>& json);
    
    //Translates a string into the matching StepType
    StepType parseStepType(const std::string& str) const;
    
    //Translates a string into the matching ZoneType
    ZoneType parseZoneType(const std::string& str) const;
    
    //Translates a string into the matching Action
    InputController::Action parseAction(const std::string& str) const;
};

#endif /* __TUTORIAL_CONTROLLER_H__ */
