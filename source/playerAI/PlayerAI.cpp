#include "PlayerAI.h"

/**
 * Initializes shared AI parameters from the JSON config.
 * Subclasses should call this first in their own init() override.
 */
bool PlayerAI::init(const ItemDatabase& db, const std::string& path) {
    _db = &db;
    _thinkTimer = 0.0f;

    auto reader = cugl::JsonReader::alloc(path);
    if (!reader) {
        CULogError("PlayerAI::init — failed to open %s", path.c_str());
        return false;
    }

    auto config = reader->readJson();
    if (!config) {
        CULogError("PlayerAI::init — failed to parse %s", path.c_str());
        return false;
    }

    bool valid = true;

    if (config->has("thinkInterval") && config->get("thinkInterval")->isNumber()) {
        _thinkInterval = config->getFloat("thinkInterval");
    } else {
        CULogError("PlayerAI::init — missing or invalid 'thinkInterval' in %s", path.c_str());
        valid = false;
    }

    if (config->has("aggressionWeight") && config->get("aggressionWeight")->isNumber()) {
        _aggressionWeight = config->getFloat("aggressionWeight");
    } else {
        CULogError("PlayerAI::init — missing or invalid 'aggressionWeight' in %s", path.c_str());
        valid = false;
    }

    if (config->has("supportWeight") && config->get("supportWeight")->isNumber()) {
        _supportWeight = config->getFloat("supportWeight");
    } else {
        CULogError("PlayerAI::init — missing or invalid 'supportWeight' in %s", path.c_str());
        valid = false;
    }

    if (config->has("healThreshold") && config->get("healThreshold")->isNumber()) {
        _healThreshold = config->getFloat("healThreshold");
    } else {
        CULogError("PlayerAI::init — missing or invalid 'healThreshold' in %s", path.c_str());
        valid = false;
    }

    return valid;
}

/**
 * Advances the FSM by one frame. Shared across all difficulty levels —
 * only evaluate() and the action handlers differ between subclasses.
 */
void PlayerAI::update(float dt, Enemy& enemy, ItemController& items) {
    _thinkTimer += dt;
    if (_thinkTimer < _thinkInterval) return;
    _thinkTimer = 0.0f;
    
    CULog("[PlayerAI '%s'] inventory size=%d hp=%.1f/%.1f",
              getPlayerName().c_str(),
              (int)getInventory().size(),
              getCurrentHealth(),
              getMaxHealth());

    _state = evaluate(enemy);

    switch (_state) {
        case State::ATTACK:
            CULog("[PlayerAI '%s'] state → ATTACK", getPlayerName().c_str());
            actAttack(enemy, items);
            break;
        case State::SUPPORT:
            CULog("[PlayerAI '%s'] state → SUPPORT", getPlayerName().c_str());
            actSupport(items);
            break;
        case State::PASS:
            CULog("[PlayerAI '%s'] state → PASS", getPlayerName().c_str());
            actPass();
            break;
        case State::IDLE:
            CULog("[PlayerAI '%s'] state → IDLE (no inventory)", getPlayerName().c_str());
            break;
        default:
            break;
    }
}
