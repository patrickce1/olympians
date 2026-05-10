// ItemTests.cpp
// Unit tests for item JSON parsing and house multipliers loading.

#include "ItemTests.h"
#include "../items/ItemController.h"
#include "../items/ItemDatabase.h"
#include "../items/ItemInstance.h"
#include "../HouseLoader.h"
#include "../Player.h"
#include "../Enemy.h"
#include <cugl/cugl.h>
#include <cmath>
#include <set>
#include <vector>

namespace {

int _passed = 0;  ///< Count of passed test assertions.
int _failed = 0;   ///< Count of failed test assertions.

/**
 * Assertion helper that logs a pass/fail message and increments counters.
 *
 * @param condition Boolean condition to test; true logs [PASS], false logs [FAIL]
 * @param label    Test label text describing what is being asserted
 */
void assertWithLabel(bool condition, const std::string& label) {
    if (condition) {
        CULog("[PASS] %s", label.c_str());
        _passed++;
    } else {
        CULog("[FAIL] %s", label.c_str());
        _failed++;
    }
}

/**
 * Logs a summary of passed and failed test assertions with formatting.
 */
void printSummary() {
    CULog("-----------------------------------------");
    CULog("  %d passed   %d failed", _passed, _failed);
    CULog("-----------------------------------------");
}

/**
 * Compares two floats for equality within an error range (tolerance).
 *
 * Useful for floating-point arithmetic comparisons where exact equality is unreliable due to
 * rounding errors or precision loss in calculations. Two values are considered equal if their
 * absolute difference is within the specified tolerance.
 *
 * @param firstValue   First value to compare
 * @param secondValue  Second value to compare
 * @param tolerance    Error range/tolerance for comparison (default: 1e-4)
 * @return            True if |firstValue - secondValue| <= tolerance, false otherwise
 */
bool floatsEqualWithinTolerance(float firstValue, float secondValue, float tolerance = 1e-4f) {
    return std::fabs(firstValue - secondValue) <= tolerance;
}

/**
 * Loads JSON from an asset file path using CUGL's JsonReader.
 *
 * The path is resolved relative to the assets directory. Logs errors if file opening or
 * JSON parsing fails.
 *
 * @param path Asset-relative path to JSON file (e.g., "json/items.json")
 * @return     Shared pointer to parsed JsonValue, or nullptr if loading/parsing fails
 */
std::shared_ptr<cugl::JsonValue> readJson(const std::string& path) {
    auto reader = cugl::JsonReader::alloc(path);
    if (!reader) {
        CULogError("ItemTests: failed to open '%s'", path.c_str());
        return nullptr;
    }
    auto json = reader->readJson();
    if (!json) {
        CULogError("ItemTests: failed to parse '%s'", path.c_str());
    }
    return json;
}

/**
 * Collects the current definition IDs from a player's inventory in slot order.
 *
 * @param player Player whose inventory should be inspected
 * @return       Definition IDs for each item instance in inventory order
 */
std::vector<std::string> collectInventoryDefIds(const Player& player) {
    std::vector<std::string> result;
    for (const ItemInstance& item : player.getInventory()) {
        result.push_back(item.getDefId());
    }
    return result;
}

/**
 * Collects stable item instance IDs from a player's inventory in slot order.
 *
 * @param player Player whose inventory should be inspected
 * @return       Stable item instance IDs for each inventory slot
 */
std::vector<ItemInstance::ItemId> collectInventoryItemIds(const Player& player) {
    std::vector<ItemInstance::ItemId> result;
    for (const ItemInstance& item : player.getInventory()) {
        result.push_back(item.getId());
    }
    return result;
}

/**
 * Tests item JSON loading and basic field validation.
 *
 * Verifies that:
 * - ItemDatabase loads successfully from parsed JSON
 * - At least one item definition exists after loading
 * - All loaded items have positive baseValue (either explicit or default)
 * - Specific item affinities parse correctly (e.g., lightning_bolt -> Zeus)
 *
 * @param itemsJson Parsed JSON object containing item definitions
 */
void testItemsLoad(const std::shared_ptr<cugl::JsonValue>& itemsJson) {
    ItemDatabase db;
    bool ok = db.loadFromJson(itemsJson);
    assertWithLabel(ok, "items: loadFromJson succeeds");
    
    auto allIds = db.getAllDefIds();
    assertWithLabel(!allIds.empty(), "items: at least one item definition exists");
    
    bool allValid = true;
    for (const std::string& id : allIds) {
        auto def = db.getDef(id);
        if (!def) {
            allValid = false;
            break;
        }
        if (def->getBaseValue() < 0.0f) {
            allValid = false;
            break;
        }
    }
    assertWithLabel(allValid, "items: all defs have positive baseValue (including 0)");
    
    auto lightningBoltDef = db.getDef("lightning_bolt");
    auto appleDef = db.getDef("apple");
    auto shieldDef = db.getDef("shield");
    auto helmDef = db.getDef("helm");
    auto wheatDef = db.getDef("wheat");
    auto swordDef = db.getDef("sword");
    auto resurrectionDef = db.getDef("resurrection");
    auto educateDef = db.getDef("educate");
    auto forgeDef = db.getDef("forge");
    auto spearDef = db.getDef("spear");
    auto wingsDef = db.getDef("wings");
    assertWithLabel(lightningBoltDef && lightningBoltDef->getHouseAffinity() == ItemDef::House::Zeus,
           "items: lightning_bolt affinity parses as Zeus");
    assertWithLabel(lightningBoltDef && lightningBoltDef->hasEffectType(ItemDef::EffectType::Stun),
           "items: lightning_bolt parses stun effect");
    assertWithLabel(lightningBoltDef && !lightningBoltDef->getEffects().empty() && floatsEqualWithinTolerance(lightningBoltDef->getEffects()[0].duration, 2.0f),
           "items: lightning_bolt stun duration parses");
    assertWithLabel(appleDef && appleDef->getHouseAffinity() == ItemDef::House::None,
           "items: apple affinity parses as none");
    assertWithLabel(shieldDef && shieldDef->hasEffectType(ItemDef::EffectType::Shield),
           "items: shield parses shield effect");
    assertWithLabel(shieldDef && !shieldDef->getEffects().empty() && floatsEqualWithinTolerance(shieldDef->getEffects()[0].duration, 5.0f),
           "items: shield effect duration parses");
    assertWithLabel(helmDef && helmDef->hasEffectType(ItemDef::EffectType::Barrier),
           "items: helm parses barrier effect");
    assertWithLabel(helmDef && !helmDef->getEffects().empty() &&
                    floatsEqualWithinTolerance(helmDef->getEffects()[0].multiplier, 0.0f) &&
                    floatsEqualWithinTolerance(helmDef->getEffects()[0].duration, 1000.0f),
           "items: helm barrier values parse");
    assertWithLabel(wheatDef && wheatDef->hasEffectType(ItemDef::EffectType::Regen),
           "items: wheat parses regen effect");
    assertWithLabel(wheatDef && !wheatDef->getEffects().empty() &&
                    floatsEqualWithinTolerance(wheatDef->getEffects()[0].regenAmount, 25.0f) &&
                    floatsEqualWithinTolerance(wheatDef->getEffects()[0].duration, 5.0f),
           "items: wheat regen values parse");
    assertWithLabel(swordDef && swordDef->getAttackTarget() == ItemDef::AttackTarget::Enemy,
           "items: sword attack target parses as enemy");
    assertWithLabel(resurrectionDef && resurrectionDef->hasEffectType(ItemDef::EffectType::Resurrect),
           "items: resurrection parses resurrect effect");
    assertWithLabel(resurrectionDef && resurrectionDef->getAttackTarget() == ItemDef::AttackTarget::AllAllies,
           "items: resurrection attack target parses as all allies");
    assertWithLabel(resurrectionDef && !resurrectionDef->getEffects().empty() &&
                    resurrectionDef->getEffects()[0].targetAllAllies &&
                    floatsEqualWithinTolerance(resurrectionDef->getEffects()[0].reviveHealth, 25.0f) &&
                    floatsEqualWithinTolerance(resurrectionDef->getEffects()[0].regenAmount, 25.0f) &&
                    floatsEqualWithinTolerance(resurrectionDef->getEffects()[0].duration, 5.0f),
           "items: resurrection revive and regen values parse");
    assertWithLabel(educateDef && educateDef->hasEffectType(ItemDef::EffectType::Educate),
           "items: educate parses educate effect");
    assertWithLabel(educateDef && educateDef->getAttackTarget() == ItemDef::AttackTarget::AllAllies,
           "items: educate attack target parses as all allies");
    assertWithLabel(educateDef && !educateDef->getEffects().empty() &&
                    floatsEqualWithinTolerance(educateDef->getEffects()[0].duration, 10.0f),
           "items: educate duration parses");
    assertWithLabel(forgeDef && forgeDef->hasEffectType(ItemDef::EffectType::Forge),
           "items: forge parses forge effect");
    assertWithLabel(forgeDef && forgeDef->getAttackTarget() == ItemDef::AttackTarget::AllAllies,
           "items: forge attack target parses as all allies");
    assertWithLabel(forgeDef && !forgeDef->getEffects().empty() &&
                    floatsEqualWithinTolerance(forgeDef->getEffects()[0].chance, 0.1f),
           "items: forge divine upgrade chance parses");
    assertWithLabel(spearDef && spearDef->hasEffectType(ItemDef::EffectType::Vulnerable),
           "items: spear parses vulnerable effect");
    assertWithLabel(spearDef && !spearDef->getEffects().empty() && floatsEqualWithinTolerance(spearDef->getEffects()[0].multiplier, 2.0f),
           "items: spear vulnerable multiplier parses");
    assertWithLabel(wingsDef && wingsDef->hasEffectType(ItemDef::EffectType::Slow),
           "items: wings parses slow effect");
    assertWithLabel(wingsDef && wingsDef->getEffects().size() >= 2 &&
                    floatsEqualWithinTolerance(wingsDef->getEffects()[0].multiplier, 0.5f) &&
                    floatsEqualWithinTolerance(wingsDef->getEffects()[0].duration, 5.0f),
           "items: wings slow values parse");
}

/**
 * Tests house multiplier JSON loading and bounds validation.
 *
 * Verifies that:
 * - ItemDatabase loads house multipliers successfully from parsed JSON
 * - All six houses (Zeus, Poseidon, Athena, Ares, Hephaestus, Demeter) have loaded multipliers
 * - All multiplier values (attack, support) are bounded in [0.0, 1.0]
 * - Affinity bonus values are positive (> 0.0)
 *
 * @param housesJson Parsed JSON object containing house definitions with multiplier data
 */
void testHouseMultipliersLoad(const std::shared_ptr<cugl::JsonValue>& housesJson) {
    ItemDatabase db;
    bool ok = db.loadHouseMultipliersFromJson(housesJson);
    assertWithLabel(ok, "houses: loadHouseMultipliersFromJson succeeds");

    const ItemDatabase::HouseMultipliers* zeus = db.getHouseMultipliers("Zeus");
    const ItemDatabase::HouseMultipliers* poseidon = db.getHouseMultipliers("Poseidon");
    const ItemDatabase::HouseMultipliers* athena = db.getHouseMultipliers("Athena");

    assertWithLabel(zeus != nullptr, "houses: Zeus multipliers exist");
    assertWithLabel(poseidon != nullptr, "houses: Poseidon multipliers exist");
    assertWithLabel(athena != nullptr, "houses: Athena multipliers exist");
    const ItemDatabase::HouseMultipliers* checks[] = { zeus, poseidon, athena };
    bool bounded = true;
    for (const auto* entry : checks) {
        if (!entry) {
            bounded = false;
            break;
        }
        if (entry->attack < 0.0f || entry->attack > 1.0f ||
            entry->support < 0.0f || entry->support > 1.0f ||
            entry->affinityBonus <= 0.0f) {
            bounded = false;
            break;
        }
    }
    assertWithLabel(bounded, "houses: multipliers are bounded and affinityBonus is positive");
}

/**
 * Tests parsing of enum string representations (Type, Rarity, House).
 *
 * Verifies that ItemDef parsing functions correctly convert strings to enum values:
 * - Type parsing: "attack", "support"
 * - Rarity parsing: "common", "rare", "divine"
 * - House parsing: "Zeus", "Ares", "none", and other house names
 */
void testEnumParsers() {
    assertWithLabel(ItemDef::typeFromString("attack") == ItemDef::Type::Attack, "parse: type attack");
    assertWithLabel(ItemDef::typeFromString("support") == ItemDef::Type::Support, "parse: type support");
    
    assertWithLabel(ItemDef::rarityFromString("common") == ItemDef::Rarity::Common, "parse: rarity common");
    assertWithLabel(ItemDef::rarityFromString("rare") == ItemDef::Rarity::Rare, "parse: rarity rare");
    assertWithLabel(ItemDef::rarityFromString("divine") == ItemDef::Rarity::Divine, "parse: rarity divine");
    
    assertWithLabel(ItemDef::houseFromString("Zeus") == ItemDef::House::Zeus, "parse: house Zeus");
    assertWithLabel(ItemDef::houseFromString("Ares") == ItemDef::House::Ares, "parse: house Ares");
    assertWithLabel(ItemDef::houseFromString("none") == ItemDef::House::None, "parse: house none");
}

/**
 * Tests weighted random item selection and instance creation.
 *
 * Verifies that:
 * - ItemDatabase can roll random item IDs based on rarity weights (common items rolled more frequently)
 * - All rolled IDs correspond to actual database entries
 * - createInstance() succeeds for known item IDs and returns valid ItemInstance pointers
 * - createInstance() fails for unknown item IDs and returns nullptr
 *
 * @param itemsJson Parsed JSON object containing item definitions
 */
void testWeightedRollAndInstanceCreation(const std::shared_ptr<cugl::JsonValue>& itemsJson) {
    ItemDatabase db;
    assertWithLabel(db.loadFromJson(itemsJson), "db: loads for roll/instance tests");
    db.setStartingPoint(1337);
    
    // Keep the vector alive; constructing from two temporary vectors can produce invalid iterators.
    std::vector<std::string> allIds = db.getAllDefIds();
    std::set<std::string> known(allIds.begin(), allIds.end());
    bool allRolledKnown = true;
    bool rolledAtLeastOne = false;
    for (int i = 0; i < 200; ++i) {
        std::string rolled = db.rollRandomDefId();
        if (rolled.empty()) {
            continue;
        }
        rolledAtLeastOne = true;
        if (known.find(rolled) == known.end()) {
            allRolledKnown = false;
            break;
        }
    }
    assertWithLabel(rolledAtLeastOne, "db: weighted roll returns at least one item id");
    assertWithLabel(allRolledKnown, "db: weighted roll only returns known item ids");
    
    auto okInstance = db.createInstance("apple", 42);
    auto badInstance = db.createInstance("does_not_exist", 999);
    assertWithLabel(okInstance != nullptr, "db: createInstance works for known defId");
    assertWithLabel(badInstance == nullptr, "db: createInstance fails for unknown defId");
}

/**
 * Tests validation that rejects items with unsupported enum values.
 *
 * Verifies that ItemDatabase correctly rejects items with:
 * - Invalid rarity values (not in {common, rare, divine})
 * - Invalid type values (not in {attack, support})
 *
 * Uses fixture JSON files stored in assets/json/tests/ directory.
 */
void testValidationFailures() {
    ItemDatabase db;

    auto badRarityJson = readJson("json/tests/items_bad_rarity.json");
    assertWithLabel(badRarityJson != nullptr, "validation: parse bad-rarity fixture json");
    if (badRarityJson) {
        assertWithLabel(!db.loadFromJson(badRarityJson), "validation: unsupported rarity is rejected");
    }

    auto badTypeJson = readJson("json/tests/items_bad_type.json");
    assertWithLabel(badTypeJson != nullptr, "validation: parse bad-type fixture json");
    if (badTypeJson) {
        assertWithLabel(!db.loadFromJson(badTypeJson), "validation: unsupported item type is rejected");
    }
}

/**
 * Tests multiplier value clamping and default fallbacks for missing or invalid values.
 *
 * Verifies that ItemDatabase correctly:
 * - Clamps attack/support values to [0.0, 1.0]
 * - Defaults missing slider values to 0.0
 * - Defaults invalid or missing affinityBonus to 1.5
 * - Creates default entries for houses without multiplier data
 *
 * Uses fixture JSON file: assets/json/tests/houses_scaling_fallbacks.json
 */
void testScalingFallbacks() {
    ItemDatabase db;
    auto json = readJson("json/tests/houses_scaling_fallbacks.json");
    assertWithLabel(json != nullptr, "fallback: parse clamp/fallback house fixture");
    if (!json) return;

    assertWithLabel(db.loadHouseMultipliersFromJson(json), "fallback: house multipliers load with fallback defaults");

    const auto* clampHouseMultipliers = db.getHouseMultipliers("ClampHouse");
    const auto* missingHouseMultipliers = db.getHouseMultipliers("MissingHouse");
    assertWithLabel(clampHouseMultipliers != nullptr, "fallback: ClampHouse scaling exists");
    assertWithLabel(missingHouseMultipliers != nullptr, "fallback: MissingHouse scaling exists");
    if (clampHouseMultipliers) {
        assertWithLabel(floatsEqualWithinTolerance(clampHouseMultipliers->attack, 1.0f), "fallback: attack clamped to 1.0");
        assertWithLabel(floatsEqualWithinTolerance(clampHouseMultipliers->support, 0.0f), "fallback: support clamped to 0.0");
        assertWithLabel(floatsEqualWithinTolerance(clampHouseMultipliers->affinityBonus, 1.5f), "fallback: non-positive affinityBonus defaults to 1.5");
    }
    if (missingHouseMultipliers) {
        assertWithLabel(floatsEqualWithinTolerance(missingHouseMultipliers->attack, 0.0f), "fallback: missing attack defaults to 0.0");
        assertWithLabel(floatsEqualWithinTolerance(missingHouseMultipliers->support, 0.0f), "fallback: missing support defaults to 0.0");
        assertWithLabel(floatsEqualWithinTolerance(missingHouseMultipliers->affinityBonus, 1.5f), "fallback: missing affinityBonus defaults to 1.5");
    }
}

/**
 * Tests baseValue fallback behavior for missing or invalid item values.
 *
 * Verifies that ItemDatabase correctly:
 * - Defaults negative baseValue to 0.0
 * - Defaults missing baseValue to 0.0
 * - Preserves valid baseValue without modification
 *
 * Uses fixture JSON file: assets/json/tests/items_basevalue_fallbacks.json
 */
void testBaseValueDefaults() {
    ItemDatabase db;
    auto json = readJson("json/tests/items_basevalue_fallbacks.json");
    assertWithLabel(json != nullptr, "baseValue: parse fallback fixture");
    if (!json) return;

    assertWithLabel(db.loadFromJson(json), "baseValue: fixture loads with defaults");
    
    assertWithLabel(db.loadFromJson(json), "baseValue: fixture loads with defaults");
    auto negativeBaseValueDef = db.getDef("neg_base");
    auto missingBaseValueDef = db.getDef("missing_base");
    assertWithLabel(negativeBaseValueDef != nullptr && missingBaseValueDef != nullptr, "baseValue: fallback defs exist");
    if (negativeBaseValueDef) {
        assertWithLabel(floatsEqualWithinTolerance(negativeBaseValueDef->getBaseValue(), 0.0f), "baseValue: negative baseValue defaults to 0.0");
    }
    if (missingBaseValueDef) {
        assertWithLabel(floatsEqualWithinTolerance(missingBaseValueDef->getBaseValue(), 0.0f), "baseValue: missing baseValue defaults to 0.0");
    }
}

/**
 * Tests complete item usage computation pipeline with value-scaling scenarios.
 *
 * Verifies the full damage/healing calculation:
 *   resolvedValue = baseValue * (1 + houseSlider) * affinityBonus
 *
 * Tests multiple scenarios:
 * - Attack item with matching house affinity (should apply affinityBonus)
 * - Attack item without matching affinity (should skip affinityBonus)
 * - Support item heal on ally (should apply support slider)
 * - Item used on wrong target type (should return 0 but consume item)
 * - Missing item ID lookup (should return -1)
 *
 * @param itemsJson       Parsed JSON object containing item definitions
 * @param housesJson      Parsed JSON object containing house multipliers
 * @param housesJsonPath  Asset path to houses JSON for HouseLoader initialization
 * @param enemiesJsonPath Asset path to enemies JSON for Enemy initialization
 */
void testEffectiveValueComputation(const std::shared_ptr<cugl::JsonValue>& itemsJson,
                                   const std::shared_ptr<cugl::JsonValue>& housesJson,
                                   const std::string& housesJsonPath,
                                   const std::string& enemiesJsonPath) {
    ItemDatabase db;
    assertWithLabel(db.loadFromJson(itemsJson), "compute: item db load succeeds");
    assertWithLabel(db.loadHouseMultipliersFromJson(housesJson), "compute: house multipliers load succeeds");

    auto rareAttackDef = db.getDef("spear");
    assertWithLabel(rareAttackDef != nullptr, "compute: spear def exists");
    if (!rareAttackDef) return;

    HouseLoader loader;
    bool housesOk = loader.loadFromFile(housesJsonPath);
    assertWithLabel(housesOk, "compute: house loader init succeeds");

    Enemy enemy;
    bool enemyOk = enemy.init("cyclops", enemiesJsonPath);
    assertWithLabel(enemyOk, "compute: enemy init succeeds");
    if (!enemyOk) return;
    enemy.setTargetIndex(0);

    Player ares("ares", 2, "Ares Tester", loader);
    auto instAres = ItemInstance::alloc("spear", 1001);
    assertWithLabel(instAres != nullptr, "compute: create spear instance (Ares)");
    if (!instAres) return;
    ares.addItem(*instAres);

    enemy.setCurrentHealth(enemy.getMaxHealth());
    const float aresSideMultiplierBeforeUse = enemy.getSideMultiplier(ares.getPlayerNumber());
    float hpBeforeAres = enemy.getCurrentHealth();
    float resolvedAres = ares.useItemById(instAres->getId(), enemy, db);
    float expectedAres = rareAttackDef->getBaseValue() * (1.0f + 1.0f) * 1.5f;
    assertWithLabel(floatsEqualWithinTolerance(resolvedAres, expectedAres), "compute: matching rare affinity resolves correctly");
    assertWithLabel(floatsEqualWithinTolerance(hpBeforeAres - enemy.getCurrentHealth(), expectedAres * aresSideMultiplierBeforeUse), "compute: enemy damage equals resolved attack value");

    // Attack rare item without matching affinity (Poseidon + spear)
    Player poseidon("poseidon", 1, "Poseidon Tester", loader);
    auto instPoseidon = ItemInstance::alloc("spear", 1002);
    assertWithLabel(instPoseidon != nullptr, "compute: create spear instance (Poseidon)");
    if (!instPoseidon) return;
    poseidon.addItem(*instPoseidon);

    enemy.setCurrentHealth(enemy.getMaxHealth());
    enemy.clearRuntimeEffects();
    const float poseidonSideMultiplierBeforeUse = enemy.getSideMultiplier(poseidon.getPlayerNumber());
    float hpBeforePoseidon = enemy.getCurrentHealth();
    float resolvedPoseidon = poseidon.useItemById(instPoseidon->getId(), enemy, db);
    float expectedPoseidon = rareAttackDef->getBaseValue() * (1.0f + 0.75f);
    assertWithLabel(floatsEqualWithinTolerance(resolvedPoseidon, expectedPoseidon), "compute: non-matching rare affinity resolves correctly");
    assertWithLabel((hpBeforePoseidon - enemy.getCurrentHealth()) > 0.0f, "compute: enemy damage without affinity is correct");

    // Support common item (Demeter + apple) should not use affinity
    Player demeter("demeter", 3, "Demeter Tester", loader);
    Player ally("ares", 4, "Ally", loader);
    ally.updateHealth(-20.0f);

    auto instApple = ItemInstance::alloc("apple", 1003);
    assertWithLabel(instApple != nullptr, "compute: create apple instance");
    if (!instApple) return;
    demeter.addItem(*instApple);

    float allyBefore = ally.getCurrentHealth();
    float resolvedSupport = demeter.useItemById(instApple->getId(), ally, db);
    float expectedSupport = 10.0f * (1.0f + 1.0f);
    assertWithLabel(floatsEqualWithinTolerance(resolvedSupport, expectedSupport), "compute: support scaling resolves correctly");
    assertWithLabel(floatsEqualWithinTolerance(ally.getCurrentHealth() - allyBefore, expectedSupport), "compute: support heal equals resolved value");

    // Mismatched target type should return 0 and still consume item
    Player testAttacker("ares", 5, "Ares Tester 2", loader);
    Player testTarget("zeus", 6, "Zeus Target", loader);
    auto instAttack = ItemInstance::alloc("spear", 1004);
    assertWithLabel(instAttack != nullptr, "compute: create mismatch attack instance");
    if (!instAttack) return;
    testAttacker.addItem(*instAttack);
    float mismatchResult = testAttacker.useItemById(instAttack->getId(), testTarget, db);
    assertWithLabel(floatsEqualWithinTolerance(mismatchResult, 0.0f), "compute: attack on player returns 0.0");
    assertWithLabel(testAttacker.getInventory().empty(), "compute: mismatch target still consumes item");

    // Missing item id should fail with -1
    float missingItemResult = testAttacker.useItemById(999999, testTarget, db);
    assertWithLabel(floatsEqualWithinTolerance(missingItemResult, -1.0f), "compute: missing item id returns -1.0");
}

/**
 * Tests the shield item effect on a player target.
 *
 * Verifies that:
 * - Shield support items apply their base heal immediately
 * - Shield effects arm fixed mitigation with the configured magnitude and duration
 * - The next incoming hit consumes the shield and reduces damage by the mitigation amount
 * - Later hits apply normally after the shield has been consumed
 *
 * @param itemsJson       Parsed JSON object containing item definitions
 * @param housesJson      Parsed JSON object containing house multipliers
 * @param housesJsonPath  Asset path to houses JSON for HouseLoader initialization
 * @param enemiesJsonPath Asset path to enemies JSON for Enemy initialization
 */
void testShieldEffect(const std::shared_ptr<cugl::JsonValue>& itemsJson,
                      const std::shared_ptr<cugl::JsonValue>& housesJson,
                      const std::string& housesJsonPath,
                      const std::string& enemiesJsonPath) {
    ItemDatabase db;
    assertWithLabel(db.loadFromJson(itemsJson), "shield: item db load succeeds");
    assertWithLabel(db.loadHouseMultipliersFromJson(housesJson), "shield: house multipliers load succeeds");

    auto shieldDef = db.getDef("shield");
    assertWithLabel(shieldDef != nullptr, "shield: shield def exists");
    if (!shieldDef || shieldDef->getEffects().empty()) return;

    const ItemDef::Effect shieldEffect = shieldDef->getEffects()[0];

    HouseLoader loader;
    bool housesOk = loader.loadFromFile(housesJsonPath);
    assertWithLabel(housesOk, "shield: house loader init succeeds");

    Enemy enemy;
    bool enemyOk = enemy.init("cyclops", enemiesJsonPath);
    assertWithLabel(enemyOk, "shield: enemy init succeeds");
    if (!enemyOk) return;

    Player athena("athena", 7, "Athena Tester", loader);
    Player shieldTarget("ares", 8, "Shield Target", loader);
    shieldTarget.updateHealth(-5.0f);
    auto instShield = ItemInstance::alloc("shield", 1005);
    assertWithLabel(instShield != nullptr, "shield: create shield instance");
    if (!instShield) return;
    athena.addItem(*instShield);

    const float shieldHealthBeforeUse = shieldTarget.getCurrentHealth();
    float resolvedShield = athena.useItemById(instShield->getId(), shieldTarget, db);
    assertWithLabel(floatsEqualWithinTolerance(resolvedShield, shieldDef->getBaseValue() * (1.0f + 0.3f)),
                    "shield: shield item returns the expected resolved base heal");
    assertWithLabel(floatsEqualWithinTolerance(shieldTarget.getCurrentHealth() - shieldHealthBeforeUse,
                                              std::min(resolvedShield, shieldTarget.getMaxHealth() - shieldHealthBeforeUse)),
                    "shield: shield item still applies its base heal");
    assertWithLabel(shieldTarget.hasShield(), "shield: shield effect arms fixed mitigation");
    assertWithLabel(floatsEqualWithinTolerance(shieldTarget.getShieldHealth(), shieldEffect.mitigation), "shield: shield mitigation value applies");
    assertWithLabel(floatsEqualWithinTolerance(shieldTarget.getShieldDuration(), shieldEffect.duration), "shield: shield effect duration applies");

    float shieldedHealthBefore = shieldTarget.getCurrentHealth();
    shieldTarget.updateHealth(-6.0f);
    assertWithLabel(floatsEqualWithinTolerance(shieldTarget.getCurrentHealth(), shieldedHealthBefore), "shield: shield mitigates fixed damage from the next hit");
    assertWithLabel(shieldTarget.hasShield(), "shield: shield remains active while mitigation remains");

    shieldTarget.updateHealth(-2.0f);
    assertWithLabel(floatsEqualWithinTolerance(shieldTarget.getCurrentHealth(), shieldedHealthBefore), "shield: later hits continue consuming remaining shield mitigation");
}

/**
 * Tests the barrier item effect on a player target.
 *
 * Verifies that:
 * - Barrier support items apply their base heal immediately
 * - Barrier effects arm percentage mitigation with the configured multiplier and duration
 * - The next incoming hit is reduced by the active multiplier
 * - The barrier is consumed after mitigating one hit
 *
 * @param itemsJson       Parsed JSON object containing item definitions
 * @param housesJson      Parsed JSON object containing house multipliers
 * @param housesJsonPath  Asset path to houses JSON for HouseLoader initialization
 * @param enemiesJsonPath Asset path to enemies JSON for Enemy initialization
 */
void testBarrierEffect(const std::shared_ptr<cugl::JsonValue>& itemsJson,
                       const std::shared_ptr<cugl::JsonValue>& housesJson,
                       const std::string& housesJsonPath,
                       const std::string& enemiesJsonPath) {
    ItemDatabase db;
    assertWithLabel(db.loadFromJson(itemsJson), "barrier: item db load succeeds");
    assertWithLabel(db.loadHouseMultipliersFromJson(housesJson), "barrier: house multipliers load succeeds");

    auto barrierDef = db.getDef("aegis");
    assertWithLabel(barrierDef != nullptr, "barrier: aegis def exists");
    if (!barrierDef || barrierDef->getEffects().empty()) return;

    const ItemDef::Effect barrierEffect = barrierDef->getEffects()[0];

    HouseLoader loader;
    bool housesOk = loader.loadFromFile(housesJsonPath);
    assertWithLabel(housesOk, "barrier: house loader init succeeds");

    Enemy enemy;
    bool enemyOk = enemy.init("cyclops", enemiesJsonPath);
    assertWithLabel(enemyOk, "barrier: enemy init succeeds");
    if (!enemyOk) return;

    Player athena("athena", 7, "Athena Tester", loader);
    Player barrierTarget("ares", 9, "Barrier Target", loader);
    barrierTarget.updateHealth(-2.0f);
    auto instBarrier = ItemInstance::alloc("aegis", 1006);
    assertWithLabel(instBarrier != nullptr, "barrier: create barrier instance");
    if (!instBarrier) return;
    athena.addItem(*instBarrier);

    const float barrierHealthBeforeUse = barrierTarget.getCurrentHealth();
    float resolvedBarrier = athena.useItemById(instBarrier->getId(), barrierTarget, db);
    assertWithLabel(floatsEqualWithinTolerance(barrierTarget.getCurrentHealth() - barrierHealthBeforeUse,
                                              std::min(resolvedBarrier, barrierTarget.getMaxHealth() - barrierHealthBeforeUse)),
                    "barrier: barrier item still applies its base heal");
    assertWithLabel(barrierTarget.hasBarrier(), "barrier: barrier effect arms percentage mitigation");
    assertWithLabel(floatsEqualWithinTolerance(barrierTarget.getBarrierMultiplier(), barrierEffect.multiplier), "barrier: barrier multiplier value applies");
    assertWithLabel(floatsEqualWithinTolerance(barrierTarget.getBarrierDuration(), barrierEffect.duration), "barrier: barrier duration applies");

    float barrierHealthBefore = barrierTarget.getCurrentHealth();
    barrierTarget.updateHealth(-6.0f);
    const float expectedBarrierHealthAfterHit = std::max(0.0f, barrierHealthBefore - (6.0f * barrierEffect.multiplier));
    assertWithLabel(floatsEqualWithinTolerance(barrierTarget.getCurrentHealth(), expectedBarrierHealthAfterHit), "barrier: barrier mitigates the next hit by percentage");
    assertWithLabel(barrierTarget.hasBarrier(), "barrier: barrier remains active while duration remains");
}

/**
 * Tests the helm item effect on a player target.
 *
 * Verifies that:
 * - Helm applies a barrier effect with zero damage multiplier
 * - The configured long-duration barrier values are preserved on the target
 * - The next incoming hit is fully negated
 * - Later hits use the neutral multiplier after the first protected hit
 *
 * @param itemsJson       Parsed JSON object containing item definitions
 * @param housesJson      Parsed JSON object containing house multipliers
 * @param housesJsonPath  Asset path to houses JSON for HouseLoader initialization
 * @param enemiesJsonPath Asset path to enemies JSON for Enemy initialization
 */
void testRegenEffect(const std::shared_ptr<cugl::JsonValue>& itemsJson,
                     const std::shared_ptr<cugl::JsonValue>& housesJson,
                     const std::string& housesJsonPath,
                     const std::string& enemiesJsonPath) {
    (void)enemiesJsonPath;

    ItemDatabase db;
    assertWithLabel(db.loadFromJson(itemsJson), "regen: item db load succeeds");
    assertWithLabel(db.loadHouseMultipliersFromJson(housesJson), "regen: house multipliers load succeeds");

    auto wheatDef = db.getDef("wheat");
    assertWithLabel(wheatDef != nullptr, "regen: wheat def exists");
    if (!wheatDef || wheatDef->getEffects().empty()) return;

    const ItemDef::Effect regenEffect = wheatDef->getEffects()[0];

    HouseLoader loader;
    bool housesOk = loader.loadFromFile(housesJsonPath);
    assertWithLabel(housesOk, "regen: house loader init succeeds");

    Player demeter("demeter", 10, "Demeter Tester", loader);
    Player regenTarget("ares", 11, "Regen Target", loader);
    regenTarget.updateHealth(-80.0f);

    auto instWheat = ItemInstance::alloc("wheat", 1007);
    assertWithLabel(instWheat != nullptr, "regen: create wheat instance");
    if (!instWheat) return;
    demeter.addItem(*instWheat);

    const float healthBeforeUse = regenTarget.getCurrentHealth();
    float resolvedHeal = demeter.useItemById(instWheat->getId(), regenTarget, db);
    const float expectedImmediateHeal = wheatDef->getBaseValue() * (1.0f + 1.0f) * 1.5f;
    assertWithLabel(floatsEqualWithinTolerance(resolvedHeal, expectedImmediateHeal),
                    "regen: wheat item returns the expected resolved base heal");
    assertWithLabel(floatsEqualWithinTolerance(regenTarget.getCurrentHealth() - healthBeforeUse, expectedImmediateHeal),
                    "regen: wheat still applies its base heal immediately");
    assertWithLabel(regenTarget.hasRegen(), "regen: wheat applies a timed regen effect");
    assertWithLabel(floatsEqualWithinTolerance(regenTarget.getRegenAmountRemaining(), regenEffect.regenAmount),
                    "regen: regen amount tracks remaining healing");
    assertWithLabel(floatsEqualWithinTolerance(regenTarget.getRegenDuration(), regenEffect.duration),
                    "regen: regen duration applies");

    const float healthBeforeTick = regenTarget.getCurrentHealth();
    regenTarget.updateEffects(4.0f);
    assertWithLabel(floatsEqualWithinTolerance(regenTarget.getCurrentHealth() - healthBeforeTick, 5.0f),
                    "regen: ticking effects heals proportionally over time");
    assertWithLabel(floatsEqualWithinTolerance(regenTarget.getRegenAmountRemaining(), 5.0f),
                    "regen: remaining amount decreases after ticking");
    assertWithLabel(floatsEqualWithinTolerance(regenTarget.getRegenDuration(), 1.0f),
                    "regen: remaining duration decreases after ticking");

    regenTarget.updateEffects(1.0f);
    assertWithLabel(!regenTarget.hasRegen(), "regen: regen expires after its duration completes");
}

/**
 * Tests the helm item effect on a player target.
 *
 * Verifies that:
 * - Helm applies a barrier effect with zero damage multiplier
 * - The configured long-duration barrier values are preserved on the target
 * - The next incoming hit is fully negated
 * - Later hits use the neutral multiplier after the first protected hit
 *
 * @param itemsJson       Parsed JSON object containing item definitions
 * @param housesJson      Parsed JSON object containing house multipliers
 * @param housesJsonPath  Asset path to houses JSON for HouseLoader initialization
 * @param enemiesJsonPath Asset path to enemies JSON for Enemy initialization
 */
void testHelmEffect(const std::shared_ptr<cugl::JsonValue>& itemsJson,
                    const std::shared_ptr<cugl::JsonValue>& housesJson,
                    const std::string& housesJsonPath,
                    const std::string& enemiesJsonPath) {
    ItemDatabase db;
    assertWithLabel(db.loadFromJson(itemsJson), "helm: item db load succeeds");
    assertWithLabel(db.loadHouseMultipliersFromJson(housesJson), "helm: house multipliers load succeeds");

    auto helmDef = db.getDef("helm");
    assertWithLabel(helmDef != nullptr, "helm: helm def exists");
    if (!helmDef || helmDef->getEffects().empty()) return;

    const ItemDef::Effect helmEffect = helmDef->getEffects()[0];

    HouseLoader loader;
    bool housesOk = loader.loadFromFile(housesJsonPath);
    assertWithLabel(housesOk, "helm: house loader init succeeds");

    Enemy enemy;
    bool enemyOk = enemy.init("cyclops", enemiesJsonPath);
    assertWithLabel(enemyOk, "helm: enemy init succeeds");
    if (!enemyOk) return;

    Player hades("hades", 11, "Hades Tester", loader);
    Player helmTarget("ares", 12, "Helm Target", loader);
    helmTarget.updateHealth(-8.0f);
    auto instHelm = ItemInstance::alloc("helm", 1011);
    assertWithLabel(instHelm != nullptr, "helm: create helm instance");
    if (!instHelm) return;
    hades.addItem(*instHelm);

    const float helmHealthBeforeUse = helmTarget.getCurrentHealth();
    const float resolvedHelm = hades.useItemById(instHelm->getId(), helmTarget, db);
    assertWithLabel(helmTarget.hasBarrier(), "helm: helm arms a barrier on the target");
    assertWithLabel(floatsEqualWithinTolerance(helmTarget.getBarrierMultiplier(), helmEffect.multiplier), "helm: helm barrier multiplier applies");
    assertWithLabel(floatsEqualWithinTolerance(helmTarget.getBarrierDuration(), helmEffect.duration), "helm: helm barrier duration applies");

    const float firstHitHealthBefore = helmTarget.getCurrentHealth();
    helmTarget.updateHealth(-6.0f);
    assertWithLabel(floatsEqualWithinTolerance(helmTarget.getCurrentHealth(), firstHitHealthBefore), "helm: first hit is fully negated");
    assertWithLabel(!helmTarget.hasBarrier(), "helm: barrier does not remain active after the first protected hit");
    assertWithLabel(floatsEqualWithinTolerance(helmTarget.getBarrierMultiplier(), 1.0f), "helm: barrier multiplier resets after protecting one hit");

    const float secondHitHealthBefore = helmTarget.getCurrentHealth();
    helmTarget.updateHealth(-6.0f);
    assertWithLabel(floatsEqualWithinTolerance(secondHitHealthBefore - helmTarget.getCurrentHealth(), 6.0f), "helm: later hits use the neutral barrier multiplier");
}

/**
 * Tests that shield and barrier can coexist on the same target.
 *
 * Verifies that:
 * - A target can hold both shield and barrier effects at the same time
 * - Barrier mitigation is applied before shield mitigation on the same hit
 * - Both effects are consumed when they mitigate that incoming hit
 *
 * @param itemsJson       Parsed JSON object containing item definitions
 * @param housesJson      Parsed JSON object containing house multipliers
 * @param housesJsonPath  Asset path to houses JSON for HouseLoader initialization
 * @param enemiesJsonPath Asset path to enemies JSON for Enemy initialization
 */
void testShieldBarrierCoexistence(const std::shared_ptr<cugl::JsonValue>& itemsJson,
                                  const std::shared_ptr<cugl::JsonValue>& housesJson,
                                  const std::string& housesJsonPath,
                                  const std::string& enemiesJsonPath) {
    ItemDatabase db;
    assertWithLabel(db.loadFromJson(itemsJson), "layered: item db load succeeds");
    assertWithLabel(db.loadHouseMultipliersFromJson(housesJson), "layered: house multipliers load succeeds");

    auto shieldDef = db.getDef("shield");
    auto barrierDef = db.getDef("aegis");
    assertWithLabel(shieldDef != nullptr && barrierDef != nullptr, "layered: shield and aegis defs exist");
    if (!shieldDef || !barrierDef || shieldDef->getEffects().empty() || barrierDef->getEffects().empty()) return;

    const ItemDef::Effect shieldEffect = shieldDef->getEffects()[0];
    const ItemDef::Effect barrierEffect = barrierDef->getEffects()[0];

    HouseLoader loader;
    bool housesOk = loader.loadFromFile(housesJsonPath);
    assertWithLabel(housesOk, "layered: house loader init succeeds");

    Enemy enemy;
    bool enemyOk = enemy.init("cyclops", enemiesJsonPath);
    assertWithLabel(enemyOk, "layered: enemy init succeeds");
    if (!enemyOk) return;

    Player athena("athena", 7, "Athena Tester", loader);
    Player layeredTarget("ares", 10, "Layered Target", loader);
    auto layeredShield = ItemInstance::alloc("shield", 1007);
    auto layeredBarrier = ItemInstance::alloc("aegis", 1008);
    assertWithLabel(layeredShield != nullptr && layeredBarrier != nullptr, "layered: create coexistence shield and barrier instances");
    if (!layeredShield || !layeredBarrier) return;
    athena.addItem(*layeredShield);
    athena.addItem(*layeredBarrier);

    athena.useItemById(layeredShield->getId(), layeredTarget, db);
    athena.useItemById(layeredBarrier->getId(), layeredTarget, db);
    assertWithLabel(layeredTarget.hasShield() && layeredTarget.hasBarrier(), "layered: shield and barrier coexist on the same target");

    float layeredHealthBefore = layeredTarget.getCurrentHealth();
    layeredTarget.updateHealth(-10.0f);
    const float expectedDamage = std::max(0.0f, (10.0f * barrierEffect.multiplier) - shieldEffect.mitigation);
    const float expectedLayeredHealthAfterHit = std::max(0.0f, layeredHealthBefore - expectedDamage);
    assertWithLabel(floatsEqualWithinTolerance(layeredTarget.getCurrentHealth(), expectedLayeredHealthAfterHit), "layered: barrier then shield mitigation apply on the same hit");
    assertWithLabel(layeredTarget.hasShield() && layeredTarget.hasBarrier(), "layered: shield and barrier remain active while duration or mitigation remains");
}

/**
 * Tests the stun attack effect on the enemy.
 *
 * Verifies that:
 * - Stun attack items still apply their base damage
 * - The enemy enters the stunned state with the configured duration
 * - The enemy remains in the same state while stunned
 * - Enemy state timers do not advance while stunned
 * - The stun state clears after the duration elapses and timers resume
 *
 * @param itemsJson       Parsed JSON object containing item definitions
 * @param housesJson      Parsed JSON object containing house multipliers
 * @param housesJsonPath  Asset path to houses JSON for HouseLoader initialization
 * @param enemiesJsonPath Asset path to enemies JSON for Enemy initialization
 */
void testStunEffect(const std::shared_ptr<cugl::JsonValue>& itemsJson,
                    const std::shared_ptr<cugl::JsonValue>& housesJson,
                    const std::string& housesJsonPath,
                    const std::string& enemiesJsonPath) {
    ItemDatabase db;
    assertWithLabel(db.loadFromJson(itemsJson), "stun: item db load succeeds");
    assertWithLabel(db.loadHouseMultipliersFromJson(housesJson), "stun: house multipliers load succeeds");

    HouseLoader loader;
    bool housesOk = loader.loadFromFile(housesJsonPath);
    assertWithLabel(housesOk, "stun: house loader init succeeds");

    Enemy enemy;
    bool enemyOk = enemy.init("cyclops", enemiesJsonPath);
    assertWithLabel(enemyOk, "stun: enemy init succeeds");
    if (!enemyOk) return;
    enemy.setTargetIndex(0);

    Player zeus("zeus", 3, "Zeus Tester", loader);
    auto instLightning = ItemInstance::alloc("lightning_bolt", 1009);
    assertWithLabel(instLightning != nullptr, "stun: create lightning_bolt instance");
    if (!instLightning) return;
    zeus.addItem(*instLightning);

    enemy.setStateTime(1.25f);
    const EnemyLoader::State stateBeforeStun = enemy.getCurrentState();
    enemy.setCurrentHealth(enemy.getMaxHealth());
    enemy.clearRuntimeEffects();
    const float enemyHealthBeforeStunUse = enemy.getCurrentHealth();
    float resolvedStun = zeus.useItemById(instLightning->getId(), enemy, db);
    assertWithLabel(resolvedStun > 0.0f, "stun: stun item returns a positive base damage");
    assertWithLabel((enemyHealthBeforeStunUse - enemy.getCurrentHealth()) > 0.0f,
                    "stun: stun item still applies its base damage");
    assertWithLabel(enemy.isStunned(), "stun: stun effect marks enemy as stunned");
    assertWithLabel(floatsEqualWithinTolerance(enemy.getStunDuration(), 2.0f), "stun: stun duration applies to enemy");
    assertWithLabel(enemy.getCurrentState() == stateBeforeStun, "stun: enemy state is preserved when stunned");

    enemy.update(1.0f);
    assertWithLabel(enemy.isStunned(), "stun: enemy remains stunned before duration expires");
    assertWithLabel(enemy.getCurrentState() == stateBeforeStun, "stun: enemy state remains unchanged during stun");
    assertWithLabel(floatsEqualWithinTolerance(enemy.getStateTime(), 1.25f), "stun: enemy state timer is frozen during stun");

    enemy.update(1.1f);
    assertWithLabel(!enemy.isStunned(), "stun: enemy stun expires after duration elapses");
    enemy.update(0.5f);
    assertWithLabel(floatsEqualWithinTolerance(enemy.getStateTime(), 1.85f), "stun: enemy state timer resumes after stun ends");

    Player hades("hades", 4, "Hades Tester", loader);
    auto instOffAffinityLightning = ItemInstance::alloc("lightning_bolt", 1019);
    assertWithLabel(instOffAffinityLightning != nullptr, "stun: create off-affinity lightning_bolt instance");
    if (!instOffAffinityLightning) return;
    hades.addItem(*instOffAffinityLightning);

    enemy.setCurrentHealth(enemy.getMaxHealth());
    enemy.clearRuntimeEffects();
    enemy.setStateTime(0.5f);
    const float enemyHealthBeforeOffAffinityUse = enemy.getCurrentHealth();
    const float resolvedOffAffinityStun = hades.useItemById(instOffAffinityLightning->getId(), enemy, db);
    assertWithLabel(resolvedOffAffinityStun > 0.0f, "stun: off-affinity lightning still resolves positive base damage");
    assertWithLabel((enemyHealthBeforeOffAffinityUse - enemy.getCurrentHealth()) > 0.0f,
                    "stun: off-affinity lightning still applies its base damage");
    assertWithLabel(!enemy.isStunned(), "stun: off-affinity lightning does not apply stun");
}

/**
 * Tests the direct love effect on the enemy.
 *
 * Verifies that:
 * - Love immediately forces the enemy into idle
 * - Love keeps the enemy loved for the configured duration
 * - The love state clears after the duration elapses
 *
 * @param enemiesJsonPath Asset path to enemies JSON for Enemy initialization
 */
void testLoveEffect(const std::shared_ptr<cugl::JsonValue>& itemsJson,
                    const std::shared_ptr<cugl::JsonValue>& housesJson,
                    const std::string& housesJsonPath,
                    const std::string& enemiesJsonPath) {
    ItemDatabase db;
    assertWithLabel(db.loadFromJson(itemsJson), "love: item db load succeeds");
    assertWithLabel(db.loadHouseMultipliersFromJson(housesJson), "love: house multipliers load succeeds");

    HouseLoader loader;
    bool housesOk = loader.loadFromFile(housesJsonPath);
    assertWithLabel(housesOk, "love: house loader init succeeds");

    auto potionDef = db.getDef("potion");
    assertWithLabel(potionDef != nullptr, "love: potion def exists");
    if (!potionDef || !housesOk) return;

    Player aphrodite("aphrodite", 2, "Aphrodite Tester", loader);
    auto instPotion = ItemInstance::alloc("potion", 1011);
    assertWithLabel(instPotion != nullptr, "love: create potion instance");
    if (!instPotion) return;
    aphrodite.addItem(*instPotion);

    Enemy enemy;
    bool enemyOk = enemy.init("cyclops", enemiesJsonPath);
    assertWithLabel(enemyOk, "love: enemy init succeeds");
    if (!enemyOk) return;

    for (const auto& stateEntry : enemy.getStates()) {
        if (stateEntry.first != EnemyLoader::State::IDLE) {
            enemy.enterState(stateEntry.first);
            break;
        }
    }
    enemy.setStateTime(0.75f);
    enemy.clearRuntimeEffects();
    enemy.setTargetIndex(0);
    const float resolvedMagnitude = aphrodite.useItemById(instPotion->getId(), enemy, db);
    assertWithLabel(resolvedMagnitude > 0.0f, "love: potion use resolves attack magnitude");
    assertWithLabel(enemy.isLoved(), "love: potion marks enemy as loved");
    assertWithLabel(enemy.getCurrentState() == EnemyLoader::State::IDLE, "love: love forces enemy into idle");
    assertWithLabel(enemy.getTargetIndex() == aphrodite.getPlayerNumber(), "love: enemy turns to face potion user");
    assertWithLabel(floatsEqualWithinTolerance(enemy.getLoveDuration(), 5.0f), "love: configured duration applies to enemy");

    enemy.update(1.0f);
    assertWithLabel(enemy.isLoved(), "love: enemy remains loved before duration expires");
    assertWithLabel(enemy.getTargetIndex() == aphrodite.getPlayerNumber(), "love: enemy keeps facing loved target during duration");

    enemy.update(4.1f);
    assertWithLabel(!enemy.isLoved(), "love: enemy love expires after duration elapses");
}

/**
 * Tests the vulnerable attack effect on the enemy.
 *
 * Verifies that:
 * - Vulnerable attack items still apply their base damage
 * - The enemy enters the vulnerable state with the configured multiplier and duration
 * - Incoming damage is amplified while vulnerable remains active
 * - The vulnerable state clears after the duration elapses
 *
 * @param itemsJson       Parsed JSON object containing item definitions
 * @param housesJson      Parsed JSON object containing house multipliers
 * @param housesJsonPath  Asset path to houses JSON for HouseLoader initialization
 * @param enemiesJsonPath Asset path to enemies JSON for Enemy initialization
 */
void testVulnerableEffect(const std::shared_ptr<cugl::JsonValue>& itemsJson,
                          const std::shared_ptr<cugl::JsonValue>& housesJson,
                          const std::string& housesJsonPath,
                          const std::string& enemiesJsonPath) {
    ItemDatabase db;
    assertWithLabel(db.loadFromJson(itemsJson), "vulnerable: item db load succeeds");
    assertWithLabel(db.loadHouseMultipliersFromJson(housesJson), "vulnerable: house multipliers load succeeds");

    auto vulnerableDef = db.getDef("spear");
    assertWithLabel(vulnerableDef != nullptr, "vulnerable: spear def exists");
    if (!vulnerableDef || vulnerableDef->getEffects().empty()) return;

    const ItemDef::Effect vulnerableEffect = vulnerableDef->getEffects()[0];
    assertWithLabel(!vulnerableEffect.applyToAllSides, "vulnerable: spear vulnerable effect stays single-side");

    HouseLoader loader;
    bool housesOk = loader.loadFromFile(housesJsonPath);
    assertWithLabel(housesOk, "vulnerable: house loader init succeeds");

    Enemy enemy;
    bool enemyOk = enemy.init("cyclops", enemiesJsonPath);
    assertWithLabel(enemyOk, "vulnerable: enemy init succeeds");
    if (!enemyOk) return;

    Player ares("ares", 2, "Ares Tester", loader);
    auto instSpear = ItemInstance::alloc("spear", 1010);
    assertWithLabel(instSpear != nullptr, "vulnerable: create spear instance");
    if (!instSpear) return;
    ares.addItem(*instSpear);

    enemy.setCurrentHealth(enemy.getMaxHealth());
    enemy.clearRuntimeEffects();
    enemy.setTargetIndex(0);
    const float vulnerableSideMultiplierBeforeUse = enemy.getSideMultiplier(ares.getPlayerNumber());
    const float enemyHealthBeforeVulnerableUse = enemy.getCurrentHealth();
    const float resolvedVulnerable = ares.useItemById(instSpear->getId(), enemy, db);
    assertWithLabel(resolvedVulnerable > 0.0f, "vulnerable: vulnerable item returns a positive base damage");
    assertWithLabel(floatsEqualWithinTolerance(enemyHealthBeforeVulnerableUse - enemy.getCurrentHealth(), resolvedVulnerable * vulnerableSideMultiplierBeforeUse),
                    "vulnerable: vulnerable item still applies its base damage");
    assertWithLabel(enemy.isVulnerable(), "vulnerable: vulnerable effect marks enemy as vulnerable");
    assertWithLabel(floatsEqualWithinTolerance(enemy.getVulnerableMultiplier(), vulnerableEffect.multiplier), "vulnerable: vulnerable multiplier applies to enemy");
    assertWithLabel(floatsEqualWithinTolerance(enemy.getVulnerableDuration(), vulnerableEffect.duration), "vulnerable: vulnerable duration applies to enemy");
    assertWithLabel(floatsEqualWithinTolerance(enemy.getVulnerableMultiplierForSide(2), vulnerableEffect.multiplier), "vulnerable: hit side receives vulnerable multiplier");
    assertWithLabel(floatsEqualWithinTolerance(enemy.getVulnerableDurationForSide(2), vulnerableEffect.duration), "vulnerable: hit side stores vulnerable timer");
    assertWithLabel(floatsEqualWithinTolerance(enemy.getVulnerableMultiplierForSide(1), 1.0f), "vulnerable: untouched side stays neutral");

    const float vulnerableHealthBefore = enemy.getCurrentHealth();
    const float matchingSideMultiplier = enemy.getSideMultiplier(2);
    enemy.takeDamage(5.0f, 2);
    assertWithLabel(floatsEqualWithinTolerance(vulnerableHealthBefore - enemy.getCurrentHealth(), 5.0f * matchingSideMultiplier), "vulnerable: matching side increases incoming damage while active");

    const float nonVulnerableHealthBefore = enemy.getCurrentHealth();
    const float neutralSideMultiplier = enemy.getSideMultiplier(1);
    enemy.takeDamage(5.0f, 1);
    assertWithLabel(floatsEqualWithinTolerance(nonVulnerableHealthBefore - enemy.getCurrentHealth(), 5.0f * neutralSideMultiplier), "vulnerable: other sides remain neutral");

    enemy.setTargetIndex(1);
    const float turnedEnemyHealthBefore = enemy.getCurrentHealth();
    const float turnedMatchingSideMultiplier = enemy.getSideMultiplier(3);
    enemy.takeDamage(5.0f, 3);
    assertWithLabel(floatsEqualWithinTolerance(turnedEnemyHealthBefore - enemy.getCurrentHealth(), 5.0f * turnedMatchingSideMultiplier), "vulnerable: vulnerable side follows the enemy when it turns");

    enemy.update(vulnerableEffect.duration + 0.1f);
    assertWithLabel(!enemy.isVulnerable(), "vulnerable: vulnerable expires after duration elapses");
}

/**
 * Tests the slow attack effect on the enemy.
 *
 * Verifies that:
 * - Slow attack items still apply their base damage
 * - Slow scales only enemy state-time advancement
 * - Slow duration expires in real time instead of slowed time
 * - Other timers such as attack lockout continue advancing normally
 *
 * @param itemsJson       Parsed JSON object containing item definitions
 * @param housesJson      Parsed JSON object containing house multipliers
 * @param housesJsonPath  Asset path to houses JSON for HouseLoader initialization
 * @param enemiesJsonPath Asset path to enemies JSON for Enemy initialization
 */
void testSlowEffect(const std::shared_ptr<cugl::JsonValue>& itemsJson,
                    const std::shared_ptr<cugl::JsonValue>& housesJson,
                    const std::string& housesJsonPath,
                    const std::string& enemiesJsonPath) {
    ItemDatabase db;
    assertWithLabel(db.loadFromJson(itemsJson), "slow: item db load succeeds");
    assertWithLabel(db.loadHouseMultipliersFromJson(housesJson), "slow: house multipliers load succeeds");

    auto wingsDef = db.getDef("wings");
    assertWithLabel(wingsDef != nullptr, "slow: wings def exists");
    if (!wingsDef || wingsDef->getEffects().empty()) return;

    const ItemDef::Effect slowEffect = wingsDef->getEffects()[0];
    assertWithLabel(slowEffect.type == ItemDef::EffectType::Slow, "slow: wings slow effect is first");

    HouseLoader loader;
    bool housesOk = loader.loadFromFile(housesJsonPath);
    assertWithLabel(housesOk, "slow: house loader init succeeds");

    Enemy enemy;
    bool enemyOk = enemy.init("cyclops", enemiesJsonPath);
    assertWithLabel(enemyOk, "slow: enemy init succeeds");
    if (!enemyOk) return;

    Player hermes("hermes", 2, "Hermes Tester", loader);
    auto instWings = ItemInstance::alloc("wings", 3030);
    assertWithLabel(instWings != nullptr, "slow: create wings instance");
    if (!instWings) return;
    hermes.addItem(*instWings);

    enemy.setCurrentHealth(enemy.getMaxHealth());
    enemy.clearRuntimeEffects();
    enemy.setTargetIndex(0);
    enemy.enterState(EnemyLoader::State::ATTACK_1);
    enemy.setStateTime(1.0f);
    const float enemyHealthBeforeUse = enemy.getCurrentHealth();
    const float resolvedSlow = hermes.useItemById(instWings->getId(), enemy, db);
    assertWithLabel(resolvedSlow > 0.0f, "slow: wings returns a positive base damage");
    assertWithLabel((enemyHealthBeforeUse - enemy.getCurrentHealth()) > 0.0f,
                    "slow: wings still applies its base damage");
    assertWithLabel(enemy.isSlowed(), "slow: enemy is marked slowed");
    assertWithLabel(floatsEqualWithinTolerance(enemy.getSlowMultiplier(), 0.5f), "slow: slow multiplier applies");
    assertWithLabel(floatsEqualWithinTolerance(enemy.getSlowDuration(), 5.0f), "slow: slow duration applies");

    enemy.update(1.0f);
    assertWithLabel(floatsEqualWithinTolerance(enemy.getSlowDuration(), 4.0f), "slow: slow timer still counts down in real time");
}

/**
 * Verifies that trident applies vulnerability to all four boss sides.
 *
 * @param itemsJson        Parsed items fixture JSON.
 * @param housesJson       Parsed houses fixture JSON.
 * @param housesJsonPath   Path to the houses fixture used for loading Player house data.
 * @param enemiesJsonPath  Path to the enemies fixture used for loading the test enemy.
 * @return None.
 */
void testTridentVulnerableAllSides(const std::shared_ptr<cugl::JsonValue>& itemsJson,
                                   const std::shared_ptr<cugl::JsonValue>& housesJson,
                                   const std::string& housesJsonPath,
                                   const std::string& enemiesJsonPath) {
    ItemDatabase db;
    assertWithLabel(db.loadFromJson(itemsJson), "trident vulnerable: item db load succeeds");
    assertWithLabel(db.loadHouseMultipliersFromJson(housesJson), "trident vulnerable: house multipliers load succeeds");

    auto tridentDef = db.getDef("trident");
    assertWithLabel(tridentDef != nullptr, "trident vulnerable: trident def exists");
    if (!tridentDef || tridentDef->getEffects().empty()) return;

    const ItemDef::Effect vulnerableEffect = tridentDef->getEffects()[0];
    assertWithLabel(vulnerableEffect.applyToAllSides, "trident vulnerable: trident vulnerable effect is marked as all-sides");

    HouseLoader loader;
    bool housesOk = loader.loadFromFile(housesJsonPath);
    assertWithLabel(housesOk, "trident vulnerable: house loader init succeeds");

    Enemy enemy;
    bool enemyOk = enemy.init("cyclops", enemiesJsonPath);
    assertWithLabel(enemyOk, "trident vulnerable: enemy init succeeds");
    if (!enemyOk) return;

    Player poseidon("poseidon", 2, "Poseidon Tester", loader);
    auto instTrident = ItemInstance::alloc("trident", 2020);
    assertWithLabel(instTrident != nullptr, "trident vulnerable: create trident instance");
    if (!instTrident) return;
    poseidon.addItem(*instTrident);

    enemy.setCurrentHealth(enemy.getMaxHealth());
    enemy.clearRuntimeEffects();
    enemy.setTargetIndex(0);
    const float enemyHealthBeforeUse = enemy.getCurrentHealth();
    const float resolvedVulnerable = poseidon.useItemById(instTrident->getId(), enemy, db);
    assertWithLabel(resolvedVulnerable > 0.0f, "trident vulnerable: trident returns a positive base damage");
    assertWithLabel((enemyHealthBeforeUse - enemy.getCurrentHealth()) > 0.0f, "trident vulnerable: trident still applies its base damage");

    for (int side = 0; side < Enemy::NUM_PLAYERS; side++) {
        assertWithLabel(floatsEqualWithinTolerance(enemy.getVulnerableMultiplierForSide(side), vulnerableEffect.multiplier),
                        "trident vulnerable: every side receives the vulnerable multiplier");
        assertWithLabel(floatsEqualWithinTolerance(enemy.getVulnerableDurationForSide(side), vulnerableEffect.duration),
                        "trident vulnerable: every side receives the vulnerable duration");
    }
}

/**
 * Tests that Gaia's rock heals the boss instead of dealing damage.
 *
 * Verifies that:
 * - Gaia's rock item exists in the database
 * - Using it returns a positive heal value
 * - The boss gains health equal to the resolved heal amount
 * - The item is consumed from the player's inventory after use
 *
 * @param itemsJson       Parsed JSON object containing item definitions
 * @param housesJsonPath  Asset path to houses JSON for HouseLoader initialization
 * @param enemiesJsonPath Asset path to enemies JSON for Enemy initialization
 */
void testGaiaRockHealsEnemy(const std::shared_ptr<cugl::JsonValue>& itemsJson,
    const std::string& housesJsonPath,
    const std::string& enemiesJsonPath) {
    ItemDatabase db;
    assertWithLabel(db.loadFromJson(itemsJson), "gaia_rock: item db load succeeds");

    auto gaiaRockDef = db.getDef("gaia_rock");
    assertWithLabel(gaiaRockDef != nullptr, "gaia_rock: gaia_rock def exists");
    if (!gaiaRockDef) return;

    HouseLoader loader;
    bool housesOk = loader.loadFromFile(housesJsonPath);
    assertWithLabel(housesOk, "gaia_rock: house loader init succeeds");

    Enemy enemy;
    bool enemyOk = enemy.init("cyclops", enemiesJsonPath);
    assertWithLabel(enemyOk, "gaia_rock: enemy init succeeds");
    if (!enemyOk) return;

    // Damage the boss first so there is room to heal
    enemy.setCurrentHealth(enemy.getMaxHealth() - 50.0f);

    Player attacker("zeus", 0, "Zeus Tester", loader);
    auto instRock = ItemInstance::alloc("gaia_rock", 2001);
    assertWithLabel(instRock != nullptr, "gaia_rock: create gaia_rock instance");
    if (!instRock) return;
    attacker.addItem(*instRock);

    const float enemyHealthBefore = enemy.getCurrentHealth();
    const float resolvedHeal = attacker.useItemById(instRock->getId(), enemy, db);
    assertWithLabel(resolvedHeal > 0.0f, "gaia_rock: useItemById returns a positive heal amount");
    assertWithLabel(floatsEqualWithinTolerance(enemy.getCurrentHealth() - enemyHealthBefore, resolvedHeal),
        "gaia_rock: enemy gains health equal to the resolved heal amount");
    assertWithLabel(attacker.getInventory().empty(), "gaia_rock: item is consumed from inventory after use");
}

/**
 * Tests the resurrection attack effect on dead allies only.
 */
void testResurrectionEffect(const std::shared_ptr<cugl::JsonValue>& itemsJson,
    const std::string& housesJsonPath,
    const std::string& enemiesJsonPath) {
    ItemDatabase db;
    assertWithLabel(db.loadFromJson(itemsJson), "resurrection: item db load succeeds");

    auto resurrectionDef = db.getDef("resurrection");
    assertWithLabel(resurrectionDef != nullptr, "resurrection: resurrection def exists");
    if (!resurrectionDef || resurrectionDef->getEffects().empty()) return;

    const ItemDef::Effect resurrectEffect = resurrectionDef->getEffects()[0];

    HouseLoader loader;
    bool housesOk = loader.loadFromFile(housesJsonPath);
    assertWithLabel(housesOk, "resurrection: house loader init succeeds");

    Enemy enemy;
    bool enemyOk = enemy.init("cyclops", enemiesJsonPath);
    assertWithLabel(enemyOk, "resurrection: enemy init succeeds");
    if (!enemyOk) return;

    Player hades("hades", 0, "Hades Tester", loader);
    Player allyOne("ares", 1, "Dead Ally One", loader);
    Player allyTwo("zeus", 2, "Living Ally", loader);
    Player allyThree("demeter", 3, "Dead Ally Two", loader);

    hades.setLeftPlayer(&allyThree);
    hades.setRightPlayer(&allyOne);
    allyOne.setLeftPlayer(&hades);
    allyOne.setRightPlayer(&allyTwo);
    allyTwo.setLeftPlayer(&allyOne);
    allyTwo.setRightPlayer(&allyThree);
    allyThree.setLeftPlayer(&allyTwo);
    allyThree.setRightPlayer(&hades);

    allyOne.updateHealth(-999999.0f);
    allyThree.updateHealth(-999999.0f);
    allyTwo.updateHealth(-15.0f);
    const float livingHealthBefore = allyTwo.getCurrentHealth();
    const float enemyHealthBefore = enemy.getCurrentHealth();

    auto instResurrection = ItemInstance::alloc("resurrection", 2002);
    assertWithLabel(instResurrection != nullptr, "resurrection: create resurrection instance");
    if (!instResurrection) return;
    hades.addItem(*instResurrection);

    const float resolvedAmount = hades.useItemById(instResurrection->getId(), enemy, db);
    assertWithLabel(floatsEqualWithinTolerance(resolvedAmount, 0.0f), "resurrection: item returns zero base damage");
    assertWithLabel(floatsEqualWithinTolerance(enemy.getCurrentHealth(), enemyHealthBefore), "resurrection: enemy is not damaged");
    assertWithLabel(allyOne.isAlive() && allyThree.isAlive(), "resurrection: dead allies are revived");
    assertWithLabel(floatsEqualWithinTolerance(allyOne.getCurrentHealth(), resurrectEffect.reviveHealth) &&
                    floatsEqualWithinTolerance(allyThree.getCurrentHealth(), resurrectEffect.reviveHealth),
                    "resurrection: revived allies receive configured revive health");
    assertWithLabel(floatsEqualWithinTolerance(allyTwo.getCurrentHealth(), livingHealthBefore),
                    "resurrection: living allies are untouched");
    assertWithLabel(allyOne.hasRegen() && allyThree.hasRegen(), "resurrection: revived allies receive regen");
    assertWithLabel(floatsEqualWithinTolerance(allyOne.getRegenAmountRemaining(), resurrectEffect.regenAmount) &&
                    floatsEqualWithinTolerance(allyThree.getRegenDuration(), resurrectEffect.duration),
                    "resurrection: regen values come from JSON");
    assertWithLabel(hades.getInventory().empty(), "resurrection: item is consumed from inventory after use");
}

/**
 * Tests that educate temporarily allows off-affinity players to apply item effects.
 */
void testEducateEffect(const std::shared_ptr<cugl::JsonValue>& itemsJson,
    const std::string& housesJsonPath,
    const std::string& enemiesJsonPath) {
    ItemDatabase db;
    assertWithLabel(db.loadFromJson(itemsJson), "educate: item db load succeeds");

    auto educateDef = db.getDef("educate");
    assertWithLabel(educateDef != nullptr, "educate: educate def exists");
    if (!educateDef || educateDef->getEffects().empty()) return;

    const ItemDef::Effect educateEffect = educateDef->getEffects()[0];

    HouseLoader loader;
    bool housesOk = loader.loadFromFile(housesJsonPath);
    assertWithLabel(housesOk, "educate: house loader init succeeds");

    Enemy enemy;
    bool enemyOk = enemy.init("cyclops", enemiesJsonPath);
    assertWithLabel(enemyOk, "educate: enemy init succeeds");
    if (!enemyOk) return;

    Player athena("athena", 0, "Athena Tester", loader);
    Player zeus("zeus", 1, "Zeus Ally", loader);
    Player hades("hades", 2, "Hades Ally", loader);
    Player demeter("demeter", 3, "Demeter Ally", loader);

    athena.setLeftPlayer(&demeter);
    athena.setRightPlayer(&zeus);
    zeus.setLeftPlayer(&athena);
    zeus.setRightPlayer(&hades);
    hades.setLeftPlayer(&zeus);
    hades.setRightPlayer(&demeter);
    demeter.setLeftPlayer(&hades);
    demeter.setRightPlayer(&athena);

    auto instEducate = ItemInstance::alloc("educate", 3001);
    auto instLightning = ItemInstance::alloc("lightning_bolt", 3002);
    assertWithLabel(instEducate != nullptr && instLightning != nullptr, "educate: create item instances");
    if (!instEducate || !instLightning) return;

    athena.addItem(*instEducate);
    zeus.addItem(*instLightning);

    const float educateResolved = athena.useItemById(instEducate->getId(), enemy, db);
    assertWithLabel(floatsEqualWithinTolerance(educateResolved, 0.0f), "educate: item returns zero base damage");
    assertWithLabel(athena.hasEducate() && zeus.hasEducate() && hades.hasEducate() && demeter.hasEducate(),
                    "educate: all allies receive educate buff");
    assertWithLabel(floatsEqualWithinTolerance(zeus.getEducateDuration(), educateEffect.duration),
                    "educate: duration comes from JSON");

    const float enemyHealthBefore = enemy.getCurrentHealth();
    const float lightningResolved = zeus.useItemById(instLightning->getId(), enemy, db);
    assertWithLabel(lightningResolved > 0.0f, "educate: off-affinity user still resolves attack magnitude");
    assertWithLabel(enemyHealthBefore > enemy.getCurrentHealth(), "educate: off-affinity lightning still damages enemy");
    assertWithLabel(enemy.isStunned(), "educate: off-affinity user can apply item effects while educated");

    zeus.updateEffects(educateEffect.duration);
    assertWithLabel(!zeus.hasEducate(), "educate: buff expires after configured duration");
}

/**
 * Tests the forge item transformation rules and stable instance identity behavior.
 */
void testForgeEffect(const std::string& housesJsonPath) {
    ItemController controller;
    std::shared_ptr<cugl::AssetManager> assets;
    bool controllerOk = controller.init(assets);
    assertWithLabel(controllerOk, "forge: item controller init succeeds");
    if (!controllerOk) return;

    const ItemDatabase& db = controller.getDatabase();
    auto appleDef = db.getDef("apple");
    auto spearDef = db.getDef("spear");
    auto forgeDef = db.getDef("forge");
    assertWithLabel(appleDef && appleDef->getRarity() == ItemDef::Rarity::Common,
                    "forge: apple fixture is common");
    assertWithLabel(spearDef && spearDef->getRarity() == ItemDef::Rarity::Rare,
                    "forge: spear fixture is rare");
    assertWithLabel(forgeDef && forgeDef->getRarity() == ItemDef::Rarity::Divine,
                    "forge: forge fixture is divine");
    if (!appleDef || !spearDef || !forgeDef) return;

    HouseLoader loader;
    bool housesOk = loader.loadFromFile(housesJsonPath);
    assertWithLabel(housesOk, "forge: house loader init succeeds");
    if (!housesOk) return;

    Player player("zeus", 0, "Forge Tester", loader);
    auto commonItem = ItemInstance::alloc("apple", 4001);
    auto rareItem = ItemInstance::alloc("spear", 4002);
    auto divineItem = ItemInstance::alloc("forge", 4003);
    assertWithLabel(commonItem && rareItem && divineItem, "forge: create item instances");
    if (!commonItem || !rareItem || !divineItem) return;

    player.addItem(*commonItem);
    player.addItem(*rareItem);
    player.addItem(*divineItem);
    const auto idsBefore = collectInventoryItemIds(player);

    const int changed = controller.applyForgeEffect(&player, 1.0f, 12345u);
    const auto& inventory = player.getInventory();
    assertWithLabel(inventory.size() == 3, "forge: inventory size is unchanged");
    assertWithLabel(collectInventoryItemIds(player) == idsBefore,
                    "forge: existing item instance ids are preserved");
    assertWithLabel(changed == 2, "forge: only common and rare items are redefined");

    auto commonAfterDef = db.getDef(inventory[0].getDefId());
    auto rareAfterDef = db.getDef(inventory[1].getDefId());
    assertWithLabel(commonAfterDef && commonAfterDef->getRarity() == ItemDef::Rarity::Rare,
                    "forge: common item becomes rare");
    assertWithLabel(rareAfterDef && rareAfterDef->getRarity() == ItemDef::Rarity::Divine,
                    "forge: rare item becomes divine when chance succeeds");
    assertWithLabel(inventory[2].getDefId() == "forge",
                    "forge: divine items are left unchanged");

    Player chancePlayer("zeus", 1, "Forge Chance Tester", loader);
    auto chanceCommon = ItemInstance::alloc("apple", 4011);
    auto chanceRare = ItemInstance::alloc("spear", 4012);
    assertWithLabel(chanceCommon && chanceRare, "forge: create chance item instances");
    if (!chanceCommon || !chanceRare) return;
    chancePlayer.addItem(*chanceCommon);
    chancePlayer.addItem(*chanceRare);

    const int chanceChanged = controller.applyForgeEffect(&chancePlayer, 0.0f, 67890u);
    const auto& chanceInventory = chancePlayer.getInventory();
    auto chanceCommonAfterDef = db.getDef(chanceInventory[0].getDefId());
    assertWithLabel(chanceChanged == 1, "forge: zero divine chance only redefines common items");
    assertWithLabel(chanceCommonAfterDef && chanceCommonAfterDef->getRarity() == ItemDef::Rarity::Rare,
                    "forge: common item still becomes rare when divine chance is zero");
    assertWithLabel(chanceInventory[1].getDefId() == "spear",
                    "forge: rare item stays rare when divine chance fails");

    Player firstSeedPlayer("zeus", 2, "Forge Seed One", loader);
    Player secondSeedPlayer("zeus", 3, "Forge Seed Two", loader);
    auto firstCommon = ItemInstance::alloc("apple", 4021);
    auto firstRare = ItemInstance::alloc("spear", 4022);
    auto secondCommon = ItemInstance::alloc("apple", 4031);
    auto secondRare = ItemInstance::alloc("spear", 4032);
    assertWithLabel(firstCommon && firstRare && secondCommon && secondRare,
                    "forge: create deterministic seed item instances");
    if (!firstCommon || !firstRare || !secondCommon || !secondRare) return;
    firstSeedPlayer.addItem(*firstCommon);
    firstSeedPlayer.addItem(*firstRare);
    secondSeedPlayer.addItem(*secondCommon);
    secondSeedPlayer.addItem(*secondRare);

    const int firstChanged = controller.applyForgeEffect(&firstSeedPlayer, 1.0f, 24680u);
    const int secondChanged = controller.applyForgeEffect(&secondSeedPlayer, 1.0f, 24680u);
    assertWithLabel(firstChanged == secondChanged,
                    "forge: same seed changes the same number of items");
    assertWithLabel(collectInventoryDefIds(firstSeedPlayer) == collectInventoryDefIds(secondSeedPlayer),
                    "forge: same seed produces the same replacement definitions");
}

} // namespace

void ItemTests::runAll(const std::string& itemsJsonPath,
                       const std::string& housesJsonPath,
                       const std::string& enemiesJsonPath) {
    _passed = 0;
    _failed = 0;
    CULog("=========================================");
    CULog("  ItemTests::runAll");
    CULog("=========================================");
    
    auto itemsJson = readJson(itemsJsonPath);
    auto housesJson = readJson(housesJsonPath);
    
    if (!itemsJson || !housesJson) {
        assertWithLabel(false, "fixtures: item/house JSON must parse");
        printSummary();
        return;
    }
    
    testItemsLoad(itemsJson);
    testHouseMultipliersLoad(housesJson);
    testEnumParsers();
    testWeightedRollAndInstanceCreation(itemsJson);
    testValidationFailures();
    testScalingFallbacks();
    testBaseValueDefaults();
    testEffectiveValueComputation(itemsJson, housesJson, housesJsonPath, enemiesJsonPath);
    testShieldEffect(itemsJson, housesJson, housesJsonPath, enemiesJsonPath);
    testBarrierEffect(itemsJson, housesJson, housesJsonPath, enemiesJsonPath);
    testRegenEffect(itemsJson, housesJson, housesJsonPath, enemiesJsonPath);
    testHelmEffect(itemsJson, housesJson, housesJsonPath, enemiesJsonPath);
    testShieldBarrierCoexistence(itemsJson, housesJson, housesJsonPath, enemiesJsonPath);
    testStunEffect(itemsJson, housesJson, housesJsonPath, enemiesJsonPath);
    testLoveEffect(itemsJson, housesJson, housesJsonPath, enemiesJsonPath);
    testSlowEffect(itemsJson, housesJson, housesJsonPath, enemiesJsonPath);
    testVulnerableEffect(itemsJson, housesJson, housesJsonPath, enemiesJsonPath);
    testTridentVulnerableAllSides(itemsJson, housesJson, housesJsonPath, enemiesJsonPath);
    testGaiaRockHealsEnemy(itemsJson, housesJsonPath, enemiesJsonPath);
    testResurrectionEffect(itemsJson, housesJsonPath, enemiesJsonPath);
    testEducateEffect(itemsJson, housesJsonPath, enemiesJsonPath);
    testForgeEffect(housesJsonPath);
    
    printSummary();
}
