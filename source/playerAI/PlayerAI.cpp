#include "PlayerAI.h"

static constexpr float HOUSE_WEIGHT_MIN = 0.3f;
static constexpr float HOUSE_WEIGHT_MAX = 0.7f;

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
    _pendingForgeChances.clear();
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

    if (config->has("rarePassChanceMin") && config->get("rarePassChanceMin")->isNumber()) {
        _rarePassChanceMin = config->getFloat("rarePassChanceMin");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'rarePassChanceMin'");
        valid = false;
    }

    if (config->has("rarePassChanceMax") && config->get("rarePassChanceMax")->isNumber()) {
        _rarePassChanceMax = config->getFloat("rarePassChanceMax");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'rarePassChanceMax'");
        valid = false;
    }

    if (config->has("divinePassChanceMin") && config->get("divinePassChanceMin")->isNumber()) {
        _divinePassChanceMin = config->getFloat("divinePassChanceMin");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'divinePassChanceMin'");
        valid = false;
    }

    if (config->has("divinePassChanceMax") && config->get("divinePassChanceMax")->isNumber()) {
        _divinePassChanceMax = config->getFloat("divinePassChanceMax");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'divinePassChanceMax'");
        valid = false;
    }
    
    if (config->has("rockPassChanceMin") && config->get("rockPassChanceMin")->isNumber()) {
        _rockPassChanceMin = config->getFloat("rockPassChanceMin");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'rockPassChanceMin'");
        valid = false;
    }

    if (config->has("rockPassChanceMax") && config->get("rockPassChanceMax")->isNumber()) {
        _rockPassChanceMax = config->getFloat("rockPassChanceMax");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'rockPassChanceMax'");
        valid = false;
    }
    
    if (config->has("divineObedienceMin") && config->get("divineObedienceMin")->isNumber()) {
        _divineObedienceMin = config->getFloat("divineObedienceMin");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'divineObedienceMin'");
        valid = false;
    }

    if (config->has("divineObedienceMax") && config->get("divineObedienceMax")->isNumber()) {
        _divineObedienceMax = config->getFloat("divineObedienceMax");
    } else {
        if (_debug) CULogError("PlayerAI::init — missing or invalid 'divineObedienceMax'");
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
            float total = mults->attack + mults->support;
            if (total > 0.0f) {
                houseAttack  = mults->attack  / total;
                houseSupport = mults->support / total;
            }
            // Clamp so no house goes more extreme than 70/30 in either direction.
            // This ensures even pure-attack (Ares: 1/0) or pure-support (Demeter: 0/1)
            // houses retain meaningful behaviour in both action types.
            houseAttack  = std::max(HOUSE_WEIGHT_MIN, std::min(HOUSE_WEIGHT_MAX, houseAttack));
            houseSupport = 1.0f - houseAttack;
        }
    }
    _attackWeight  = 0.5f + _decisionMultiplier * (houseAttack  - 0.5f);
    _supportWeight = 0.5f + _decisionMultiplier * (houseSupport - 0.5f);

    // rarityWisdom: scale pass chances linearly from 0 at worst to full at best
    _effectiveRarePassChance   = _rarePassChanceMin   + _decisionMultiplier * (_rarePassChanceMax   - _rarePassChanceMin);
    _effectiveDivinePassChance = _divinePassChanceMin + _decisionMultiplier * (_divinePassChanceMax - _divinePassChanceMin);
    _effectiveRockPassChance = _rockPassChanceMin + _decisionMultiplier * (_rockPassChanceMax - _rockPassChanceMin);
    _effectiveDivineObedience = _divineObedienceMin + _decisionMultiplier * (_divineObedienceMax - _divineObedienceMin);

    CULog(
        "[PlayerAI '%s'] multiplier=%.2f → interval=%.2f heal=%.2f "
        "atk=%.2f sup=%.2f rarePass=%.2f divinePass=%.2f rockPass=%.2f divineObedience=%.2f",
        getPlayerName().c_str(), _decisionMultiplier,
        _thinkInterval, _healThreshold,
        _attackWeight, _supportWeight,
        _effectiveRarePassChance, _effectiveDivinePassChance,
        _effectiveRockPassChance, _effectiveDivineObedience);
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
 * Returns whether the AI has at least one support item in its inventory,
 * regardless of whether any neighbor currently needs healing.
 * Used by evaluate() to decide if SUPPORT is a viable roll option.
 *
 * @return true if any inventory item has type ItemDef::Type::Support.
 */
bool PlayerAI::hasSupportItem() const {
    for (const ItemInstance& item : getInventory()) {
        auto def = _db->getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Support) return true;
    }
    return false;
}

/**
 * Returns whether the AI has a support item AND at least one neighbor is
 * alive and below _healThreshold. Used only by actSupport() to find a target.
 *
 * @return true if a support item exists and a valid heal target is available.
 */
bool PlayerAI::canSupport() const {
    if (!hasSupportItem()) return false;

    auto needsHeal = [&](Player* player) {
        return player && player->isAlive() &&
               player->getCurrentHealth() < 100;
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
 *   1. Support check — heal a teammate below _healThreshold immediately.
 *   2. Rarity-pass check — pass unowned rare/divine non-attack items to
 *      affinity-matched neighbors. See evaluateRarityPass().
 *   3. Weighted action roll — divine ruleset check then house-weighted roll
 *      between ATTACK and SUPPORT. See evaluateWeightedAction().
 *
 * @param enemy  The current enemy.
 * @return The State the AI should transition to this think cycle.
 */
PlayerAI::State PlayerAI::evaluate(const Enemy& enemy) {
    _pendingPassItemId  = 0;
    _pendingPassTarget  = nullptr;
    _rulesetDeferDivine = false;

    if (getInventory().empty()) return State::IDLE;

    // --- 1. Support check ---
    if (hasSupportItem()) {
        auto needsHeal = [&](Player* player) {
            return player && player->isAlive() &&
                   player->getCurrentHealth() / player->getMaxHealth() < _healThreshold;
        };
        if (needsHeal(getLeftPlayer()) || needsHeal(getRightPlayer())) {
            if (_debug) CULog("[PlayerAI '%s'] evaluate — support priority (teammate below %.0f%%)",
                  getPlayerName().c_str(), _healThreshold * 100.0f);
            return State::SUPPORT;
        }
    }

    // --- 2. Rarity-pass check ---
    bool hasAliveNeighbor = (getLeftPlayer()  && getLeftPlayer()->isAlive()) ||
                            (getRightPlayer() && getRightPlayer()->isAlive());

    if (hasAliveNeighbor && evaluateRarityPass()) {
        return State::PASS;
    }

    // --- 3. Weighted action roll ---
    return evaluateWeightedAction(hasAliveNeighbor);
}

/**
 * Scans inventory for unowned non-attack rare and divine items and rolls
 * against pass chances. If triggered, sets _pendingPassItemId and
 * _pendingPassTarget as side effects and returns true.
 *
 * Attack items are never passed. Commons are never rarity-passed.
 * Pass target is the alive neighbor whose house matches the item's affinity,
 * or nullptr if neither neighbor matches (actPass() picks randomly).
 *
 * @return true if a rarity-pass was triggered, false otherwise.
 */
bool PlayerAI::evaluateRarityPass() {
    auto findAffinityNeighbor = [&](const ItemInstance& inventoryItem) -> Player* {
        auto itemDef = _db->getDef(inventoryItem.getDefId());
        if (!itemDef) return nullptr;

        ItemDef::House itemAffinity = itemDef->getHouseAffinity();
        if (itemAffinity == ItemDef::House::None) return nullptr;

        auto matchesAffinity = [&](Player* neighbor) -> bool {
            if (!neighbor || !neighbor->isAlive()) return false;
            return ItemDef::houseFromString(neighbor->getHouseName(), ItemDef::House::None)
                   == itemAffinity;
        };

        if (matchesAffinity(getLeftPlayer()))  return getLeftPlayer();
        if (matchesAffinity(getRightPlayer())) return getRightPlayer();
        return nullptr;
    };

    for (const ItemInstance& inventoryItem : getInventory()) {
        auto itemDef = _db->getDef(inventoryItem.getDefId());
        if (!itemDef) continue;
        if (itemDef->getType() == ItemDef::Type::Attack) continue;

        float passChance = 0.0f;
        if      (itemDef->getRarity() == ItemDef::Rarity::Divine) passChance = _effectiveDivinePassChance;
        else if (itemDef->getRarity() == ItemDef::Rarity::Rare)   passChance = _effectiveRarePassChance;
        else continue;

        float passRoll = static_cast<float>(rand()) / RAND_MAX;
        if (passRoll < passChance) {
            _pendingPassItemId = inventoryItem.getId();
            _pendingPassTarget = findAffinityNeighbor(inventoryItem);
            if (_debug) CULog(
                "[PlayerAI '%s'] evaluateRarityPass — triggered (roll=%.2f chance=%.2f) → target='%s'",
                getPlayerName().c_str(), passRoll, passChance,
                _pendingPassTarget ? _pendingPassTarget->getPlayerName().c_str() : "random");
            return true;
        }
    }

    return false;
}

/**
 * Checks the divine ruleset for any owned house-affinity divine in inventory,
 * then performs a house-weighted roll between ATTACK and SUPPORT.
 * Sets _rulesetDeferDivine if the ruleset defers the divine this cycle so
 * actAttack() can exclude it from the candidate pool.
 *
 * Returns PASS or IDLE if neither ATTACK nor SUPPORT is viable.
 *
 * @param hasAliveNeighbor  Whether an alive neighbor exists, used for the
 *                          PASS vs IDLE fallback decision.
 * @return The State the AI should transition to.
 */
PlayerAI::State PlayerAI::evaluateWeightedAction(bool hasAliveNeighbor) {
    // Divine ruleset check — only applies to this AI's own house-affinity divine
    ItemDef::House ownHouse = ItemDef::houseFromString(getHouseName(), ItemDef::House::None);
    bool hasOwnedDivine = false;

    if (ownHouse != ItemDef::House::None) {
        for (const ItemInstance& inventoryItem : getInventory()) {
            auto itemDef = _db->getDef(inventoryItem.getDefId());
            if (itemDef &&
                itemDef->getRarity()        == ItemDef::Rarity::Divine &&
                itemDef->getHouseAffinity() == ownHouse) {
                hasOwnedDivine = true;
                break;
            }
        }
    }

    if (hasOwnedDivine) {
        float obedienceRoll = static_cast<float>(rand()) / RAND_MAX;
        bool  shouldObey    = obedienceRoll < _effectiveDivineObedience;
        bool  rulesetPassed = checkDivineRuleset();

        if (shouldObey && !rulesetPassed) {
            if (_debug) CULog(
                "[PlayerAI '%s'] evaluateWeightedAction — divine deferred by ruleset "
                "(roll=%.2f obedience=%.2f)",
                getPlayerName().c_str(), obedienceRoll, _effectiveDivineObedience);
            _rulesetDeferDivine = true;
        }
    }

    bool canAttackNow  = canAttack();
    bool canSupportNow = canSupport();

    if (!canAttackNow && !canSupportNow) {
        if (_debug) CULog("[PlayerAI '%s'] evaluateWeightedAction — fallback pass/idle",
              getPlayerName().c_str());
        return hasAliveNeighbor ? State::PASS : State::IDLE;
    }

    if (canAttackNow && !canSupportNow) {
        if (_debug) CULog("[PlayerAI '%s'] evaluateWeightedAction — attacking (no support items)",
              getPlayerName().c_str());
        return State::ATTACK;
    }

    if (!canAttackNow && canSupportNow) {
        if (_debug) CULog("[PlayerAI '%s'] evaluateWeightedAction — supporting (no attack items)",
              getPlayerName().c_str());
        return State::SUPPORT;
    }

    // Both available — roll against house-weighted ratio
    float totalWeight = _attackWeight + _supportWeight;
    float actionRoll  = static_cast<float>(rand()) / RAND_MAX * totalWeight;

    if (actionRoll < _attackWeight) {
        if (_debug) CULog("[PlayerAI '%s'] evaluateWeightedAction — attacking (roll=%.2f atk=%.2f sup=%.2f)",
              getPlayerName().c_str(), actionRoll, _attackWeight, _supportWeight);
        return State::ATTACK;
    } else {
        if (_debug) CULog("[PlayerAI '%s'] evaluateWeightedAction — supporting (roll=%.2f atk=%.2f sup=%.2f)",
              getPlayerName().c_str(), actionRoll, _attackWeight, _supportWeight);
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
    ItemDef::House ownHouse = ItemDef::houseFromString(getHouseName(), ItemDef::House::None);

    std::vector<ItemInstance::ItemId> attackItems;
    for (const ItemInstance& item : getInventory()) {
        auto itemDef = _db->getDef(item.getDefId());
        if (!itemDef || itemDef->getType() != ItemDef::Type::Attack) continue;

        // If the ruleset deferred the divine, remove any attack item that is
        // this AI's own house-affinity divine from the candidate pool entirely.
        if (_rulesetDeferDivine &&
            itemDef->getRarity()        == ItemDef::Rarity::Divine &&
            itemDef->getHouseAffinity() == ownHouse) {
            if (_debug) CULog("[PlayerAI '%s'] actAttack — excluding own divine (ruleset deferred)",
                  getPlayerName().c_str());
            continue;
        }

        attackItems.push_back(item.getId());
    }

    _rulesetDeferDivine = false;

    if (attackItems.empty()) {
        if (_debug) CULog("[PlayerAI '%s'] actAttack — no eligible attack items",
              getPlayerName().c_str());
        
        // Divine was the only attack item and ruleset said no.
        // Try supporting instead, then fall back to passing.
        if (canSupport()) {
            if (_debug) CULog("[PlayerAI '%s'] actAttack — no eligible attack items, diverting to support",
                  getPlayerName().c_str());
            actSupport(items);
            return;
        } else {
            if (_debug) CULog("[PlayerAI '%s'] actAttack — no eligible attack items, trying to pass",
                  getPlayerName().c_str());
            actPass();
            return;
        }
        return;
    }

    ItemInstance::ItemId chosen = attackItems[rand() % attackItems.size()];
    if (_debug) CULog("[PlayerAI '%s'] actAttack — using item %llu on '%s'",
          getPlayerName().c_str(), (unsigned long long)chosen, enemy.getId().c_str());
    useAttackItemById(chosen, enemy, items);
}

/**
 * Finds the most injured neighbor and uses a random
 * support item on them. Does nothing if no valid target or support item exists.
 *
 * @param items  The ItemController used to resolve the item action.
 */
void PlayerAI::actSupport(ItemController& items) {
    Player* target = nullptr;
    float lowestRatio = 1.0f;

    auto check = [&](Player* p) {
        if (!p || !p->isAlive()) return;
        float ratio = p->getCurrentHealth() / p->getMaxHealth();
        if (ratio < lowestRatio) { lowestRatio = ratio; target = p; }
    };
    check(getLeftPlayer());
    check(getRightPlayer());

    if (!target) {
        if (_debug) CULog("[PlayerAI '%s'] actSupport — no alive neighbour, aborting",
                getPlayerName().c_str());
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
    
    // Gaia rock check — using a gaia_rock on a teammate damages them.
    // Roll against _effectiveRockPassChance and pass it instead if triggered.
    auto chosenDef = _db->getDef("");
    for (const ItemInstance& inventoryItem : getInventory()) {
        if (inventoryItem.getId() == chosen) {
            chosenDef = _db->getDef(inventoryItem.getDefId());
            break;
        }
    }

    if (chosenDef && chosenDef->getId() == "gaia_rock") {
        float rockRoll = static_cast<float>(rand()) / RAND_MAX;
        if (rockRoll < _effectiveRockPassChance) {
            if (_debug) CULog(
                "[PlayerAI '%s'] actSupport — gaia_rock pass triggered (roll=%.2f chance=%.2f)",
                getPlayerName().c_str(), rockRoll, _effectiveRockPassChance);
            // Divert to a pass — set pending item and let actPass() handle delivery
            _pendingPassItemId = chosen;
            _pendingPassTarget = nullptr; // no affinity target for rocks — pass randomly
            actPass();
            return;
        }
    }
    if (_debug) CULog("[PlayerAI '%s'] actSupport — using item %llu on '%s' (ratio=%.2f)",
          getPlayerName().c_str(), (unsigned long long)chosen,
          target->getPlayerName().c_str(), lowestRatio);
    useItemById(chosen, *target, *_db);
}

/**
 * Passes an item to a neighbor (left or right).
 *
 * If _pendingPassItemId was set by evaluate() this cycle, that specific item
 * is passed. If _pendingPassTarget is set, the item goes directly to that
 * neighbor (the one who originally owned it). If _pendingPassTarget is nullptr,
 * the item goes to a random alive neighbor.
 *
 * When no pending item is set (fallback pass), selects an item based on
 * inventory balance: passes a support item if the AI has more support items
 * than attack items, passes an attack item if it has more attack items, or
 * picks randomly if counts are equal. This keeps the AI's inventory balanced
 * rather than hoarding one type while passing another.
 *
 * Does nothing if inventory is empty or no alive neighbor exists.
 */
void PlayerAI::actPass() {
    if (getInventory().empty()) {
        if (_debug) CULog("[PlayerAI '%s'] actPass — inventory empty, aborting", getPlayerName().c_str());
        return;
    }

    std::vector<Player*> targets;
    if (getLeftPlayer()  && getLeftPlayer()->isAlive() && !hasLeftVine() && !getLeftPlayer()->hasRightVine())  targets.push_back(getLeftPlayer());
    if (getRightPlayer() && getRightPlayer()->isAlive() && !hasRightVine() && !getRightPlayer()->hasLeftVine()) targets.push_back(getRightPlayer());

    if (targets.empty()) {
        if (_debug) CULog("[PlayerAI '%s'] actPass — no alive neighbours, aborting", getPlayerName().c_str());
        return;
    }

    // Resolve which item to pass
    const ItemInstance* resolved = nullptr;
    if (_pendingPassItemId != 0) {
        for (const ItemInstance& inventoryItem : getInventory()) {
            if (inventoryItem.getId() == _pendingPassItemId) {
                resolved = &inventoryItem;
                break;
            }
        }
        if (!resolved && _debug) {
            CULog("[PlayerAI '%s'] actPass — pending item %llu not found, falling back to balance pass",
                  getPlayerName().c_str(), (unsigned long long)_pendingPassItemId);
        }
    }

    ItemInstance itemToPass;
    if (resolved) {
        itemToPass = *resolved;
    } else {
        // Fallback pass — balance inventory by passing from the larger type bucket.
        // Count attack and support items separately, then pick from whichever
        // is larger. If equal, pick randomly across all items.
        std::vector<ItemInstance::ItemId> attackItems;
        std::vector<ItemInstance::ItemId> supportItems;

        for (const ItemInstance& inventoryItem : getInventory()) {
            auto itemDef = _db->getDef(inventoryItem.getDefId());
            if (!itemDef) continue;
            if      (itemDef->getType() == ItemDef::Type::Attack)  attackItems.push_back(inventoryItem.getId());
            else if (itemDef->getType() == ItemDef::Type::Support) supportItems.push_back(inventoryItem.getId());
        }

        ItemInstance::ItemId chosenId = 0;
        if (attackItems.size() > supportItems.size() && !attackItems.empty()) {
            // More attack items — pass one to balance toward support
            chosenId = attackItems[rand() % attackItems.size()];
            if (_debug) CULog("[PlayerAI '%s'] actPass — balance pass: passing attack item (atk=%zu sup=%zu)",
                  getPlayerName().c_str(), attackItems.size(), supportItems.size());
        } else if (supportItems.size() > attackItems.size() && !supportItems.empty()) {
            // More support items — pass one to balance toward attack
            chosenId = supportItems[rand() % supportItems.size()];
            if (_debug) CULog("[PlayerAI '%s'] actPass — balance pass: passing support item (atk=%zu sup=%zu)",
                  getPlayerName().c_str(), attackItems.size(), supportItems.size());
        } else {
            // Equal counts or only one type present — pick randomly from full inventory
            const auto& inventory = getInventory();
            chosenId = inventory[rand() % inventory.size()].getId();
            if (_debug) CULog("[PlayerAI '%s'] actPass — balance pass: random (atk=%zu sup=%zu)",
                  getPlayerName().c_str(), attackItems.size(), supportItems.size());
        }

        // Resolve the chosen ID back to an ItemInstance
        for (const ItemInstance& inventoryItem : getInventory()) {
            if (inventoryItem.getId() == chosenId) {
                itemToPass = inventoryItem;
                break;
            }
        }
    }

    _pendingPassItemId = 0;

    // Resolve target — prefer the affinity neighbor set by evaluate(), fall back to random
    Player* chosenTarget = nullptr;
    if (_pendingPassTarget && _pendingPassTarget->isAlive()) {
        chosenTarget = _pendingPassTarget;
    } else {
        chosenTarget = targets[rand() % targets.size()];
    }
    _pendingPassTarget = nullptr;

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

/**
 * Evaluates whether current game conditions meet the house-specific divine
 * ruleset. Returns true if conditions are satisfied or no ruleset exists
 * for this house, meaning the AI may proceed with using the divine item.
 * Returns false if conditions are not met, meaning the AI should defer.
 *
 * @return true if the divine may be used, false if conditions are not met.
 */
bool PlayerAI::checkDivineRuleset() const {
    ItemDef::House house = ItemDef::houseFromString(getHouseName(), ItemDef::House::None);

    // Helper: health ratio of a player, or 1.0 if null/dead
    auto healthRatio = [](Player* player) -> float {
        if (!player || !player->isAlive()) return 1.0f;
        return player->getCurrentHealth() / player->getMaxHealth();
    };

    Player* leftNeighbor  = getLeftPlayer();
    Player* rightNeighbor = getRightPlayer();

    // Count how many of {self, left, right} are below a health threshold
    auto countBelowThreshold = [&](float threshold) -> int {
        int count = 0;
        if (getCurrentHealth() / getMaxHealth() < threshold) count++;
        if (leftNeighbor  && leftNeighbor->isAlive()  && healthRatio(leftNeighbor)  < threshold) count++;
        if (rightNeighbor && rightNeighbor->isAlive() && healthRatio(rightNeighbor) < threshold) count++;
        return count;
    };

    // Check if a player holds any divine item
    auto hasDivine = [&](Player* player) -> bool {
        if (!player) return false;
        for (const ItemInstance& item : player->getInventory()) {
            auto itemDef = _db->getDef(item.getDefId());
            if (itemDef && itemDef->getRarity() == ItemDef::Rarity::Divine) return true;
        }
        return false;
    };

    bool conditionsMet = true;

    switch (house) {

        case ItemDef::House::Poseidon: {
            // Self or a teammate below 50%, OR 2+ players below 75%
            bool anyBelowHalf   = countBelowThreshold(0.5f) >= 1;
            bool twoBelowThreeQ = countBelowThreshold(0.75f) >= 2;
            conditionsMet       = anyBelowHalf || twoBelowThreeQ;
            if (_debug) CULog(
                "[PlayerAI '%s'] divineRuleset poseidon — anyBelow50=%d twoBelowQ75=%d → %s",
                getPlayerName().c_str(), anyBelowHalf, twoBelowThreeQ,
                conditionsMet ? "USE" : "DEFER");
            break;
        }

        case ItemDef::House::Demeter: {
            // Self or teammate below 30%, OR 2+ below 50%, OR all 3 below 60%
            bool anyBelowThird = countBelowThreshold(0.3f) >= 1;
            bool twoBelowHalf  = countBelowThreshold(0.5f) >= 2;
            bool allBelowSixty = countBelowThreshold(0.6f) >= 3;
            conditionsMet      = anyBelowThird || twoBelowHalf || allBelowSixty;
            if (_debug) CULog(
                "[PlayerAI '%s'] divineRuleset demeter — anyBelow30=%d twoBelowHalf=%d allBelow60=%d → %s",
                getPlayerName().c_str(), anyBelowThird, twoBelowHalf, allBelowSixty,
                conditionsMet ? "USE" : "DEFER");
            break;
        }

        case ItemDef::House::Hades: {
            // At least one teammate must be dead
            bool leftDead  = leftNeighbor  && !leftNeighbor->isAlive();
            bool rightDead = rightNeighbor && !rightNeighbor->isAlive();
            conditionsMet  = leftDead || rightDead;
            if (_debug) CULog(
                "[PlayerAI '%s'] divineRuleset hades — leftDead=%d rightDead=%d → %s",
                getPlayerName().c_str(), leftDead, rightDead,
                conditionsMet ? "USE" : "DEFER");
            break;
        }

        case ItemDef::House::Hephaestus: {
            // Self and all alive teammates must each have at least 2 items
            bool selfHasEnough  = (int)getInventory().size() >= 2;
            bool leftHasEnough  = !leftNeighbor  || !leftNeighbor->isAlive()
                                  || (int)leftNeighbor->getInventory().size()  >= 2;
            bool rightHasEnough = !rightNeighbor || !rightNeighbor->isAlive()
                                  || (int)rightNeighbor->getInventory().size() >= 2;
            conditionsMet       = selfHasEnough && leftHasEnough && rightHasEnough;
            if (_debug) CULog(
                "[PlayerAI '%s'] divineRuleset hephaestus — self=%d left=%d right=%d → %s",
                getPlayerName().c_str(), selfHasEnough, leftHasEnough, rightHasEnough,
                conditionsMet ? "USE" : "DEFER");
            break;
        }

        case ItemDef::House::Hermes: {
            // No teammate currently holds a divine item
            bool leftHasDivine  = hasDivine(leftNeighbor);
            bool rightHasDivine = hasDivine(rightNeighbor);
            conditionsMet       = !leftHasDivine && !rightHasDivine;
            if (_debug) CULog(
                "[PlayerAI '%s'] divineRuleset hermes — leftHasDivine=%d rightHasDivine=%d → %s",
                getPlayerName().c_str(), leftHasDivine, rightHasDivine,
                conditionsMet ? "USE" : "DEFER");
            break;
        }

        default:
            // No ruleset for this house — always allow use
            if (_debug) CULog("[PlayerAI '%s'] divineRuleset — no ruleset for house '%s', allowing use",
                  getPlayerName().c_str(), getHouseName().c_str());
            conditionsMet = true;
            break;
    }

    return conditionsMet;
}

/**
 * Returns whether an AI player is allowed to apply a definition's configured effects.
 *
 * @param player The AI player attempting to use the item.
 * @param def    The item definition whose effect eligibility is being checked.
 * @return True if the player's house or educate effect allows item effects to apply.
 */
bool PlayerAI::canPlayerApplyEffects(const Player& player, const ItemDef& def) {
    if (player.hasEducate()) {
        return true;
    }

    if (def.getHouseAffinity() == ItemDef::House::None) {
        return true;
    }

    return def.getHouseAffinity() ==
           ItemDef::houseFromString(player.getHouseName(), ItemDef::House::None);
}

/**
 * Collects every party member reachable from the source player's neighbor links.
 *
 * @param source The AI player whose party ring should be traversed.
 * @return The connected party members, including source, with no duplicate players.
 */
std::vector<Player*> PlayerAI::collectReachablePartyMembers(Player& source) {
    std::vector<Player*> party;
    party.push_back(&source);

    for (size_t index = 0; index < party.size() && party.size() < Enemy::NUM_PLAYERS; ++index) {
        Player* player = party[index];
        if (!player) {
            continue;
        }

        Player* neighbors[2] = { player->getLeftPlayer(), player->getRightPlayer() };
        for (Player* neighbor : neighbors) {
            if (!neighbor) {
                continue;
            }
            if (std::find(party.begin(), party.end(), neighbor) == party.end()) {
                party.push_back(neighbor);
            }
        }
    }

    return party;
}

/**
 * Applies AI-triggered frenzy item-spawn effects after an attack item use.
 *
 * @param source The AI player that used the item.
 * @param def    The item definition that may contain frenzy effects.
 * @param items  The ItemController that owns item-spawn frenzy state.
 */
void PlayerAI::applyAIFrenzyEffects(Player& source, const ItemDef& def, ItemController& items) {
    if (!canPlayerApplyEffects(source, def)) {
        return;
    }

    for (const ItemDef::Effect& effect : def.getEffects()) {
        if (effect.type != ItemDef::EffectType::Frenzy) {
            continue;
        }

        float itemInterval = effect.amount;
        if (source.hasCharm()) {
            itemInterval *= 0.5f;
        }

        if (itemInterval <= 0.0f || effect.duration <= 0.0f) {
            continue;
        }

        for (Player* player : collectReachablePartyMembers(source)) {
            if (player) {
                player->clearInventory();
            }
        }
        items.applyFrenzy(itemInterval, effect.duration);
    }
}

/**
 * Collects AI-triggered forge chances for GameScene to apply authoritatively.
 *
 * @param source The AI player that used the item.
 * @param def    The item definition that may contain forge effects.
 * @return Resolved forge chances after AI house/effect eligibility and charm.
 */
std::vector<float> PlayerAI::collectAIForgeChances(Player& source, const ItemDef& def) {
    std::vector<float> forgeChances;
    if (!canPlayerApplyEffects(source, def)) {
        return forgeChances;
    }

    for (const ItemDef::Effect& effect : def.getEffects()) {
        if (effect.type != ItemDef::EffectType::Forge) {
            continue;
        }

        float divineChance = effect.chance;
        if (source.hasCharm()) {
            divineChance = std::min(1.0f, divineChance * 2.0f);
        }

        forgeChances.push_back(divineChance);
    }

    return forgeChances;
}

/**
 * Returns and clears host-level forge effects triggered by this AI.
 *
 * GameScene owns the authoritative forge seed and network broadcast path,
 * so PlayerAI only reports the resolved chance for each triggered effect.
 *
 * @return The resolved forge chances triggered since the last drain.
 */
std::vector<float> PlayerAI::consumePendingForgeChances() {
    std::vector<float> chances = _pendingForgeChances;
    _pendingForgeChances.clear();
    return chances;
}

/**
 * Uses an attack item and applies AI-owned follow-up effects that live
 * outside Player::useItemById, such as item-controller frenzy.
 *
 * @param itemId The inventory item instance to consume.
 * @param enemy  The enemy attack target.
 * @param items  The ItemController used for shared item-spawn effects.
 * @return The resolved attack magnitude, or -1.0f if the item use failed.
 */
float PlayerAI::useAttackItemById(ItemInstance::ItemId itemId, Enemy& enemy, ItemController& items) {
    if (!_db) {
        return -1.0f;
    }

    std::shared_ptr<const ItemDef> def = nullptr;
    for (const ItemInstance& item : getInventory()) {
        if (item.getId() == itemId) {
            def = _db->getDef(item.getDefId());
            break;
        }
    }

    const float resolvedMagnitude = useItemById(itemId, enemy, *_db);
    if (resolvedMagnitude >= 0.0f && def && def->getAttackTarget() == ItemDef::AttackTarget::AllAllies) {
        std::vector<float> forgeChances = collectAIForgeChances(*this, *def);
        _pendingForgeChances.insert(_pendingForgeChances.end(), forgeChances.begin(), forgeChances.end());
        applyAIFrenzyEffects(*this, *def, items);
    }

    return resolvedMagnitude;
}

