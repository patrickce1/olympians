#include "TutorialController.h"
#include "scenes/GameScene.h"
#include "SavedDataManager.h"

using namespace cugl;

#pragma mark - Lifecycle

/**
 * Initializes the TutorialController with a game scene and asset manager.
 *
 * @param scene   A pointer to the active GameScene. Must not be null.
 * @param assets  The shared asset manager used to load tutorial resources. Must not be null.
 *
 * @return true if initialization succeeded, false if either parameter is null.
 */
bool TutorialController::init(GameScene* scene, const std::shared_ptr<cugl::AssetManager>& assets) {
    if (!scene || !assets) return false;
    _gameScene = scene;
    _assets = assets;
    loadInstructionsFromJson();
    return true;
}

/**
 * Releases all resources held by the TutorialController and resets it to
 * its default state.
 *
 * Clears the tutorial step sequence, nullifies scene and asset references,
 * and resets all playback state including the current index, timer, and
 * action-wait flag.
 */
void TutorialController::dispose() {
    _steps.clear();
    _gameScene = nullptr;
    _assets = nullptr;
    _index = 0;
    _active = false;
    _timer = 0.0f;
    _waitingForAction = false;
}

/**
 * Loads tutorial instructions from the "tutorial" JSON asset.
 *
 * Retrieves the JSON value keyed "tutorial" from the asset manager,
 * parses it into the internal step sequence via parseSteps(), and
 * logs the number of steps loaded. Does nothing if the asset manager
 * is null or the asset is not found.
 */
void TutorialController::loadInstructionsFromJson() {
    if (!_assets) return;
    auto json = _assets->get<JsonValue>("tutorial");
    if (!json) return;
    parseSteps(json);
    CULog("Tutorial: loaded %zu steps from assets", _steps.size());
}

/**
 * Loads tutorial instructions from a JSON file at the given path.
 *
 * @param path  The file path to the JSON tutorial definition.
 *
 * @return true if the file was successfully read and parsed, false if
 *         the file could not be opened or contained invalid JSON.
 */
bool TutorialController::loadFromFile(const std::string& path) {
    auto reader = cugl::JsonReader::alloc(path);
    if (!reader) return false;
    auto json = reader->readJson();
    if (!json) return false;
    parseSteps(json);
    CULog("Tutorial: loaded %zu steps from '%s'", _steps.size(), path.c_str());
    return true;
}

/**
 * Starts the tutorial from the beginning.
 *
 * Resets all playback state, deactivates the boss in the game scene,
 * and advances to the first step. Does nothing if the step sequence
 * is empty.
 */
void TutorialController::start() {
    if (_steps.empty()) return;
    _active = true;
    _index = 0;
    _timer = 0.0f;
    _waitingForAction = false;
    
    if (_gameScene) {
        _gameScene->setTutorialBossActive(false);
    }
    CULog("Tutorial: started with %zu steps", _steps.size());
    advanceStep();
}

#pragma mark - Parsing

/**
 * Converts a string token to its corresponding StepType enum value.
 *
 * @param str  The string to parse. Expected values are "show_message",
 *             "wait_for_action", "spawn_item", "boss_attack", "boss_defend",
 *             "delay", and "end".
 *
 * @return The matching StepType, or StepType::UNKNOWN if the string is
 *         not recognized.
 */
StepType TutorialController::parseStepType(const std::string& str) const {
    if (str == "show_message")    return StepType::SHOW_MESSAGE;
    if (str == "wait_for_action") return StepType::WAIT_FOR_ACTION;
    if (str == "spawn_item")      return StepType::SPAWN_ITEM;
    if (str == "boss_attack")     return StepType::BOSS_ATTACK;
    if (str == "boss_defend")     return StepType::BOSS_DEFEND;
    if (str == "delay")           return StepType::DELAY;
    if (str == "end")             return StepType::END;
    CULog("Tutorial: unknown step type '%s'", str.c_str());
    return StepType::UNKNOWN;
}

/**
 * Converts a string token to its corresponding InputController::Action enum value.
 *
 * @param str  The string to parse. Expected values are "DROP_BOSS",
 *             "DROP_ALLY_LEFT", "DROP_ALLY_RIGHT", "PASS_LEFT", "PASS_RIGHT",
 *             and "HOLD_FOR_TOOLTIP".
 *
 * @return The matching Action, or InputController::Action::NONE if the
 *         string is not recognized.
 */
InputController::Action TutorialController::parseAction(const std::string& str) const {
    if (str == "DROP_BOSS")       return InputController::Action::DROP_BOSS;
    if (str == "DROP_ALLY_LEFT")  return InputController::Action::DROP_ALLY_LEFT;
    if (str == "DROP_ALLY_RIGHT") return InputController::Action::DROP_ALLY_RIGHT;
    if (str == "PASS_LEFT")       return InputController::Action::PASS_LEFT;
    if (str == "PASS_RIGHT")      return InputController::Action::PASS_RIGHT;
    if (str == "HOLD_FOR_TOOLTIP") return InputController::Action::HOLD_FOR_TOOLTIP;
    return InputController::Action::NONE;
}

/**
 * Parses a JSON object into the internal tutorial step sequence.
 *
 * Expects a JSON object with a "steps" array, where each element is an
 * object that may contain the following fields:
 *
 *   - "type"          (string)  Step type; see parseStepType().
 *   - "text"          (string)  Message text to display.
 *   - "action"        (string)  Required player action; see parseAction().
 *   - "defId"         (string)  Definition ID for spawned items.
 *   - "delay"         (float)   Duration in seconds for delay steps.
 *   - "passDirection" (int)     Direction of a pass action.
 *   - "bossActive"    (bool)    Whether the boss should be active.
 *   - "bossTarget"    (int)     Target index for boss behavior.
 *
 * Clears any previously loaded steps before parsing. Silently skips
 * malformed or non-object array elements.
 *
 * @param json  The root JSON value containing the "steps" array.
 */
void TutorialController::parseSteps(const std::shared_ptr<JsonValue>& json) {
    _steps.clear();
    if (!json || !json->isObject()) return;
    auto arr = json->get("steps");
    if (!arr || !arr->isArray()) return;

    for (int i = 0; i < arr->size(); ++i) {
        auto val = arr->get(i);
        if (!val || !val->isObject()) continue;

        TutorialStep step;
        if (val->has("type"))          step.type          = parseStepType(val->getString("type"));
        if (val->has("text"))          step.text          = val->getString("text");
        if (val->has("action"))        step.action        = parseAction(val->getString("action"));
        if (val->has("defId"))         step.defId         = val->getString("defId");
        if (val->has("delay"))         step.delay         = val->getFloat("delay");
        if (val->has("passDirection")) step.passDirection = val->getInt("passDirection");
        if (val->has("bossActive"))    step.bossActive    = val->getBool("bossActive");
        if (val->has("bossTarget"))    step.bossTarget    = val->getInt("bossTarget");

        _steps.push_back(step);
    }
}

#pragma mark - State

/**
 * Returns whether the given action would satisfy the current wait condition.
 *
 * @param action  The action to check against the current step's requirement.
 *
 * @return true if the tutorial is active, waiting for an action, and the
 *         current step accepts NONE (any action) or matches the given action.
 *         False otherwise.
 */
bool TutorialController::isWaitingForActionMatch(InputController::Action action) const {
    
    if (!_active || !_waitingForAction) return false;
    
    if (_index < 0 || _index >= (int)_steps.size()) return false;
    
    const TutorialStep& step = _steps[_index];
    return step.action == InputController::Action::NONE || step.action == action;
}

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
void TutorialController::update(float dt) {
    if (!_active || _index < 0 || _index >= (int)_steps.size()) return;
    
    // wait_for_action steps never auto-advance — only handlePlayerAction() can advance them.
    if (_waitingForAction) {
        
        return;
    }
    
    //Wait until the time defined is up
    if (_timer > 0.0f) {
        _timer = std::max(0.0f, _timer - dt);
        if (_timer <= 0.0f) {
            _index++;
            advanceStep();
        }
    }
}

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
void TutorialController::handlePlayerAction(InputController::Action action) {
    CULog("Tutorial: handlePlayerAction called action=%d active=%d waiting=%d expected=%d",
        (int)action, _active ? 1 : 0, _waitingForAction ? 1 : 0, (int)_steps[_index].action);

    if (!_active || !_waitingForAction) return;
    
    const TutorialStep& step = _steps[_index];

    // Empty action means any action advances.
    if (step.action == InputController::Action::NONE || step.action == action) {
        CULog("Tutorial: action matched, advancing");
        if (_gameScene){
            _gameScene->clearTutorialHighlight();
            _gameScene->setTutorialAllowedDropZone(InputController::Action::NONE);
        }
        _waitingForAction = false;
        _index++;
        advanceStep();
    } else {
        CULog("Tutorial: wrong action, still waiting");
    }
}

/**
 * Dismisses the current tutorial message on player tap.
 *
 * Only advances the tutorial if the current step is a SHOW_MESSAGE step
 * with no auto-dismiss delay. Does nothing if the tutorial is inactive,
 * the index is out of bounds, the current step is not SHOW_MESSAGE, or
 * the step has a positive delay (in which case it auto-advances via the
 * timer instead).
 */
void TutorialController::dismissMessage() {
    // Inactive controller
    if (!_active) return;
    
    // Instruction out of bound
    if (_index < 0 || _index >= (int)_steps.size()) return;
    
    const TutorialStep& step = _steps[_index];
    
    // Only Steps that are SHOW_MESSAGE should be dismissible.
    if (step.type != StepType::SHOW_MESSAGE) return;
    
    // Only steps that had delays of 0.0 can be tapped to dismiss.
    if (step.delay > 0.0f) return;
    CULog("Tutorial: message dismissed by tap");
    _index++;
    advanceStep();
}

#pragma mark - Step Execution

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
void TutorialController::advanceStep() {
    while (_active && _index >= 0 && _index < (int)_steps.size()) {
        const TutorialStep& step = _steps[_index];
        
        if (executeStep(step)) return;
        _index++;
    }
    _active = false;
}

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
bool TutorialController::executeStep(const TutorialStep& step) {
    // Reset per-step state.
    _waitingForAction = false;
    _timer = 0.0f;
    
    // Apply the Boss State globally for the duration of this step
        if (_gameScene && step.bossActive.has_value()) {
            _gameScene->setTutorialBossActive(*step.bossActive);
            CULog("Tutorial: Setting Boss Active to %s", step.bossActive ? "TRUE" : "FALSE");
        }

    switch (step.type){
        case StepType::SHOW_MESSAGE:
            if (_gameScene && !step.text.empty()) _gameScene->showDialogue(step.text);
            if (step.delay > 0.0f) _timer = step.delay;
            return true;
        
        case StepType::SPAWN_ITEM:
            if (_gameScene) _gameScene->spawnTutorialItem(step.defId, step.passDirection);
            return false;
            
        case StepType::WAIT_FOR_ACTION:
            _waitingForAction = true;
            if (_gameScene){
                if (!step.text.empty()){
                    _gameScene->showDialogue(step.text);
                }
                applyZoneHighlight(step.action);
                _gameScene->setTutorialAllowedDropZone(step.action);
            }
            return true;
        case StepType::BOSS_ATTACK:
            if (_gameScene){
                int targetIndex = step.bossTarget.value_or(0);
                _gameScene->triggerBossAttack(targetIndex);
            }
            return false;
        case StepType::BOSS_DEFEND:
            if (_gameScene){
                int targetIndex = step.bossTarget.value_or(0);
                _gameScene->triggerBossDefense(targetIndex);
                if (!step.text.empty()){
                    _gameScene->showDialogue(step.text);
                }
            }
            if (step.delay > 0.0f) _timer = step.delay;
                return true;
        case StepType::END:
            if (_gameScene) {
                _gameScene->hideDialogue();
                _gameScene->setTutorialHighlight("none");
                CULog("Tutorial: END step reached — re-enabling boss");
                _gameScene->setTutorialBossActive(true);
            }
            _active = false;
            return true;
            
        default:
            if (_gameScene) _gameScene->hideDialogue();
            break;
    }
    return false;
}

/**
 * Highlights the drop zone in the game scene corresponding to the given action.
 *
 * Maps each action to its associated highlight zone name
 *
 * Does nothing if the game scene is null.
 *
 * @param action  The action whose corresponding zone should be highlighted.
 */
void TutorialController::applyZoneHighlight(InputController::Action action) {
    if (!_gameScene) {
            CULog("Tutorial Error: _gameScene is NULL in applyZoneHighlight");
            return;
        }
    CULog("Tutorial: applyZoneHighlight called with Action Enum ID: %d", (int)action);
    
    if (!_gameScene) return;
    switch (action) {
        case InputController::Action::PASS_LEFT:
            _gameScene->setTutorialHighlight("pass_left");
            break;
        case InputController::Action::PASS_RIGHT:
            _gameScene->setTutorialHighlight("pass_right");
            break;
        case InputController::Action::DROP_ALLY_LEFT:
            _gameScene->setTutorialHighlight("left_support");
            break;
        case InputController::Action::DROP_ALLY_RIGHT:
            _gameScene->setTutorialHighlight("right_support");
            break;
        case InputController::Action::DROP_BOSS:
            _gameScene->setTutorialHighlight("attack");
            break;
        case InputController::Action::HOLD_FOR_TOOLTIP:
        case InputController::Action::NONE:
        case InputController::Action::DROP_INVALID:
        default:
            _gameScene->setTutorialHighlight("none");
            break;
    }
}
