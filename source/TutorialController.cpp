#include "TutorialController.h"
#include "GameScene.h"
#include "InputController.h"

using namespace cugl;

#pragma mark - Lifecycle

bool TutorialController::init(GameScene* scene, const std::shared_ptr<cugl::AssetManager>& assets) {
    if (!scene || !assets) return false;
    _gameScene = scene;
    _assets = assets;
    loadInstructionsFromJson();
    return true;
}

void TutorialController::dispose() {
    _steps.clear();
    _gameScene = nullptr;
    _assets = nullptr;
    _index = 0;
    _active = false;
    _timer = 0.0f;
    _waitingForAction = false;
}

void TutorialController::loadInstructionsFromJson() {
    if (!_assets) return;
    auto json = _assets->get<JsonValue>("tutorial");
    if (!json) return;
    parseSteps(json);
    CULog("Tutorial: loaded %zu steps from assets", _steps.size());
}

bool TutorialController::loadFromFile(const std::string& path) {
    auto reader = cugl::JsonReader::alloc(path);
    if (!reader) return false;
    auto json = reader->readJson();
    if (!json) return false;
    parseSteps(json);
    CULog("Tutorial: loaded %zu steps from '%s'", _steps.size(), path.c_str());
    return true;
}

void TutorialController::start() {
    if (_steps.empty()) return;
    _active = true;
    _index = 0;
    _timer = 0.0f;
    _waitingForAction = false;
    CULog("Tutorial: started with %zu steps", _steps.size());
    advanceStep();
}

void TutorialController::stop() {
    _active = false;
}

#pragma mark - Parsing

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

ZoneType TutorialController::parseZoneType(const std::string& str) const {
    if (str == "left_support")  return ZoneType::LEFT_SUPPORT;
    if (str == "right_support") return ZoneType::RIGHT_SUPPORT;
    if (str == "attack")        return ZoneType::ATTACK;
    return ZoneType::NONE;
}

InputController::Action TutorialController::parseAction(const std::string& str) const {
    if (str == "DROP_BOSS")       return InputController::Action::DROP_BOSS;
    if (str == "DROP_ALLY_LEFT")  return InputController::Action::DROP_ALLY_LEFT;
    if (str == "DROP_ALLY_RIGHT") return InputController::Action::DROP_ALLY_RIGHT;
    if (str == "PASS_LEFT")       return InputController::Action::PASS_LEFT;
    if (str == "PASS_RIGHT")      return InputController::Action::PASS_RIGHT;
    return InputController::Action::NONE;
}

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
        if (val->has("zone"))          step.zone          = parseZoneType(val->getString("zone"));
        if (val->has("action"))        step.action        = parseAction(val->getString("action"));
        if (val->has("defId"))         step.defId         = val->getString("defId");
        if (val->has("delay"))         step.delay         = val->getFloat("delay");
        if (val->has("passDirection")) step.passDirection = val->getInt("passDirection");
        if (val->has("bossActive"))    step.bossActive    = val->getBool("bossActive");
        if (val->has("bossTarget"))    step.bossTarget    = val->getInt("bossTarget");

        _steps.push_back(step);
    }
}

#pragma mark - Update
void TutorialController::update(float dt) {
    if (!_active || _index < 0 || _index >= (int)_steps.size()) return;
    
    // wait_for_action steps never auto-advance — only onAction() can advance them.
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
void TutorialController::onAction(InputController::Action action) {
    if (!_active || !_waitingForAction) return;
    const TutorialStep& step = _steps[_index];

    // Empty action means any action advances.
    if (step.action == InputController::Action::NONE || step.action == action) {
        CULog("Tutorial: action matched, advancing");
        _waitingForAction = false;
        _index++;
        advanceStep();
    } else {
        CULog("Tutorial: wrong action, still waiting");
    }
}

void TutorialController::dismissMessage() {
    //Inactive controller
    if (!_active) return;
    
    //Instruction out of bound
    if (_index < 0 || _index >= (int)_steps.size()) return;
    
    const TutorialStep& step = _steps[_index];
    
    //Only Steps that are SHOW_MESSAGE should be dismissible.
    if (step.type != StepType::SHOW_MESSAGE) return;
    
    //Only steps that had delays of 0.0 can be tapped to dismiss.
    if (step.delay > 0.0f) return;
    CULog("Tutorial: message dismissed by tap");
    _index++;
    advanceStep();
}
