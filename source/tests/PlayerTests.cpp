// PlayerTests.cpp
// Unit tests for Player inventory, card passing, card usage, and AI behavior.
//
// HOW TO RUN:
//   Call PlayerTests::runAll() from your app startup AFTER cugl is initialized
//   but BEFORE the game loop starts, e.g. in AppDelegate::onStartup().
//   All results print via CULog as [PASS] or [FAIL]. Remove the call before shipping.
//
//   Example:
//     PlayerTests::runAll("json/characters.json",
//                         "json/items.json",
//                         "json/enemies.json",
//                         "assets/json/playerAI.json");

#include "PlayerTests.h"
#include "Player.h"
#include "Enemy.h"
#include "EasyPlayerAI.h"
#include "ItemDatabase.h"
#include "ItemDef.h"
#include "ItemInstance.h"
#include "ItemController.h"
#include "CharacterLoader.h"
#include <cugl/cugl.h>

// ─────────────────────────────────────────────
// Internal helpers — not exposed in the header
// ─────────────────────────────────────────────
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
    CULog("─────────────────────────────────────────");
    CULog("  %d passed   %d failed", _passed, _failed);
    CULog("─────────────────────────────────────────");
}

// Prints every item in a player's hand to CULog — purely for visual debugging
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

// ── Loaders ─────────────────────────────────

ItemDatabase loadDatabase(const std::string& itemsJsonPath) {
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

    db.setStartingPointWithTime();
    return db;
}

CharacterLoader loadCharacters(const std::string& charactersJsonPath) {
    CharacterLoader loader;
    if (!loader.loadFromFile(charactersJsonPath)) {
        CULogError("PlayerTests: failed to load characters from '%s'", charactersJsonPath.c_str());
    }
    return loader;
}

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

ItemInstance makeItem(const std::string& defId) {
    auto inst = ItemInstance::alloc(defId, _testIdCounter++);
    CUAssertLog(inst != nullptr, "PlayerTests: makeItem failed for defId '%s'", defId.c_str());
    return *inst;
}

// ── Player factories ─────────────────────────

std::vector<std::shared_ptr<Player>> makeTwoPlayers(const CharacterLoader& loader,
                                                    const std::string& characterId) {
    std::vector<std::shared_ptr<Player>> players;
    players.push_back(std::make_shared<Player>(characterId, 1, "Player 1", loader));
    players.push_back(std::make_shared<Player>(characterId, 2, "Player 2", loader));
    players[0]->setRightPlayer(players[1].get());
    players[0]->setLeftPlayer (players[1].get());
    players[1]->setRightPlayer(players[0].get());
    players[1]->setLeftPlayer (players[0].get());
    return players;
}

std::vector<std::shared_ptr<Player>> makeFourPlayers(const CharacterLoader& loader,
                                                     const std::string& characterId) {
    std::vector<std::shared_ptr<Player>> players;
    players.push_back(std::make_shared<Player>(characterId, 1, "Player 1", loader));
    players.push_back(std::make_shared<Player>(characterId, 2, "Player 2", loader));
    players.push_back(std::make_shared<Player>(characterId, 3, "Player 3", loader));
    players.push_back(std::make_shared<Player>(characterId, 4, "Player 4", loader));

    const int n = (int)players.size();
    for (int i = 0; i < n; i++) {
        players[i]->setLeftPlayer (players[(i - 1 + n) % n].get());
        players[i]->setRightPlayer(players[(i + 1)     % n].get());
    }
    return players;
}

// ── Utility: first defId of a given type ────

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

static void testInventoryStartsEmpty(const CharacterLoader& loader,
                                     const std::string& characterId) {
    auto players = makeTwoPlayers(loader, characterId);
    expect(players[0]->getInventory().empty(), "Inventory starts empty");
}

static void testAddItemIncreasesCount(const CharacterLoader& loader,
                                      const std::string& characterId,
                                      const std::string& attackDefId) {
    auto players = makeTwoPlayers(loader, characterId);
    players[0]->addItem(makeItem(attackDefId));
    expect(players[0]->getInventory().size() == 1,
           "addItem: inventory count increases to 1");
}

static void testRemoveItemDecreasesCount(const CharacterLoader& loader,
                                         const std::string& characterId,
                                         const std::string& attackDefId) {
    auto players  = makeTwoPlayers(loader, characterId);
    ItemInstance item = makeItem(attackDefId);
    players[0]->addItem(item);
    players[0]->removeItemById(item.getId());
    expect(players[0]->getInventory().empty(),
           "removeItemById: inventory count drops to 0");
}

static void testRemoveNonexistentItemIsNoop(const CharacterLoader& loader,
                                            const std::string& characterId,
                                            const std::string& attackDefId) {
    auto players  = makeTwoPlayers(loader, characterId);
    ItemInstance item = makeItem(attackDefId);
    players[0]->addItem(item);
    players[0]->removeItemById(item.getId() + 99999);  // bogus id
    expect(players[0]->getInventory().size() == 1,
           "removeItemById: does nothing when no id match, real items untouched");
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 2 — Print hands (visual / debug)
// ─────────────────────────────────────────────────────────────────────────────

static void testPrintHands(const CharacterLoader& loader,
                            const std::string& characterId,
                            const std::string& attackDefId,
                            const std::string& supportDefId) {
    CULog("── printHands ────────────────────────────");
    auto players = makeFourPlayers(loader, characterId);

    players[0]->addItem(makeItem(attackDefId));
    players[0]->addItem(makeItem(supportDefId));
    players[1]->addItem(makeItem(attackDefId));
    players[2]->addItem(makeItem(supportDefId));
    players[2]->addItem(makeItem(supportDefId));
    // players[3] intentionally empty

    for (const auto& p : players) printHand(*p);
    CULog("──────────────────────────────────────────");
    expect(true, "printHands: ran without crash");
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 3 — Card passing
// ─────────────────────────────────────────────────────────────────────────────

static void testPassRightMovesItem(const CharacterLoader& loader,
                                   const std::string& characterId,
                                   const std::string& attackDefId) {
    auto players = makeTwoPlayers(loader, characterId);
    players[0]->addItem(makeItem(attackDefId));

    Player*      target = players[0]->getRightPlayer();
    ItemInstance item   = players[0]->getInventory()[0];
    players[0]->removeItemById(item.getId());
    target->addItem(item);

    CULog("── passRight ─────────────────────────────");
    printHand(*players[0]);
    printHand(*players[1]);

    expect(players[0]->getInventory().empty(),       "passRight: sender inventory is empty");
    expect(players[1]->getInventory().size() == 1,   "passRight: receiver has 1 item");
    expect(players[1]->getInventory()[0].getDefId() == attackDefId,
                                                     "passRight: received item has correct defId");
}

static void testPassLeftMovesItem(const CharacterLoader& loader,
                                  const std::string& characterId,
                                  const std::string& supportDefId) {
    auto players = makeTwoPlayers(loader, characterId);
    players[0]->addItem(makeItem(supportDefId));

    Player*      target = players[0]->getLeftPlayer();
    ItemInstance item   = players[0]->getInventory()[0];
    players[0]->removeItemById(item.getId());
    target->addItem(item);

    CULog("── passLeft ──────────────────────────────");
    printHand(*players[0]);
    printHand(*players[1]);

    expect(players[0]->getInventory().empty(),     "passLeft: sender inventory is empty");
    expect(players[1]->getInventory().size() == 1, "passLeft: receiver has 1 item");
}

static void testPassToDeadPlayerIsNoop(const CharacterLoader& loader,
                                       const std::string& characterId,
                                       const std::string& attackDefId) {
    auto players = makeTwoPlayers(loader, characterId);
    players[1]->updateHealth(-999999.0f);
    expect(!players[1]->isAlive(), "passToDeadPlayer: target confirmed dead");

    players[0]->addItem(makeItem(attackDefId));

    Player* target = players[0]->getRightPlayer();
    if (target && target->isAlive()) {
        ItemInstance item = players[0]->getInventory()[0];
        players[0]->removeItemById(item.getId());
        target->addItem(item);
    }

    expect(players[0]->getInventory().size() == 1,
           "passToDeadPlayer: item stays with sender");
}

static void testCircularPassAroundRing(const CharacterLoader& loader,
                                       const std::string& characterId,
                                       const std::string& attackDefId) {
    auto players = makeFourPlayers(loader, characterId);
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

    expect(players[0]->getInventory().size() == 1,
           "circularPass: item returns to origin after full loop");
    expect(players[1]->getInventory().empty() &&
           players[2]->getInventory().empty() &&
           players[3]->getInventory().empty(),
           "circularPass: only origin player holds the item");
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 4 — Card usage (attack and support)
// ─────────────────────────────────────────────────────────────────────────────

static void testUseAttackItemDamagesEnemy(const CharacterLoader& loader,
                                          const std::string& characterId,
                                          const ItemDatabase& db,
                                          Enemy& enemy,
                                          const std::string& attackDefId) {
    auto players   = makeTwoPlayers(loader, characterId);
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

    expect(enemy.getCurrentHealth() < hpBefore,   "useAttackItem: enemy hp decreased");
    expect(players[0]->getInventory().empty(),     "useAttackItem: item consumed from inventory");
}

static void testUseSupportItemHealsAlly(const CharacterLoader& loader,
                                        const std::string& characterId,
                                        const ItemDatabase& db,
                                        const std::string& supportDefId) {
    auto players = makeTwoPlayers(loader, characterId);
    players[1]->updateHealth(-20.0f);
    float hpBefore = players[1]->getCurrentHealth();

    players[0]->addItem(makeItem(supportDefId));

    for (const ItemInstance& item : players[0]->getInventory()) {
        auto def = db.getDef(item.getDefId());
        if (def && def->getType() == ItemDef::Type::Support) {
            players[0]->useItemById(item.getId(), *players[1], db);
            break;
        }
    }

    CULog("── healAlly: hp %.1f → %.1f ──────────────",
          hpBefore, players[1]->getCurrentHealth());

    expect(players[1]->getCurrentHealth() > hpBefore, "useSupportItem: ally hp increased");
    expect(players[0]->getInventory().empty(),         "useSupportItem: item consumed from inventory");
}

static void testUseAttackItemOnAllyIsNoop(const CharacterLoader& loader,
                                          const std::string& characterId,
                                          const ItemDatabase& db,
                                          const std::string& attackDefId) {
    auto players   = makeTwoPlayers(loader, characterId);
    float hpBefore = players[1]->getCurrentHealth();
    players[0]->addItem(makeItem(attackDefId));
    players[0]->useItemById(players[0]->getInventory()[0].getId(), *players[1], db);

    expect(players[1]->getCurrentHealth() == hpBefore,
           "useAttackItemOnAlly: attack item does not affect ally hp");
    expect(players[0]->getInventory().empty(),
           "useAttackItemOnAlly: item is still consumed");
}

static void testUseSupportItemOnEnemyIsNoop(const CharacterLoader& loader,
                                             const std::string& characterId,
                                             const ItemDatabase& db,
                                             Enemy& enemy,
                                             const std::string& supportDefId) {
    auto players   = makeTwoPlayers(loader, characterId);
    float hpBefore = enemy.getCurrentHealth();
    players[0]->addItem(makeItem(supportDefId));
    players[0]->useItemById(players[0]->getInventory()[0].getId(), enemy, db);

    expect(enemy.getCurrentHealth() == hpBefore,
           "useSupportItemOnEnemy: support item does not affect enemy hp");
    expect(players[0]->getInventory().empty(),
           "useSupportItemOnEnemy: item is still consumed");
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 5 — AI behavior
// ─────────────────────────────────────────────────────────────────────────────

static void testAIIdleWithEmptyInventory(const CharacterLoader& loader,
                                          const std::string& characterId,
                                          const ItemDatabase& db,
                                          Enemy& enemy,
                                          const std::string& aiConfigPath) {
    auto players = makeFourPlayers(loader, characterId);

    // players[1] is an EasyPlayerAI — construct it directly as one
    auto ai = std::make_shared<EasyPlayerAI>(characterId, 2, "Player 2", loader);
    if (!ai->init(db, aiConfigPath)) return;

    ItemController items;
    for (int i = 0; i < 10; i++) ai->update(0.5f, enemy, items);

    expect(ai->getInventory().empty(),
           "AI idle: inventory stays empty");
    expect(ai->getState() == PlayerAI::State::IDLE ||
           ai->getState() == PlayerAI::State::PASS,
           "AI idle: state is IDLE or PASS (not ATTACK/SUPPORT)");
}

static void testAIActsOnAttackItem(const CharacterLoader& loader,
                                    const std::string& characterId,
                                    const ItemDatabase& db,
                                    Enemy& enemy,
                                    const std::string& aiConfigPath,
                                    const std::string& attackDefId) {
    auto players = makeFourPlayers(loader, characterId);

    auto ai = std::make_shared<EasyPlayerAI>(characterId, 2, "Player 2", loader);
    ai->setLeftPlayer (players[0].get());
    ai->setRightPlayer(players[2].get());
    if (!ai->init(db, aiConfigPath)) return;

    float hpBefore = enemy.getCurrentHealth();
    ai->addItem(makeItem(attackDefId));

    ItemController items;
    for (int i = 0; i < 20; i++) ai->update(0.5f, enemy, items);

    CULog("── AI attackTest: enemy hp %.1f → %.1f ───",
          hpBefore, enemy.getCurrentHealth());

    expect(ai->getInventory().empty(),
           "AI attack: item was consumed or passed after ticks");
}

static void testAIHealsInjuredNeighbor(const CharacterLoader& loader,
                                        const std::string& characterId,
                                        const ItemDatabase& db,
                                        Enemy& enemy,
                                        const std::string& aiConfigPath,
                                        const std::string& supportDefId) {
    auto players = makeFourPlayers(loader, characterId);

    auto ai = std::make_shared<EasyPlayerAI>(characterId, 2, "Player 2", loader);
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

    expect(players[0]->getCurrentHealth() >= neighborHpBefore,
           "AI heal: injured neighbor hp did not decrease after AI ticks");
}

static void testAIPassesWhenNoHealTarget(const CharacterLoader& loader,
                                          const std::string& characterId,
                                          const ItemDatabase& db,
                                          Enemy& enemy,
                                          const std::string& aiConfigPath,
                                          const std::string& supportDefId) {
    auto players = makeFourPlayers(loader, characterId);

    auto ai = std::make_shared<EasyPlayerAI>(characterId, 2, "Player 2", loader);
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

    expect(ai->getInventory().empty(),
           "AI pass: support item left AI player's inventory");
    expect(itemPassedToNeighbor,
           "AI pass: item arrived at a neighbor");
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────

void PlayerTests::runAll(const std::string& charactersJsonPath,
                         const std::string& itemsJsonPath,
                         const std::string& enemiesJsonPath,
                         const std::string& aiConfigPath) {
    _passed = 0;
    _failed = 0;
    _testIdCounter = 1000;

    CULog("═════════════════════════════════════════");
    CULog("  PlayerTests::runAll");
    CULog("═════════════════════════════════════════");

    CharacterLoader loader = loadCharacters(charactersJsonPath);
    ItemDatabase    db     = loadDatabase(itemsJsonPath);
    Enemy           enemy  = loadEnemy(enemiesJsonPath, "enemy1");

    const std::string attackDefId  = firstDefIdOfType(db, ItemDef::Type::Attack);
    const std::string supportDefId = firstDefIdOfType(db, ItemDef::Type::Support);
    const std::string characterId  = "Percy";

    if (attackDefId.empty())  CULogError("PlayerTests: no Attack item found in '%s'",  itemsJsonPath.c_str());
    if (supportDefId.empty()) CULogError("PlayerTests: no Support item found in '%s'", itemsJsonPath.c_str());
    if (!enemy.isAlive())     CULogError("PlayerTests: enemy failed to load from '%s'", enemiesJsonPath.c_str());

    CULog("  characterId  : %s", characterId.c_str());
    CULog("  attackDefId  : %s", attackDefId.c_str());
    CULog("  supportDefId : %s", supportDefId.c_str());
    CULog("─────────────────────────────────────────");

    CULog("── Section 1: Inventory basics ──────────");
    testInventoryStartsEmpty       (loader, characterId);
    testAddItemIncreasesCount      (loader, characterId, attackDefId);
    testRemoveItemDecreasesCount   (loader, characterId, attackDefId);
    testRemoveNonexistentItemIsNoop(loader, characterId, attackDefId);

    CULog("── Section 2: Print hands ───────────────");
    testPrintHands(loader, characterId, attackDefId, supportDefId);

    CULog("── Section 3: Card passing ──────────────");
    testPassRightMovesItem   (loader, characterId, attackDefId);
    testPassLeftMovesItem    (loader, characterId, supportDefId);
    testPassToDeadPlayerIsNoop(loader, characterId, attackDefId);
    testCircularPassAroundRing(loader, characterId, attackDefId);

    CULog("── Section 4: Card usage ────────────────");
    testUseAttackItemDamagesEnemy  (loader, characterId, db, enemy, attackDefId);
    testUseSupportItemHealsAlly    (loader, characterId, db, supportDefId);
    testUseAttackItemOnAllyIsNoop  (loader, characterId, db, attackDefId);
    testUseSupportItemOnEnemyIsNoop(loader, characterId, db, enemy, supportDefId);

    CULog("── Section 5: AI behavior ───────────────");
    testAIIdleWithEmptyInventory(loader, characterId, db, enemy, aiConfigPath);
    testAIActsOnAttackItem      (loader, characterId, db, enemy, aiConfigPath, attackDefId);
    testAIHealsInjuredNeighbor  (loader, characterId, db, enemy, aiConfigPath, supportDefId);
    testAIPassesWhenNoHealTarget(loader, characterId, db, enemy, aiConfigPath, supportDefId);

    printSummary();
}
