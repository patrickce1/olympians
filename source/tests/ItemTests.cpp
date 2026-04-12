// ItemTests.cpp
// Unit tests for item JSON parsing and house multipliers loading.

#include "ItemTests.h"
#include "../items/ItemDatabase.h"
#include "../items/ItemInstance.h"
#include "../HouseLoader.h"
#include "../Player.h"
#include "../Enemy.h"
#include <cugl/cugl.h>
#include <cmath>
#include <set>

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
        if (def->getBaseValue() <= 0.0f) {
            allValid = false;
            break;
        }
    }
    assertWithLabel(allValid, "items: all defs have positive baseValue");
    
    auto lightningBoltDef = db.getDef("lightning_bolt");
    auto appleDef = db.getDef("apple");
    assertWithLabel(lightningBoltDef && lightningBoltDef->getHouseAffinity() == ItemDef::House::Zeus,
           "items: lightning_bolt affinity parses as Zeus");
    assertWithLabel(appleDef && appleDef->getHouseAffinity() == ItemDef::House::None,
           "items: apple affinity parses as none");
}

/**
 * Tests house multiplier JSON loading and bounds validation.
 *
 * Verifies that:
 * - ItemDatabase loads house multipliers successfully from parsed JSON
 * - All six houses (Zeus, Poseidon, Athena, Ares, Hephaestus, Demeter) have loaded multipliers
 * - All multiplier values (attack, support, utility) are bounded in [0.0, 1.0]
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
            entry->utility < 0.0f || entry->utility > 1.0f ||
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
 * - Type parsing: "attack", "support", "utility"
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
 * - Invalid type values (not in {attack, support, utility})
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
 * - Clamps attack/support/utility values to [0.0, 1.0]
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
        assertWithLabel(floatsEqualWithinTolerance(clampHouseMultipliers->utility, 0.5f), "fallback: utility unchanged when valid");
        assertWithLabel(floatsEqualWithinTolerance(clampHouseMultipliers->affinityBonus, 1.5f), "fallback: non-positive affinityBonus defaults to 1.5");
    }
    if (missingHouseMultipliers) {
        assertWithLabel(floatsEqualWithinTolerance(missingHouseMultipliers->attack, 0.0f), "fallback: missing attack defaults to 0.0");
        assertWithLabel(floatsEqualWithinTolerance(missingHouseMultipliers->support, 0.0f), "fallback: missing support defaults to 0.0");
        assertWithLabel(floatsEqualWithinTolerance(missingHouseMultipliers->utility, 0.0f), "fallback: missing utility defaults to 0.0");
        assertWithLabel(floatsEqualWithinTolerance(missingHouseMultipliers->affinityBonus, 1.5f), "fallback: missing affinityBonus defaults to 1.5");
    }
}

/**
 * Tests baseValue fallback behavior for missing or invalid item values.
 *
 * Verifies that ItemDatabase correctly:
 * - Defaults negative baseValue to 1.0
 * - Defaults missing baseValue to 1.0
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
        assertWithLabel(floatsEqualWithinTolerance(negativeBaseValueDef->getBaseValue(), 1.0f), "baseValue: negative baseValue defaults to 1.0");
    }
    if (missingBaseValueDef) {
        assertWithLabel(floatsEqualWithinTolerance(missingBaseValueDef->getBaseValue(), 1.0f), "baseValue: missing baseValue defaults to 1.0");
    }
}

/**
 * Tests complete item usage computation pipeline with different scenarios.
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

        HouseLoader loader;
        bool housesOk = loader.loadFromFile(housesJsonPath);
        assertWithLabel(housesOk, "compute: house loader init succeeds");

        Enemy enemy;
        bool enemyOk = enemy.init("enemy1", enemiesJsonPath);
        assertWithLabel(enemyOk, "compute: enemy init succeeds");
    Player ares("Ares", 2, "Ares Tester", loader);
    auto instAres = ItemInstance::alloc("noams_ballista", 1001);
    assertWithLabel(instAres != nullptr, "compute: create noams_ballista instance (Ares)");
    if (!instAres) return;
    ares.addItem(*instAres);
    
    enemy.setCurrentHealth(enemy.getMaxHealth());
    float hpBeforeAres = enemy.getCurrentHealth();
    float resolvedAres = ares.useItemById(instAres->getId(), enemy, db);
    float expectedAres = 1.5f * (1.0f + 1.0f) * 1.5f; // base * (1 + attack slider) * affinity
        assertWithLabel(floatsEqualWithinTolerance(resolvedAres, expectedAres), "compute: matching rare affinity resolves correctly");
        assertWithLabel(floatsEqualWithinTolerance(hpBeforeAres - enemy.getCurrentHealth(), expectedAres), "compute: enemy damage equals resolved attack value");
    // Attack rare item without matching affinity (Poseidon + noams_ballista)
    Player poseidon("Poseidon", 2, "Poseidon Tester", loader);
    auto instPoseidon = ItemInstance::alloc("noams_ballista", 1002);
    assertWithLabel(instPoseidon != nullptr, "compute: create noams_ballista instance (Poseidon)");
    if (!instPoseidon) return;
    poseidon.addItem(*instPoseidon);
    
    enemy.setCurrentHealth(enemy.getMaxHealth());
    float hpBeforePoseidon = enemy.getCurrentHealth();
    float resolvedPoseidon = poseidon.useItemById(instPoseidon->getId(), enemy, db);
    float expectedPoseidon = 1.5f * (1.0f + 0.8f); // no affinity bonus
        assertWithLabel(floatsEqualWithinTolerance(resolvedPoseidon, expectedPoseidon), "compute: non-matching rare affinity resolves correctly");
        assertWithLabel(floatsEqualWithinTolerance(hpBeforePoseidon - enemy.getCurrentHealth(), expectedPoseidon), "compute: enemy damage without affinity is correct");
    // Support common item (Demeter + apple) should not use affinity
    Player demeter("Demeter", 3, "Demeter Tester", loader);
    Player ally("Ares", 4, "Ally", loader);
    ally.updateHealth(-4.0f);
    
    auto instApple = ItemInstance::alloc("apple", 1003);
    assertWithLabel(instApple != nullptr, "compute: create apple instance");
    if (!instApple) return;
    demeter.addItem(*instApple);
    
    float allyBefore = ally.getCurrentHealth();
    float resolvedSupport = demeter.useItemById(instApple->getId(), ally, db);
    float expectedSupport = 2.0f * (1.0f + 0.9f);
        assertWithLabel(floatsEqualWithinTolerance(resolvedSupport, expectedSupport), "compute: support scaling resolves correctly");
        assertWithLabel(floatsEqualWithinTolerance(ally.getCurrentHealth() - allyBefore, expectedSupport), "compute: support heal equals resolved value");
    // Mismatched target type should return 0 and still consume item
    Player testAttacker("Ares", 5, "Ares Tester 2", loader);
    Player testTarget("Zeus", 6, "Zeus Target", loader);
    auto instAttack = ItemInstance::alloc("noams_ballista", 1004);
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
    
    printSummary();
}
