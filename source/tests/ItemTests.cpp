// ItemTests.cpp
// Unit tests for item JSON parsing and house-scaling loading.

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

int _passed = 0;
int _failed = 0;

void expect(bool condition, const std::string& label) {
    if (condition) {
        CULog("[PASS] %s", label.c_str());
        _passed++;
    } else {
        CULog("[FAIL] %s", label.c_str());
        _failed++;
    }
}

void printSummary() {
    CULog("-----------------------------------------");
    CULog("  %d passed   %d failed", _passed, _failed);
    CULog("-----------------------------------------");
}

bool nearlyEqual(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

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

void testItemsLoad(const std::shared_ptr<cugl::JsonValue>& itemsJson) {
    ItemDatabase db;
    bool ok = db.loadFromJson(itemsJson);
    expect(ok, "items: loadFromJson succeeds");

    auto allIds = db.getAllDefIds();
    expect(!allIds.empty(), "items: at least one item definition exists");

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
    expect(allValid, "items: all defs have positive baseValue");

    // Ensure migration fields are present for all loaded items.
    bool allHaveExpectedSchema = true;
    for (const std::string& id : allIds) {
        auto def = db.getDef(id);
        if (!def) {
            allHaveExpectedSchema = false;
            break;
        }
        if (def->getEffect().empty()) {
            allHaveExpectedSchema = false;
            break;
        }
    }
    expect(allHaveExpectedSchema, "items: loaded defs expose effect/base/affinity data");
}

void testHouseScalingLoad(const std::shared_ptr<cugl::JsonValue>& housesJson) {
    ItemDatabase db;
    bool ok = db.loadHouseScalingFromJson(housesJson);
    expect(ok, "houses: loadHouseScalingFromJson succeeds");

    const ItemDatabase::HouseScaling* zeus = db.getHouseScaling("Zeus");
    const ItemDatabase::HouseScaling* poseidon = db.getHouseScaling("Poseidon");
    const ItemDatabase::HouseScaling* athena = db.getHouseScaling("Athena");

    expect(zeus != nullptr, "houses: Zeus scaling exists");
    expect(poseidon != nullptr, "houses: Poseidon scaling exists");
    expect(athena != nullptr, "houses: Athena scaling exists");

    bool bounded = true;
    const ItemDatabase::HouseScaling* checks[] = { zeus, poseidon, athena };
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
    expect(bounded, "houses: scaling values are bounded and affinityBonus is positive");
}

void testEnumParsers() {
        expect(ItemDef::typeFromString("attack") == ItemDef::Type::Attack, "parse: type attack");
        expect(ItemDef::typeFromString("support") == ItemDef::Type::Support, "parse: type support");
        expect(ItemDef::typeFromString("utility") == ItemDef::Type::Utility, "parse: type utility");

        expect(ItemDef::rarityFromString("common") == ItemDef::Rarity::Common, "parse: rarity common");
        expect(ItemDef::rarityFromString("rare") == ItemDef::Rarity::Rare, "parse: rarity rare");
        expect(ItemDef::rarityFromString("divine") == ItemDef::Rarity::Divine, "parse: rarity divine");

        expect(ItemDef::houseFromString("Zeus") == ItemDef::House::Zeus, "parse: house Zeus");
        expect(ItemDef::houseFromString("Ares") == ItemDef::House::Ares, "parse: house Ares");
        expect(ItemDef::houseFromString("none") == ItemDef::House::None, "parse: house none");
}

void testWeightedRollAndInstanceCreation(const std::shared_ptr<cugl::JsonValue>& itemsJson) {
        ItemDatabase db;
        expect(db.loadFromJson(itemsJson), "db: loads for roll/instance tests");
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
        expect(rolledAtLeastOne, "db: weighted roll returns at least one item id");
        expect(allRolledKnown, "db: weighted roll only returns known item ids");

        auto okInstance = db.createInstance("apple", 42);
        auto badInstance = db.createInstance("does_not_exist", 999);
        expect(okInstance != nullptr, "db: createInstance works for known defId");
        expect(badInstance == nullptr, "db: createInstance fails for unknown defId");
}

void testStrictMigrationAndValidationFailures() {
        ItemDatabase db;

        auto j1 = readJson("json/tests/items_legacy_keys.json");
        expect(j1 != nullptr, "strict: parse legacy-keys fixture json");
        if (j1) {
                expect(!db.loadFromJson(j1), "strict: deprecated item keys are rejected");
        }

        auto j2 = readJson("json/tests/items_bad_rarity.json");
        expect(j2 != nullptr, "strict: parse bad-rarity fixture json");
        if (j2) {
                expect(!db.loadFromJson(j2), "strict: unsupported rarity is rejected");
        }

        auto j3 = readJson("json/tests/items_bad_type.json");
        expect(j3 != nullptr, "strict: parse bad-type fixture json");
        if (j3) {
                expect(!db.loadFromJson(j3), "strict: unsupported item type is rejected");
        }

        auto j4 = readJson("json/tests/items_legacy_rarity_weights.json");
        expect(j4 != nullptr, "strict: parse legacy-rarity-weights fixture json");
        if (j4) {
                expect(!db.loadFromJson(j4), "strict: legacy rarity weight keys are rejected");
        }
}

void testScalingFallbacks() {
        ItemDatabase db;
        auto json = readJson("json/tests/houses_scaling_fallbacks.json");
        expect(json != nullptr, "fallback: parse clamp/fallback house fixture");
        if (!json) return;

        expect(db.loadHouseScalingFromJson(json), "fallback: house scaling loads with fallback defaults");

        const auto* clamp = db.getHouseScaling("ClampHouse");
        const auto* missing = db.getHouseScaling("MissingHouse");
        expect(clamp != nullptr, "fallback: ClampHouse scaling exists");
        expect(missing != nullptr, "fallback: MissingHouse scaling exists");
        if (clamp) {
                expect(nearlyEqual(clamp->attack, 1.0f), "fallback: attack clamped to 1.0");
                expect(nearlyEqual(clamp->support, 0.0f), "fallback: support clamped to 0.0");
                expect(nearlyEqual(clamp->utility, 0.5f), "fallback: utility unchanged when valid");
                expect(nearlyEqual(clamp->affinityBonus, 1.5f), "fallback: non-positive affinityBonus defaults to 1.5");
        }
        if (missing) {
                expect(nearlyEqual(missing->attack, 0.0f), "fallback: missing attack defaults to 0.0");
                expect(nearlyEqual(missing->support, 0.0f), "fallback: missing support defaults to 0.0");
                expect(nearlyEqual(missing->utility, 0.0f), "fallback: missing utility defaults to 0.0");
                expect(nearlyEqual(missing->affinityBonus, 1.5f), "fallback: missing affinityBonus defaults to 1.5");
        }
}

void testBaseValueDefaults() {
        ItemDatabase db;
        auto json = readJson("json/tests/items_basevalue_fallbacks.json");
        expect(json != nullptr, "baseValue: parse fallback fixture");
        if (!json) return;

        expect(db.loadFromJson(json), "baseValue: fixture loads with defaults");
        auto neg = db.getDef("neg_base");
        auto miss = db.getDef("missing_base");
        expect(neg != nullptr && miss != nullptr, "baseValue: fallback defs exist");
        if (neg) {
                expect(nearlyEqual(neg->getBaseValue(), 1.0f), "baseValue: negative baseValue defaults to 1.0");
        }
        if (miss) {
                expect(nearlyEqual(miss->getBaseValue(), 1.0f), "baseValue: missing baseValue defaults to 1.0");
        }
}

void testEffectiveValueComputation(const std::shared_ptr<cugl::JsonValue>& itemsJson,
                                                                     const std::shared_ptr<cugl::JsonValue>& housesJson,
                                                                     const std::string& housesJsonPath,
                                                                     const std::string& enemiesJsonPath) {
        ItemDatabase db;
        expect(db.loadFromJson(itemsJson), "compute: item db load succeeds");
        expect(db.loadHouseScalingFromJson(housesJson), "compute: house scaling load succeeds");

        HouseLoader loader;
        bool housesOk = loader.loadFromFile(housesJsonPath);
        expect(housesOk, "compute: house loader init succeeds");

        Enemy enemy;
        bool enemyOk = enemy.init("enemy1", enemiesJsonPath);
        expect(enemyOk, "compute: enemy init succeeds");

        if (!housesOk || !enemyOk) return;

        // Attack rare item with matching affinity (Ares + noams_ballista)
        Player ares("Ares", 1, "Ares Tester", loader);
        auto instAres = ItemInstance::alloc("noams_ballista", 1001);
        expect(instAres != nullptr, "compute: create noams_ballista instance (Ares)");
        if (!instAres) return;
        ares.addItem(*instAres);

        enemy.setCurrentHealth(enemy.getMaxHealth());
        float hpBeforeAres = enemy.getCurrentHealth();
        float resolvedAres = ares.useItemById(instAres->getId(), enemy, db);
        float expectedAres = 1.5f * (1.0f + 1.0f) * 1.5f; // base * (1 + attack slider) * affinity
        expect(nearlyEqual(resolvedAres, expectedAres), "compute: matching rare affinity resolves correctly");
        expect(nearlyEqual(hpBeforeAres - enemy.getCurrentHealth(), expectedAres), "compute: enemy damage equals resolved attack value");

        // Attack rare item without matching affinity (Poseidon + noams_ballista)
        Player poseidon("Poseidon", 2, "Poseidon Tester", loader);
        auto instPoseidon = ItemInstance::alloc("noams_ballista", 1002);
        expect(instPoseidon != nullptr, "compute: create noams_ballista instance (Poseidon)");
        if (!instPoseidon) return;
        poseidon.addItem(*instPoseidon);

        enemy.setCurrentHealth(enemy.getMaxHealth());
        float hpBeforePoseidon = enemy.getCurrentHealth();
        float resolvedPoseidon = poseidon.useItemById(instPoseidon->getId(), enemy, db);
        float expectedPoseidon = 1.5f * (1.0f + 0.8f); // no affinity bonus
        expect(nearlyEqual(resolvedPoseidon, expectedPoseidon), "compute: non-matching rare affinity resolves correctly");
        expect(nearlyEqual(hpBeforePoseidon - enemy.getCurrentHealth(), expectedPoseidon), "compute: enemy damage without affinity is correct");

        // Support common item (Demeter + apple) should not use affinity
        Player demeter("Demeter", 3, "Demeter Tester", loader);
        Player ally("Ares", 4, "Ally", loader);
        ally.updateHealth(-4.0f);

        auto instApple = ItemInstance::alloc("apple", 1003);
        expect(instApple != nullptr, "compute: create apple instance");
        if (!instApple) return;
        demeter.addItem(*instApple);

        float allyBefore = ally.getCurrentHealth();
        float resolvedSupport = demeter.useItemById(instApple->getId(), ally, db);
        float expectedSupport = 2.0f * (1.0f + 0.9f);
        expect(nearlyEqual(resolvedSupport, expectedSupport), "compute: support scaling resolves correctly");
        expect(nearlyEqual(ally.getCurrentHealth() - allyBefore, expectedSupport), "compute: support heal equals resolved value");

        // Mismatched target type should return 0 and still consume item
        Player testAttacker("Ares", 5, "Ares Tester 2", loader);
        Player testTarget("Zeus", 6, "Zeus Target", loader);
        auto instAttack = ItemInstance::alloc("noams_ballista", 1004);
        expect(instAttack != nullptr, "compute: create mismatch attack instance");
        if (!instAttack) return;
        testAttacker.addItem(*instAttack);
        float mismatch = testAttacker.useItemById(instAttack->getId(), testTarget, db);
        expect(nearlyEqual(mismatch, 0.0f), "compute: attack on player returns 0.0");
        expect(testAttacker.getInventory().empty(), "compute: mismatch target still consumes item");

        // Missing item id should fail with -1
        float missing = testAttacker.useItemById(999999, testTarget, db);
        expect(nearlyEqual(missing, -1.0f), "compute: missing item id returns -1.0");
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
        expect(false, "fixtures: item/house JSON must parse");
        printSummary();
        return;
    }

    testItemsLoad(itemsJson);
    testHouseScalingLoad(housesJson);
    testEnumParsers();
    testWeightedRollAndInstanceCreation(itemsJson);
    testStrictMigrationAndValidationFailures();
    testScalingFallbacks();
    testBaseValueDefaults();
    testEffectiveValueComputation(itemsJson, housesJson, housesJsonPath, enemiesJsonPath);

    printSummary();
}
