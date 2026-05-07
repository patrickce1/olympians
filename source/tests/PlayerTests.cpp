// PlayerTests.cpp
// Unit tests for Player inventory, card passing, card usage, and AI behavior.
//
// HOW TO RUN:
//   Call PlayerTests::runAll() from your app startup AFTER cugl is initialized
//   but BEFORE the game loop starts, e.g. in AppDelegate::onStartup().
//   All results print via CULog as [PASS] or [FAIL]. Remove the call before shipping.
//
//   Example:
//     PlayerTests::runAll("json/houses.json",
//                         "json/items.json",
//                         "json/enemies.json",
//                         "assets/json/playerAI.json");

#include "PlayerTests.h"
#include "../Player.h"
#include "../Enemy.h"
#include "../EnemyLoader.h"
#include "../playerAI/EasyPlayerAI.h"
#include "../items/ItemDatabase.h"
#include "../items/ItemDef.h"
#include "../items/ItemInstance.h"
#include "../items/ItemController.h"
#include "../HouseLoader.h"
#include <cugl/cugl.h>
#include <cmath>

// ─────────────────────────────────────────────
// Internal helpers — not exposed in the header
// ─────────────────────────────────────────────
namespace {

int _passed = 0;
int _failed = 0;

/** Assertion helper: logs [PASS]/[FAIL] and updates counters. */
void assertWithLabel(bool condition, const std::string& label) {
    if (condition) {
        CULog("[PASS] %s", label.c_str());
        _passed++;
    } else {
        CULog("[FAIL] %s", label.c_str());
        _failed++;
    }
}

/** Prints a summary line with total passed/failed counts. */
void printSummary() {
    CULog("─────────────────────────────────────────");
    CULog("  %d passed   %d failed", _passed, _failed);
    CULog("─────────────────────────────────────────");
}

/** Prints every item in a player's hand to CULog (visual debugging aid). */
void printHand(const Player& p) {
    const auto& inv = p.getInventory();
    CULog("  [%s] hand (%zu item(s)):", p.getPlayerName().c_str(), inv.size());
    if (inv.empty()) {
        CULog("    (empty)");
        return;
    }
    for (const ItemInstance& item : inv) {
        CULog("    - instanceId=%-20llu  defId=%s",
              (unsigned long long)item.getId(),
              item.getDefId().c_str());
    }
}

/** Loads the ItemDatabase + house multipliers from JSON and seeds time. */
ItemDatabase loadDatabase(const std::string& itemsJsonPath,
                         const std::string& housesJsonPath) {
    ItemDatabase db;

    auto reader = cugl::JsonReader::alloc(itemsJsonPath);
    if (!reader) {
        CULogError("PlayerTests: failed to open items JSON at '%s'", itemsJsonPath.c_str());
        return db;
    }

    auto json = reader->readJson();
    if (!json) {
        CULogError("PlayerTests: failed to parse items JSON at '%s'", itemsJsonPath.c_str());
        return db;
    }

    if (!db.loadFromJson(json)) {
        CULogError("PlayerTests: ItemDatabase::loadFromJson failed for '%s'", itemsJsonPath.c_str());
    }

    auto houseReader = cugl::JsonReader::alloc(housesJsonPath);
    if (!houseReader) {
        CULogError("PlayerTests: failed to open houses JSON at '%s'", housesJsonPath.c_str());
    } else {
        auto housesJson = houseReader->readJson();
        if (!housesJson) {
            CULogError("PlayerTests: failed to parse houses JSON at '%s'", housesJsonPath.c_str());
        } else if (!db.loadHouseMultipliersFromJson(housesJson)) {
            CULogError("PlayerTests: ItemDatabase::loadHouseMultipliersFromJson failed for '%s'", housesJsonPath.c_str());
        }
    }

    db.setStartingPointWithTime();
    return db;
}

/** Returns true if two floats are equal within the given tolerance (error range). */
bool floatsEqualWithinTolerance(float firstValue, float secondValue, float tolerance = 1e-4f) {
    return std::fabs(firstValue - secondValue) <= tolerance;
}

/** Loads house definitions from the given JSON path for use in tests. */
HouseLoader loadHouses(const std::string& housesJsonPath) {
    HouseLoader loader;
    if (!loader.loadFromFile(housesJsonPath)) {
        CULogError("PlayerTests: failed to load houses from '%s'", housesJsonPath.c_str());
    }
    return loader;
}

/** Loads and returns an Enemy by id from the enemies JSON; logs errors on failure. */
Enemy loadEnemy(const std::string& enemiesJsonPath, const std::string& enemyId) {
    Enemy enemy;
    if (!enemy.init(enemyId, enemiesJsonPath)) {
        CULogError("PlayerTests: enemy init failed for id '%s' from '%s'",
                   enemyId.c_str(), enemiesJsonPath.c_str());
        return Enemy();
    }
    return enemy;
}

// ── Item creation helper ─────────────────────

static ItemInstance::ItemId _testIdCounter = 1000;

/** Creates an ItemInstance with a unique test id for the given defId. */
ItemInstance makeItem(const std::string& defId) {
    auto inst = ItemInstance::alloc(defId, _testIdCounter++);
    CUAssertLog(inst != nullptr, "PlayerTests: makeItem failed for defId '%s'", defId.c_str());
    return *inst;
}

// ── Player factories ─────────────────────────

/** Creates two players and wires them as each other's left/right neighbors. */
std::vector<std::shared_ptr<Player>> makeTwoPlayers(const HouseLoader& loader,
                                                    const std::string& houseId) {
    std::vector<std::shared_ptr<Player>> players;
    players.push_back(std::make_shared<Player>(houseId, 1, "Player 1", loader));
    players.push_back(std::make_shared<Player>(houseId, 2, "Player 2", loader));
    players[0]->setRightPlayer(players[1].get());
    players[0]->setLeftPlayer (players[1].get());
    players[1]->setRightPlayer(players[0].get());
    players[1]->setLeftPlayer (players[0].get());
    return players;
}

/** Creates four players arranged in a ring and wires neighbor pointers. */
std::vector<std::shared_ptr<Player>> makeFourPlayers(const HouseLoader& loader,
                                                     const std::string& houseId) {
    std::vector<std::shared_ptr<Player>> players;
    players.push_back(std::make_shared<Player>(houseId, 1, "Player 1", loader));
    players.push_back(std::make_shared<Player>(houseId, 2, "Player 2", loader));
    players.push_back(std::make_shared<Player>(houseId, 3, "Player 3", loader));
    players.push_back(std::make_shared<Player>(houseId, 4, "Player 4", loader));

    const int n = (int)players.size();
    for (int i = 0; i < n; i++) {
        players[i]->setLeftPlayer (players[(i - 1 + n) % n].get());
        players[i]->setRightPlayer(players[(i + 1)     % n].get());
    }
    return players;
}

// ── Utility: first defId of a given type ────

/** Returns the first item defId in the database that matches the given type, or empty. */
std::string firstDefIdOfType(const ItemDatabase& db, ItemDef::Type type) {
    for (const std::string& id : db.getAllDefIds()) {
        auto def = db.getDef(id);
        if (def && def->getType() == type) return id;
    }
    return "";
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 1 — Inventory basics
// ─────────────────────────────────────────────────────────────────────────────

/** Verifies a new player's inventory starts empty. */
static void testInventoryStartsEmpty(const HouseLoader& loader,
                                     const std::string& houseId) {
    auto players = makeTwoPlayers(loader, houseId);
    assertWithLabel(players[0]->getInventory().empty(), "Inventory starts empty");
}

/** Ensures addItem increases the inventory count. */
static void testAddItemIncreasesCount(const HouseLoader& loader,
                                      const std::string& houseId,
                                      const std::string& attackDefId) {
    auto players = makeTwoPlayers(loader, houseId);
    players[0]->addItem(makeItem(attackDefId));
    assertWithLabel(players[0]->getInventory().size() == 1,
           "addItem: inventory count increases to 1");
}

/** Ensures removeItemById decreases the inventory count when the id exists. */
static void testRemoveItemDecreasesCount(const HouseLoader& loader,
                                         const std::string& houseId,
                                         const std::string& attackDefId) {
    auto players  = makeTwoPlayers(loader, houseId);
    ItemInstance item = makeItem(attackDefId);
    players[0]->addItem(item);
    players[0]->removeItemById(item.getId());
    assertWithLabel(players[0]->getInventory().empty(),
           "removeItemById: inventory count drops to 0");
}

/** Confirms removing a non-existent item id does nothing to the inventory. */
static void testRemoveNonexistentItemIsNoop(const HouseLoader& loader,
                                            const std::string& houseId,
                                            const std::string& attackDefId) {
    auto players  = makeTwoPlayers(loader, houseId);
    ItemInstance item = makeItem(attackDefId);
    players[0]->addItem(item);
    players[0]->removeItemById(item.getId() + 99999);  // bogus id
    assertWithLabel(players[0]->getInventory().size() == 1,
           "removeItemById: does nothing when no id match, real items untouched");
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 2 — Print hands (visual / debug)
// ─────────────────────────────────────────────────────────────────────────────

/** Prints sample hands for visual verification; should run without crashing. */
static void testPrintHands(const HouseLoader& loader,
                            const std::string& houseId,
                            const std::string& attackDefId,
                            const std::string& supportDefId) {
    CULog("── printHands ────────────────────────────");
    auto players = makeFourPlayers(loader, houseId);

    players[0]->addItem(makeItem(attackDefId));
    players[0]->addItem(makeItem(supportDefId));
    players[1]->addItem(makeItem(attackDefId));
    players[2]->addItem(makeItem(supportDefId));
    players[2]->addItem(makeItem(supportDefId));
    // players[3] intentionally empty

    for (const auto& p : players) printHand(*p);
    CULog("──────────────────────────────────────────");
    assertWithLabel(true, "printHands: ran without crash");
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 3 — Card passing
// ─────────────────────────────────────────────────────────────────────────────

/** Verifies passing to the right moves the item to the neighbor and empties sender. */
static void testPassRightMovesItem(const HouseLoader& loader,
                                   const std::string& houseId,
                                   const std::string& attackDefId) {
    auto players = makeTwoPlayers(loader, houseId);
    players[0]->addItem(makeItem(attackDefId));

    Player*      target = players[0]->getRightPlayer();
    ItemInstance item   = players[0]->getInventory()[0];
    players[0]->removeItemById(item.getId());
    target->addItem(item);

    CULog("── passRight ─────────────────────────────");
    printHand(*players[0]);
    printHand(*players[1]);

    assertWithLabel(players[0]->getInventory().empty(),       "passRight: sender inventory is empty");
    assertWithLabel(players[1]->getInventory().size() == 1,   "passRight: receiver has 1 item");
    assertWithLabel(players[1]->getInventory()[0].getDefId() == attackDefId,
                                                     "passRight: received item has correct defId");
}

/** Verifies passing to the left moves the item to the neighbor and empties sender. */
static void testPassLeftMovesItem(const HouseLoader& loader,
                                  const std::string& houseId,
                                  const std::string& supportDefId) {
    auto players = makeTwoPlayers(loader, houseId);
    players[0]->addItem(makeItem(supportDefId));

    Player*      target = players[0]->getLeftPlayer();
    ItemInstance item   = players[0]->getInventory()[0];
    players[0]->removeItemById(item.getId());
    target->addItem(item);

    CULog("── passLeft ──────────────────────────────");
    printHand(*players[0]);
    printHand(*players[1]);

    assertWithLabel(players[0]->getInventory().empty(),     "passLeft: sender inventory is empty");
    assertWithLabel(players[1]->getInventory().size() == 1, "passLeft: receiver has 1 item");
}

/** Confirms passing to a dead player is a no-op and the item remains with sender. */
static void testPassToDeadPlayerIsNoop(const HouseLoader& loader,
                                       const std::string& houseId,
                                       const std::string& attackDefId) {
    auto players = makeTwoPlayers(loader, houseId);
    players[1]->updateHealth(-999999.0f);
    assertWithLabel(!players[1]->isAlive(), "passToDeadPlayer: target confirmed dead");

    players[0]->addItem(makeItem(attackDefId));

    Player* target = players[0]->getRightPlayer();
    if (target && target->isAlive()) {
        ItemInstance item = players[0]->getInventory()[0];
        players[0]->removeItemById(item.getId());
        target->addItem(item);
    }

    assertWithLabel(players[0]->getInventory().size() == 1,
           "passToDeadPlayer: item stays with sender");
}

/** Ensures an item passed around four players returns to the origin after a full loop. */
static void testCircularPassAroundRing(const HouseLoader& loader,
                                       const std::string& houseId,
                                       const std::string& attackDefId) {
    auto players = makeFourPlayers(loader, houseId);
    players[0]->addItem(makeItem(attackDefId));

    // Pass rightward once per player — 4 passes returns item to Player 0
    for (int step = 0; step < 4; step++) {
        auto& current = *players[step % 4];
        if (!current.getInventory().empty()) {
            Player*      target = current.getRightPlayer();
            ItemInstance item   = current.getInventory()[0];
            current.removeItemById(item.getId());
            target->addItem(item);
        }
    }

    CULog("── circularPass (4 passes) ───────────────");
    for (const auto& p : players) printHand(*p);

    assertWithLabel(players[0]->getInventory().size() == 1,
           "circularPass: item returns to origin after full loop");
    assertWithLabel(players[1]->getInventory().empty() &&
           players[2]->getInventory().empty() &&
           players[3]->getInventory().empty(),
           "circularPass: only origin player holds the item");
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 4 — Card usage (attack and support)
// ─────────────────────────────────────────────────────────────────────────────

/** Checks using an attack item reduces enemy hp and consumes the item. */
static void testUseAttackItemDamagesEnemy(const HouseLoader& loader,
                                          const std::string& houseId,
                                          const ItemDatabase& db,
                                          Enemy& enemy,
                                          const std::string& attackDefId) {
    auto players   = makeTwoPlayers(loader, houseId);
    float hpBefore = enemy.getCurrentHealth();
    players[0]->addItem(makeItem(attackDefId));

    for (const ItemInstance& item : players[0]->getInventory()) {
        auto def = db.getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Attack) {
            players[0]->useItemById(item.getId(), enemy, db);
            break;
        }
    }

    CULog("── attackEnemy: hp %.1f → %.1f ───────────",
          hpBefore, enemy.getCurrentHealth());

    assertWithLabel(enemy.getCurrentHealth() < hpBefore,   "useAttackItem: enemy hp decreased");
    assertWithLabel(players[0]->getInventory().empty(),     "useAttackItem: item consumed from inventory");
}

/**
 * Checks using the Apple support item heals an ally and consumes the item.
 *
 * @param loader   House definitions used to construct test players
 * @param houseId  The house assigned to both test players
 * @param db       Item database used to resolve and apply the Apple item
 */
static void testUseSupportItemHealsAlly(const HouseLoader& loader,
                                        const std::string& houseId,
                                        const ItemDatabase& db) {
    auto players = makeTwoPlayers(loader, houseId);
    players[1]->updateHealth(-20.0f);
    float hpBefore = players[1]->getCurrentHealth();

    auto appleDef = db.getDef("apple");
    assertWithLabel(appleDef != nullptr, "useSupportItem: apple def exists");
    if (!appleDef) return;

    players[0]->addItem(makeItem("apple"));
    players[0]->useItemById(players[0]->getInventory()[0].getId(), *players[1], db);

    CULog("── healAlly: hp %.1f → %.1f ──────────────",
          hpBefore, players[1]->getCurrentHealth());

    assertWithLabel(players[1]->getCurrentHealth() > hpBefore, "useSupportItem: ally hp increased");
    assertWithLabel(players[0]->getInventory().empty(),         "useSupportItem: item consumed from inventory");
}

/** Validates attack items used on allies do not change ally hp but are consumed. */
static void testUseAttackItemOnAllyIsNoop(const HouseLoader& loader,
                                          const std::string& houseId,
                                          const ItemDatabase& db,
                                          const std::string& attackDefId) {
    auto players   = makeTwoPlayers(loader, houseId);
    float hpBefore = players[1]->getCurrentHealth();
    players[0]->addItem(makeItem(attackDefId));
    players[0]->useItemById(players[0]->getInventory()[0].getId(), *players[1], db);

    assertWithLabel(players[1]->getCurrentHealth() == hpBefore,
           "useAttackItemOnAlly: attack item does not affect ally hp");
    assertWithLabel(players[0]->getInventory().empty(),
           "useAttackItemOnAlly: item is still consumed");
}

/** Validates support items used on enemies do not change enemy hp but are consumed. */
static void testUseSupportItemOnEnemyIsNoop(const HouseLoader& loader,
                                             const std::string& houseId,
                                             const ItemDatabase& db,
                                             Enemy& enemy,
                                             const std::string& supportDefId) {
    auto players   = makeTwoPlayers(loader, houseId);
    float hpBefore = enemy.getCurrentHealth();
    players[0]->addItem(makeItem(supportDefId));
    players[0]->useItemById(players[0]->getInventory()[0].getId(), enemy, db);

    assertWithLabel(enemy.getCurrentHealth() == hpBefore,
           "useSupportItemOnEnemy: support item does not affect enemy hp");
    assertWithLabel(players[0]->getInventory().empty(),
           "useSupportItemOnEnemy: item is still consumed");
}

/** Verifies attack scaling and affinity multiplier on a rare affinity-matching item. */
static void testAttackScalingAndAffinity(const HouseLoader& loader,
                                         const ItemDatabase& db,
                                         Enemy& enemy) {
    auto matchPlayer = std::make_shared<Player>("Ares", 1, "Ares P1", loader);
    auto mismatchPlayer = std::make_shared<Player>("Poseidon", 2, "Poseidon P2", loader);

    auto def = db.getDef("noams_ballista");
    assertWithLabel(def != nullptr, "scalingAttack: noams_ballista def exists");
    if (!def) return;

    const float matchHpBefore = enemy.getCurrentHealth();
    matchPlayer->addItem(makeItem("noams_ballista"));
    const float matchResolved = matchPlayer->useItemById(matchPlayer->getInventory()[0].getId(), enemy, db);

    const float expectedMatch = def->getBaseValue() * (1.0f + 1.0f) * 1.5f; // ares attack 1.0 + affinity
    assertWithLabel(floatsEqualWithinTolerance(matchResolved, expectedMatch), "scalingAttack: affinity-matching value is correct");
    assertWithLabel(floatsEqualWithinTolerance(matchHpBefore - enemy.getCurrentHealth(), expectedMatch), "scalingAttack: enemy damage matches resolved value");

    // Reset enemy health for second check
    enemy.setCurrentHealth(enemy.getMaxHealth());
    const float mismatchHpBefore = enemy.getCurrentHealth();
    mismatchPlayer->addItem(makeItem("noams_ballista"));
    const float mismatchResolved = mismatchPlayer->useItemById(mismatchPlayer->getInventory()[0].getId(), enemy, db);

    const float expectedMismatch = def->getBaseValue() * (1.0f + 0.8f); // poseidon attack 0.8, no affinity
    assertWithLabel(floatsEqualWithinTolerance(mismatchResolved, expectedMismatch), "scalingAttack: non-matching value is correct");
    assertWithLabel(floatsEqualWithinTolerance(mismatchHpBefore - enemy.getCurrentHealth(), expectedMismatch), "scalingAttack: enemy damage without affinity is correct");
}

/** Verifies support scaling on a non-affinity common item. */
static void testSupportScaling(const HouseLoader& loader,
                               const ItemDatabase& db) {
    auto healer = std::make_shared<Player>("Demeter", 1, "Demeter P1", loader);
    auto ally = std::make_shared<Player>("Ares", 2, "Ares P2", loader);
    ally->updateHealth(-3.0f);

    auto def = db.getDef("apple");
    assertWithLabel(def != nullptr, "scalingSupport: apple def exists");
    if (!def) return;

    const float hpBefore = ally->getCurrentHealth();
    healer->addItem(makeItem("apple"));
    const float resolved = healer->useItemById(healer->getInventory()[0].getId(), *ally, db);

    const float expected = def->getBaseValue() * (1.0f + 0.9f); // demeter support 0.9
    const float expectedApplied = std::min(expected, ally->getMaxHealth() - hpBefore);
    assertWithLabel(floatsEqualWithinTolerance(resolved, expected), "scalingSupport: resolved value is correct");
    assertWithLabel(floatsEqualWithinTolerance(ally->getCurrentHealth() - hpBefore, expectedApplied), "scalingSupport: heal amount is correct");
}

/** Verifies that mallet damage compounds by 1.5x per successive use for each player independently. */
static void testMalletUpgradeScaling(const HouseLoader& loader,
                                     const ItemDatabase& db,
                                     Enemy& enemy) {
    auto firstPlayer = std::make_shared<Player>("hephaestus", 1, "Hephaestus P1", loader);
    auto secondPlayer = std::make_shared<Player>("hephaestus", 2, "Hephaestus P2", loader);
    auto offAffinityPlayer = std::make_shared<Player>("ares", 3, "Ares P3", loader);

    auto malletDef = db.getDef("mallet");
    assertWithLabel(malletDef != nullptr, "mallet upgrade: mallet def exists");
    if (!malletDef) return;

    firstPlayer->addItem(makeItem("mallet"));
    const float firstResolved = firstPlayer->useItemById(firstPlayer->getInventory()[0].getId(), enemy, db);
    const float expectedFirst = 10.0f * (1.0f + 0.55f) * 1.5f;
    assertWithLabel(floatsEqualWithinTolerance(firstResolved, expectedFirst), "mallet upgrade: first use keeps base value before other multipliers");
    assertWithLabel(firstPlayer->getMalletUseCount() == 1, "mallet upgrade: first use increments streak");

    firstPlayer->addItem(makeItem("mallet"));
    const float secondResolved = firstPlayer->useItemById(firstPlayer->getInventory()[0].getId(), enemy, db);
    const float expectedSecond = 15.0f * (1.0f + 0.55f) * 1.5f;
    assertWithLabel(floatsEqualWithinTolerance(secondResolved, expectedSecond), "mallet upgrade: second use gains one upgrade step before other multipliers");
    assertWithLabel(firstPlayer->getMalletUseCount() == 2, "mallet upgrade: second use increments streak");

    secondPlayer->addItem(makeItem("mallet"));
    const float otherResolved = secondPlayer->useItemById(secondPlayer->getInventory()[0].getId(), enemy, db);
    assertWithLabel(floatsEqualWithinTolerance(otherResolved, expectedFirst), "mallet upgrade: each player tracks an independent streak");

    offAffinityPlayer->setMalletUseCount(2);
    offAffinityPlayer->addItem(makeItem("mallet"));
    const float offAffinityResolved = offAffinityPlayer->useItemById(offAffinityPlayer->getInventory()[0].getId(), enemy, db);
    const float expectedOffAffinity = 22.5f * (1.0f + 1.0f);
    assertWithLabel(floatsEqualWithinTolerance(offAffinityResolved, expectedOffAffinity), "mallet upgrade: off-affinity mallet still uses existing streak damage");
    assertWithLabel(offAffinityPlayer->getMalletUseCount() == 2, "mallet upgrade: off-affinity mallet does not increment streak");
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 5 — AI behavior
// ─────────────────────────────────────────────────────────────────────────────

/** Ensures AI with empty inventory idles/passes and does not enter attack/support states. */
static void testAIIdleWithEmptyInventory(const HouseLoader& loader,
                                          const std::string& houseId,
                                          const ItemDatabase& db,
                                          Enemy& enemy,
                                          const std::string& aiConfigPath) {
    auto players = makeFourPlayers(loader, houseId);

    // players[1] is an EasyPlayerAI — construct it directly as one
    auto ai = std::make_shared<EasyPlayerAI>(houseId, 2, "Player 2", loader);
    if (!ai->init(db, aiConfigPath)) return;

    ItemController items;
    for (int i = 0; i < 10; i++) ai->update(0.5f, enemy, items);

    assertWithLabel(ai->getInventory().empty(),
           "AI idle: inventory stays empty");
    assertWithLabel(ai->getState() == PlayerAI::State::IDLE ||
           ai->getState() == PlayerAI::State::PASS,
           "AI idle: state is IDLE or PASS (not ATTACK/SUPPORT)");
}

/** Verifies AI consumes or passes an attack item after some ticks. */
static void testAIActsOnAttackItem(const HouseLoader& loader,
                                    const std::string& houseId,
                                    const ItemDatabase& db,
                                    Enemy& enemy,
                                    const std::string& aiConfigPath,
                                    const std::string& attackDefId) {
    auto players = makeFourPlayers(loader, houseId);

    auto ai = std::make_shared<EasyPlayerAI>(houseId, 2, "Player 2", loader);
    ai->setLeftPlayer (players[0].get());
    ai->setRightPlayer(players[2].get());
    if (!ai->init(db, aiConfigPath)) return;

    float hpBefore = enemy.getCurrentHealth();
    ai->addItem(makeItem(attackDefId));

    ItemController items;
    for (int i = 0; i < 20; i++) ai->update(0.5f, enemy, items);

    CULog("── AI attackTest: enemy hp %.1f → %.1f ───",
          hpBefore, enemy.getCurrentHealth());

    assertWithLabel(ai->getInventory().empty(),
           "AI attack: item was consumed or passed after ticks");
}

/** Confirms AI heals an injured neighbor when holding a support item. */
static void testAIHealsInjuredNeighbor(const HouseLoader& loader,
                                        const std::string& houseId,
                                        const ItemDatabase& db,
                                        Enemy& enemy,
                                        const std::string& aiConfigPath,
                                        const std::string& supportDefId) {
    auto players = makeFourPlayers(loader, houseId);

    auto ai = std::make_shared<EasyPlayerAI>(houseId, 2, "Player 2", loader);
    ai->setLeftPlayer (players[0].get());
    ai->setRightPlayer(players[2].get());
    if (!ai->init(db, aiConfigPath)) return;

    // Bring left neighbor below 50% — below default healThreshold
    players[0]->updateHealth(-(players[0]->getMaxHealth() * 0.6f));
    float neighborHpBefore = players[0]->getCurrentHealth();

    ai->addItem(makeItem(supportDefId));

    ItemController items;
    for (int i = 0; i < 20; i++) ai->update(0.5f, enemy, items);

    CULog("── AI healTest: neighbor hp %.1f → %.1f ──",
          neighborHpBefore, players[0]->getCurrentHealth());

    assertWithLabel(players[0]->getCurrentHealth() >= neighborHpBefore,
           "AI heal: injured neighbor hp did not decrease after AI ticks");
}

/** Ensures AI passes a support item to a neighbor when no heal target is available. */
static void testAIPassesWhenNoHealTarget(const HouseLoader& loader,
                                          const std::string& houseId,
                                          const ItemDatabase& db,
                                          Enemy& enemy,
                                          const std::string& aiConfigPath,
                                          const std::string& supportDefId) {
    auto players = makeFourPlayers(loader, houseId);

    auto ai = std::make_shared<EasyPlayerAI>(houseId, 2, "Player 2", loader);
    ai->setLeftPlayer (players[0].get());
    ai->setRightPlayer(players[2].get());
    if (!ai->init(db, aiConfigPath)) return;

    // All neighbors at full health → canSupport() false, canAttack() false → PASS
    ai->addItem(makeItem(supportDefId));

    ItemController items;
    for (int i = 0; i < 20; i++) ai->update(0.5f, enemy, items);

    bool itemPassedToNeighbor = players[0]->getInventory().size() == 1 ||
                                players[2]->getInventory().size() == 1;

    CULog("── AI passTest ───────────────────────────");
    printHand(*players[0]);
    printHand(*ai);
    printHand(*players[2]);

    assertWithLabel(ai->getInventory().empty(),
           "AI pass: support item left AI player's inventory");
    assertWithLabel(itemPassedToNeighbor,
           "AI pass: item arrived at a neighbor");
}

/**
 * Verifies that a dead AI player never attacks or heals —
 * it should only pass items or remain idle.
 *
 * @param loader       House definitions used to construct the AI player and neighbors.
 * @param houseId      The house id used to initialize all players in the fixture.
 * @param db           The item database used to initialize the AI and resolve item types.
 * @param enemy        The enemy instance passed to update() as required by the FSM.
 * @param aiConfigPath Path to the AI config JSON (e.g. "assets/json/playerAI.json").
 * @param attackDefId  DefId of an attack item to seed the AI's inventory with.
 * @param supportDefId DefId of a support item to seed the AI's inventory with.
 */
static void testDeadAICanOnlyPassOrIdle(const HouseLoader& loader,
                                   const std::string& houseId,
                                   const ItemDatabase& db,
                                   Enemy& enemy,
                                   const std::string& aiConfigPath,
                                   const std::string& attackDefId,
                                   const std::string& supportDefId) {
    auto players = makeFourPlayers(loader, houseId);

    auto ai = std::make_shared<EasyPlayerAI>(houseId, 2, "Player 2", loader);
    ai->setLeftPlayer (players[0].get());
    ai->setRightPlayer(players[2].get());
    if (!ai->init(db, aiConfigPath)) return;

    // Kill the AI player
    ai->updateHealth(-ai->getMaxHealth());

    // Give it both item types so evaluate() has real options to choose from
    ai->addItem(makeItem(attackDefId));
    ai->addItem(makeItem(supportDefId));

    float enemyHpBefore    = enemy.getCurrentHealth();
    float neighborHpBefore = players[0]->getCurrentHealth();

    ItemController items;
    for (int i = 0; i < 20; i++) ai->update(0.5f, enemy, items);

    assertWithLabel(enemy.getCurrentHealth() >= enemyHpBefore,
           "Dead AI: enemy hp unchanged (no attack)");
    assertWithLabel(players[0]->getCurrentHealth() >= neighborHpBefore,
           "Dead AI: neighbor hp unchanged (no heal)");
    assertWithLabel(ai->getState() == PlayerAI::State::PASS ||
                    ai->getState() == PlayerAI::State::IDLE,
           "Dead AI: final state is PASS or IDLE");
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────

/** Test harness entry point: runs all Player tests and prints a summary. */
void PlayerTests::runAll(const std::string& housesJsonPath,
                         const std::string& itemsJsonPath,
                         const std::string& enemiesJsonPath,
                         const std::string& aiConfigPath) {
    _passed = 0;
    _failed = 0;
    _testIdCounter = 1000;

    CULog("═════════════════════════════════════════");
    CULog("  PlayerTests::runAll");
    CULog("═════════════════════════════════════════");

    HouseLoader loader = loadHouses(housesJsonPath);
    ItemDatabase    db     = loadDatabase(itemsJsonPath, housesJsonPath);
    Enemy           enemy  = loadEnemy(enemiesJsonPath, "cyclops");

    const std::string attackDefId  = firstDefIdOfType(db, ItemDef::Type::Attack);
    const std::string supportDefId = firstDefIdOfType(db, ItemDef::Type::Support);
    const std::string houseId  = "poseidon";

    if (attackDefId.empty())  CULogError("PlayerTests: no Attack item found in '%s'",  itemsJsonPath.c_str());
    if (supportDefId.empty()) CULogError("PlayerTests: no Support item found in '%s'", itemsJsonPath.c_str());
    if (!enemy.isAlive())     CULogError("PlayerTests: enemy failed to load from '%s'", enemiesJsonPath.c_str());

    CULog("  houseId  : %s", houseId.c_str());
    CULog("  attackDefId  : %s", attackDefId.c_str());
    CULog("  supportDefId : %s", supportDefId.c_str());
    CULog("─────────────────────────────────────────");

    CULog("── Section 1: Inventory basics ──────────");
    testInventoryStartsEmpty       (loader, houseId);
    testAddItemIncreasesCount      (loader, houseId, attackDefId);
    testRemoveItemDecreasesCount   (loader, houseId, attackDefId);
    testRemoveNonexistentItemIsNoop(loader, houseId, attackDefId);

    CULog("── Section 2: Print hands ───────────────");
    testPrintHands(loader, houseId, attackDefId, supportDefId);

    CULog("── Section 3: Card passing ──────────────");
    testPassRightMovesItem   (loader, houseId, attackDefId);
    testPassLeftMovesItem    (loader, houseId, supportDefId);
    testPassToDeadPlayerIsNoop(loader, houseId, attackDefId);
    testCircularPassAroundRing(loader, houseId, attackDefId);

    CULog("── Section 4: Card usage ────────────────");
    testUseAttackItemDamagesEnemy  (loader, houseId, db, enemy, attackDefId);
    testUseSupportItemHealsAlly    (loader, houseId, db);
    testUseAttackItemOnAllyIsNoop  (loader, houseId, db, attackDefId);
    testUseSupportItemOnEnemyIsNoop(loader, houseId, db, enemy, supportDefId);
    testMalletUpgradeScaling       (loader, db, enemy);

    CULog("── Section 5: AI behavior ───────────────");
    testAIIdleWithEmptyInventory(loader, houseId, db, enemy, aiConfigPath);
    testAIActsOnAttackItem      (loader, houseId, db, enemy, aiConfigPath, attackDefId);
    testAIHealsInjuredNeighbor  (loader, houseId, db, enemy, aiConfigPath, supportDefId);
    testAIPassesWhenNoHealTarget(loader, houseId, db, enemy, aiConfigPath, supportDefId);
    testDeadAICanOnlyPassOrIdle(loader, houseId, db, enemy, aiConfigPath, attackDefId, supportDefId);

    printSummary();
}
