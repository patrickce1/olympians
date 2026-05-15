#include "PlayerAI.h"

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

/**
 * Initializes the AI controller from the "playerAI" block in playerAI.json.
 *
 * Loads thinkInterval min/max, healThreshold min/max, rarePassChance, and
 * divinePassChance. Derives initial runtime values via applyDecisionMultiplier()
 * using the current _decisionMultiplier (default 0.0 = easiest).
 *
 * @param db    Read-only reference to the item database for item lookups.
 * @param path  Path to the playerAI.json config file.
 * @return true if all required fields were loaded successfully, false otherwise.
 */
bool PlayerAI::init(const ItemDatabase& db, const std::string& path) {
    auto reader = cugl::JsonReader::alloc(path);
    if (!reader) {
        if (_debug) CULogError("PlayerAI::init — failed to open %s", path.c_str());
        return false;
    }

    auto json = reader->readJson();
    if (!json) {
        if (_debug) CULogError("PlayerAI::init — failed to parse %s", path.c_str());
        return false;
    }

    auto config = json->get("playerAI");
    if (!config) {
        if (_debug) CULogError("PlayerAI::init — missing 'playerAI' block in %s", path.c_str());
        return false;
    }

    _db = &db;
    _thinkTimer = 0.0f;
    bool valid = true;

    if (config->has("thinkIntervalMin") && config->get("thinkIntervalMin")->isNumber()) {
        _thinkIntervalMin = config->getFloat("thinkIntervalMin");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'thinkIntervalMin'");
        valid = false;
    }

    if (config->has("thinkIntervalMax") && config->get("thinkIntervalMax")->isNumber()) {
        _thinkIntervalMax = config->getFloat("thinkIntervalMax");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'thinkIntervalMax'");
        valid = false;
    }

    if (config->has("healThresholdMin") && config->get("healThresholdMin")->isNumber()) {
        _healThresholdMin = config->getFloat("healThresholdMin");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'healThresholdMin'");
        valid = false;
    }

    if (config->has("healThresholdMax") && config->get("healThresholdMax")->isNumber()) {
        _healThresholdMax = config->getFloat("healThresholdMax");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'healThresholdMax'");
        valid = false;
    }

    if (config->has("rarePassChance") && config->get("rarePassChance")->isNumber()) {
        _rarePassChance = config->getFloat("rarePassChance");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'rarePassChance'");
        valid = false;
    }

    if (config->has("divinePassChance") && config->get("divinePassChance")->isNumber()) {
        _divinePassChance = config->getFloat("divinePassChance");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'divinePassChance'");
        valid = false;
    }

    if (valid) applyDecisionMultiplier();
    return valid;
}

// ---------------------------------------------------------------------------
// Decision multiplier
// ---------------------------------------------------------------------------

/**
 * Sets the AI difficulty multiplier and re-derives all runtime parameters.
 *
 * @param multiplier  Difficulty in [0, 1]. Clamped to that range.
 */
void PlayerAI::setDecisionMultiplier(float multiplier) {
    _decisionMultiplier = std::max(0.0f, std::min(1.0f, multiplier));
    applyDecisionMultiplier();
}

/**
 * Re-derives all runtime parameters from _decisionMultiplier.
 *
 * thinkInterval is interpolated from _thinkIntervalMax (worst/slowest) down
 * to _thinkIntervalMin (best/fastest), so a higher multiplier means the AI
 * acts more often.
 *
 * healThreshold is interpolated from _healThresholdMin (worst) to
 * _healThresholdMax (best), so a higher multiplier means the AI heals earlier.
 *
 * attackWeight and supportWeight are interpolated from a flat 0.5/0.5 split
 * toward the house's own attack/support ratio from houses.json. If no house
 * multipliers are available (e.g. no house assigned), weights stay at 0.5/0.5.
 *
 * effectiveRarePassChance and effectiveDivinePassChance are scaled linearly
 * from 0 to their configured maximums by _decisionMultiplier.
 */
void PlayerAI::applyDecisionMultiplier() {
    // thinkInterval: higher multiplier = shorter interval (faster AI)
    // Interpolates from max (slow/worst) down to min (fast/best)
    _thinkInterval = _thinkIntervalMax + _decisionMultiplier * (_thinkIntervalMin - _thinkIntervalMax);

    // healThreshold: higher multiplier = higher threshold (heals earlier)
    _healThreshold = _healThresholdMin + _decisionMultiplier * (_healThresholdMax - _healThresholdMin);

    // houseTypeWisdom: interpolate attack/support weights from 0.5/0.5
    // toward the house's own ratios. Falls back to 0.5/0.5 if no house is set.
    float houseAttack  = 0.5f;
    float houseSupport = 0.5f;
    if (_db) {
        const auto* mults = _db->getHouseMultipliers(getHouseName());
        if (mults) {
            // attack and support on HouseMultipliers are sliders in [0,1].
            // Normalize so they sum to 1.0 for use as weights.
            float total = mults->attack + mults->support;
            if (total > 0.0f) {
                houseAttack  = mults->attack  / total;
                houseSupport = mults->support / total;
            }
        }
    }
    _attackWeight  = 0.5f + _decisionMultiplier * (houseAttack  - 0.5f);
    _supportWeight = 0.5f + _decisionMultiplier * (houseSupport - 0.5f);

    // rarityWisdom: scale pass chances linearly from 0 at worst to full at best
    _effectiveRarePassChance   = _decisionMultiplier * _rarePassChance;
    _effectiveDivinePassChance = _decisionMultiplier * _divinePassChance;

    if (_debug) CULog(
        "[PlayerAI '%s'] multiplier=%.2f → interval=%.2f heal=%.2f "
        "atk=%.2f sup=%.2f rarePass=%.2f divinePass=%.2f",
        getPlayerName().c_str(), _decisionMultiplier,
        _thinkInterval, _healThreshold,
        _attackWeight, _supportWeight,
        _effectiveRarePassChance, _effectiveDivinePassChance);
}

// ---------------------------------------------------------------------------
// Ownership tracking
// ---------------------------------------------------------------------------

/**
 * Records an item ID as originally spawned for this AI.
 * Must NOT be called for items received via a pass.
 *
 * @param itemId  The ItemId of the newly spawned item to mark as owned.
 */
void PlayerAI::onItemSpawned(ItemInstance::ItemId itemId) {
    _ownedItemIds.insert(itemId);
}

// ---------------------------------------------------------------------------
// Update (FSM loop)
// ---------------------------------------------------------------------------

/**
 * Steps the AI forward by one frame.
 *
 * @param dt     Time elapsed since the last frame in seconds.
 * @param enemy  The current enemy.
 * @param items  The ItemController.
 */
void PlayerAI::update(float dt, Enemy& enemy, ItemController& items) {
    _thinkTimer += dt;
    if (_thinkTimer < _thinkInterval) return;
    _thinkTimer = 0.0f;

    if (_debug) CULog("[PlayerAI '%s'] inventory=%d hp=%.1f/%.1f multiplier=%.2f",
          getPlayerName().c_str(),
          (int)getInventory().size(),
          getCurrentHealth(), getMaxHealth(),
          _decisionMultiplier);

    _state = evaluate(enemy);

    // Dead AI cannot attack or support — fall through to pass or idle
    if (!isAlive() && (_state == State::ATTACK || _state == State::SUPPORT)) {
        _state = (rand() % 2 == 0) ? State::PASS : State::IDLE;
    }

    switch (_state) {
        case State::ATTACK:
            if (!isAlive()) break;
            if (_debug) CULog("[PlayerAI '%s'] state → ATTACK", getPlayerName().c_str());
            actAttack(enemy, items);
            break;
        case State::SUPPORT:
            if (!isAlive()) break;
            if (_debug) CULog("[PlayerAI '%s'] state → SUPPORT", getPlayerName().c_str());
            actSupport(items);
            break;
        case State::PASS:
            if (_debug) CULog("[PlayerAI '%s'] state → PASS", getPlayerName().c_str());
            actPass();
            break;
        case State::IDLE:
            if (_debug) CULog("[PlayerAI '%s'] state → IDLE", getPlayerName().c_str());
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Returns whether the AI has at least one attack item in its inventory.
 *
 * @return true if any inventory item has type ItemDef::Type::Attack.
 */
bool PlayerAI::canAttack() const {
    for (const ItemInstance& item : getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Attack) return true;
    }
    return false;
}

/**
 * Returns whether the AI has a support item AND at least one neighbor is
 * alive and below _healThreshold.
 *
 * @return true if a support item exists and a valid heal target is available.
 */
bool PlayerAI::canSupport() const {
    bool hasSupportItem = false;
    for (const ItemInstance& item : getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Support) {
            hasSupportItem = true;
            break;
        }
    }
    if (!hasSupportItem) return false;

    auto needsHeal = [&](Player* p) {
        return p && p->isAlive() &&
               p->getCurrentHealth() / p->getMaxHealth() < _healThreshold;
    };
    return needsHeal(getLeftPlayer()) || needsHeal(getRightPlayer());
}

// ---------------------------------------------------------------------------
// evaluate
// ---------------------------------------------------------------------------

/**
 * Evaluates game context and returns the state the AI should transition to.
 *
 * Priority order:
 *   1. Forced heal override — if any neighbor is below _healThreshold and a
 *      support item exists, return SUPPORT immediately.
 *   2. Rarity-pass check — scan for unowned divine items (divinePassChance),
 *      then unowned rare items (rarePassChance). First hit sets
 *      _pendingPassItemId and returns PASS. Owned items are skipped.
 *   3. Weighted random roll — between ATTACK (_attackWeight) and SUPPORT
 *      (_supportWeight). Returns PASS if neither is viable, IDLE if empty.
 *
 * @param enemy  The current enemy.
 * @return The State the AI should transition to this think cycle.
 */
PlayerAI::State PlayerAI::evaluate(const Enemy& enemy) {
    _pendingPassItemId = 0;

    if (getInventory().empty()) return State::IDLE;

    // --- 1. Forced heal override ---
    // If a neighbor is critically injured and we have a support item, heal
    // immediately regardless of the weighted roll.
    if (canSupport()) {
        auto isCritical = [&](Player* p) {
            return p && p->isAlive() &&
                   p->getCurrentHealth() / p->getMaxHealth() < _healThreshold;
        };
        if (isCritical(getLeftPlayer()) || isCritical(getRightPlayer())) {
            if (_debug) CULog("[PlayerAI '%s'] evaluate — forced heal override", getPlayerName().c_str());
            return State::SUPPORT;
        }
    }

    // --- 2. Rarity-pass check ---
    bool hasAliveNeighbor = (getLeftPlayer()  && getLeftPlayer()->isAlive()) ||
                            (getRightPlayer() && getRightPlayer()->isAlive());

    if (hasAliveNeighbor) {
        // Divine check first (higher priority / higher pass chance)
        for (const ItemInstance& item : getInventory()) {
            auto def = _db->getDef(item.getDefId());
            if (!def || def->getRarity() != ItemDef::Rarity::Divine) continue;
            if (_ownedItemIds.count(item.getId())) continue; // owned — always use

            float roll = static_cast<float>(rand()) / RAND_MAX;
            if (roll < _effectiveDivinePassChance) {
                if (_debug) CULog("[PlayerAI '%s'] evaluate — divine pass triggered (roll=%.2f chance=%.2f)",
                      getPlayerName().c_str(), roll, _effectiveDivinePassChance);
                _pendingPassItemId = item.getId();
                return State::PASS;
            }
        }

        // Rare check second
        for (const ItemInstance& item : getInventory()) {
            auto def = _db->getDef(item.getDefId());
            if (!def || def->getRarity() != ItemDef::Rarity::Rare) continue;
            if (_ownedItemIds.count(item.getId())) continue; // owned — always use

            float roll = static_cast<float>(rand()) / RAND_MAX;
            if (roll < _effectiveRarePassChance) {
                if (_debug) CULog("[PlayerAI '%s'] evaluate — rare pass triggered (roll=%.2f chance=%.2f)",
                      getPlayerName().c_str(), roll, _effectiveRarePassChance);
                _pendingPassItemId = item.getId();
                return State::PASS;
            }
        }
    }

    // --- 3. Weighted random roll ---
    if (!canAttack() && !canSupport()) return State::PASS;

    float totalWeight = 0.0f;
    if (canAttack())  totalWeight += _attackWeight;
    if (canSupport()) totalWeight += _supportWeight;

    float roll = static_cast<float>(rand()) / RAND_MAX * totalWeight;

    if (canAttack() && roll < _attackWeight) {
        return State::ATTACK;
    } else {
        return State::SUPPORT;
    }
}

// ---------------------------------------------------------------------------
// Action handlers
// ---------------------------------------------------------------------------

/**
 * Collects all attack items from inventory and uses a random one on the enemy.
 * Does nothing if no attack items are found.
 *
 * @param enemy  The enemy to attack.
 * @param items  The ItemController used to resolve the item action.
 */
void PlayerAI::actAttack(Enemy& enemy, ItemController& items) {
    std::vector<ItemInstance::ItemId> attackItems;
    for (const ItemInstance& item : getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Attack) {
            attackItems.push_back(item.getId());
        }
    }
    if (attackItems.empty()) {
        if (_debug) CULog("[PlayerAI '%s'] actAttack — no attack items, aborting", getPlayerName().c_str());
        return;
    }
    ItemInstance::ItemId chosen = attackItems[rand() % attackItems.size()];
    if (_debug) CULog("[PlayerAI '%s'] actAttack — using item %llu on '%s'",
          getPlayerName().c_str(), (unsigned long long)chosen, enemy.getId().c_str());
    useItemById(chosen, enemy, *_db);
}

/**
 * Finds the most injured neighbor below _healThreshold and uses a random
 * support item on them. Does nothing if no valid target or support item exists.
 *
 * @param items  The ItemController used to resolve the item action.
 */
void PlayerAI::actSupport(ItemController& items) {
    Player* target = nullptr;
    float lowestRatio = _healThreshold;

    auto check = [&](Player* p) {
        if (!p || !p->isAlive()) return;
        float ratio = p->getCurrentHealth() / p->getMaxHealth();
        if (ratio < lowestRatio) { lowestRatio = ratio; target = p; }
    };
    check(getLeftPlayer());
    check(getRightPlayer());

    if (!target) {
        if (_debug) CULog("[PlayerAI '%s'] actSupport — no neighbour below %.2f, aborting",
                getPlayerName().c_str(), _healThreshold);
        return;
    }

    std::vector<ItemInstance::ItemId> supportItems;
    for (const ItemInstance& item : getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Support) {
            supportItems.push_back(item.getId());
        }
    }
    if (supportItems.empty()) {
        if (_debug) CULog("[PlayerAI '%s'] actSupport — no support items, aborting", getPlayerName().c_str());
        return;
    }

    ItemInstance::ItemId chosen = supportItems[rand() % supportItems.size()];
    if (_debug) CULog("[PlayerAI '%s'] actSupport — using item %llu on '%s' (ratio=%.2f)",
          getPlayerName().c_str(), (unsigned long long)chosen,
          target->getPlayerName().c_str(), lowestRatio);
    useItemById(chosen, *target, *_db);
}

/**
 * Passes an item to a random alive neighbor (left or right).
 *
 * If _pendingPassItemId was set by evaluate(), that specific item is located
 * and passed, and its ID is removed from _ownedItemIds. Falls back to a random
 * item if the pending item is not found in inventory. Clears _pendingPassItemId
 * after resolving regardless of outcome. Does nothing if inventory is empty or
 * no alive neighbor exists.
 */
void PlayerAI::actPass() {
    if (getInventory().empty()) {
        if (_debug) CULog("[PlayerAI '%s'] actPass — inventory empty, aborting", getPlayerName().c_str());
        return;
    }

    std::vector<Player*> targets;
    if (getLeftPlayer()  && getLeftPlayer()->isAlive())  targets.push_back(getLeftPlayer());
    if (getRightPlayer() && getRightPlayer()->isAlive()) targets.push_back(getRightPlayer());

    if (targets.empty()) {
        if (_debug) CULog("[PlayerAI '%s'] actPass — no alive neighbours, aborting", getPlayerName().c_str());
        return;
    }

    // Resolve which item to pass — prefer the pending item set by evaluate()
    const ItemInstance* resolved = nullptr;
    if (_pendingPassItemId != 0) {
        for (const ItemInstance& item : getInventory()) {
            if (item.getId() == _pendingPassItemId) {
                resolved = &item;
                break;
            }
        }
        if (!resolved && _debug) {
            CULog("[PlayerAI '%s'] actPass — pending item %llu not found, falling back to random",
                  getPlayerName().c_str(), (unsigned long long)_pendingPassItemId);
        }
    }

    const auto& inventory = getInventory();
    ItemInstance itemToPass = resolved ? *resolved
                                       : inventory[rand() % inventory.size()];
    _pendingPassItemId = 0;

    // Item leaving this AI — remove from owned set
    _ownedItemIds.erase(itemToPass.getId());

    Player* chosenTarget = targets[rand() % targets.size()];

    int passDirection = 0;
    if (chosenTarget == getLeftPlayer())       passDirection = 1;
    else if (chosenTarget == getRightPlayer()) passDirection = 2;

    if (_debug) CULog("[PlayerAI '%s'] actPass — passing item %llu to '%s'",
          getPlayerName().c_str(), (unsigned long long)itemToPass.getId(),
          chosenTarget->getPlayerName().c_str());

    removeItemById(itemToPass.getId());
    itemToPass.setPassDirection(passDirection);
    itemToPass.setSlideOrigin(ItemInstance::SlideOriginType::SLIDE_FROM_PASS);
    chosenTarget->addItem(itemToPass);
}
