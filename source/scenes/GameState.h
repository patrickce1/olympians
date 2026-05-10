#ifndef __GAME_STATE_H__
#define __GAME_STATE_H__

#include <vector>
#include <unordered_map>
#include <memory>
#include "../items/ItemDatabase.h"
#include "../Player.h"
#include "../Enemy.h"
#include "../HouseLoader.h"
#include "../items/ItemController.h"
#include "../playerAI/PlayerAI.h"
#include "../playerAI/EasyPlayerAI.h"
#include "../NetworkMessage.h"
#include "../bosses/Cyclops.h"
#include "../bosses/Gaia.h"

/**
 * Pure data model for the game world.
 *
 * GameState holds all world state data that needs to be replicated or
 * broadcast over the network. It has no knowledge of the scene graph,
 * rendering, or input — those responsibilities belong to GameScene.
 *
 * Intended networking flow:
 *   - Each client maintains a local GameState and speculatively applies
 *     its own inputs immediately to avoid input latency.
 *   - The client sends atomic action updates to the host.
 *   - The host applies each update to its authoritative GameState and
 *     broadcasts the new snapshot to all clients.
 *   - Clients overwrite their local GameState with the received snapshot.
 *
 * GameState owns:
 *   - All Player instances (human and AI), stored as shared_ptr<Player>.
 *   - The Enemy instance.
 *   - A mapping of network player IDs to Player pointers for routing.
 *   - A raw pointer to the local machine's player for fast access.
 */
class GameState {
public:

#pragma mark - Constructors

    /** Constructs an empty, uninitialised GameState. Call init() before use. */
    GameState() = default;

    /** Destroys the GameState, releasing all owned player and enemy resources. */
    ~GameState() { dispose(); }

    // Non-copyable — state should be moved or shared via pointer, never sliced.
    GameState(const GameState&)            = delete;
    GameState& operator=(const GameState&) = delete;

#pragma mark - Lifecycle

    /**
     * Loads house definitions from JSON into the house loader.
     * Must be called first since player construction depends on it.
     *
     * @return true if the house file loaded successfully.
     */
    bool initHouses();

    /**
     * Builds the player array (one human + three AI), links all players in a
     * circular neighbour ring, and populates the player ID map.
     * Must be called after initHouses().
     *
     * For now, we just pass in an integer, since if there are x real players, there will be the first x players in the game scene
     * Later, when we have reordering ability, this can be changed
     */
    void initPlayers();
    
    /**
     * Assigns a unique house to every slot that does not yet have one.
     * Skips any slot that already has a house. For empty slots, builds a pool
     * of houses not yet claimed by any other slot, picks one at random, and
     * reconstructs the slot as an EasyPlayerAI with that house so AI behavior
     * is preserved. The pool is rebuilt each iteration so previously assigned
     * houses are excluded.
     *
     * Host only — rand() is called locally so clients must receive the results
     * via broadcastAIHouseSelection() rather than running this themselves.
     *
     * @param itemController  The ItemController whose database AI players need
     *                        to initialise their behavior after reconstruction.
     */
    void assignMissingHousesForAI(ItemController& itemController);

    /**
     * Replaces the AI placeholder at the given slot with a real human player.
     * If houseName is provided, constructs a full Player with house stats.
     * If houseName is empty, updates only the player's display name without
     * reconstructing the object — safe to call before house selection.
     *
     * After any replacement, all neighbour pointers in the circular ring are
     * re-wired so every player's left/right references remain valid.
     *
     * @param playerNumber  The 0-based slot index of the player to promote.
     * @param playerName    The display name of the player joining this slot.
     * @param houseName     The ID of the house the player selected. If empty,
     *                      only the name is updated and no reconstruction occurs.
     */
    void setRealPlayer(int playerNumber, const std::string& playerName, const std::string& houseName = "");

    /**
     * Loads and initialises the enemy from JSON.
     *
     * @return true if the enemy loaded and initialised successfully.
     */
    bool initEnemy();
    
    /** Initializes the enemy with animation metadata from AssetManager. 
     *  @param assets The AssetManager containing animation metadata in enemyAnimations.json
     *  @return true if initialization succeeds, false on error
    */
    bool initEnemyWithAssets(const std::shared_ptr<cugl::AssetManager>& assets);

    /**
     * Finishes initialising all AI-controlled players using the item database.
     * Must be called after initPlayers() and after the ItemController is ready.
     *
     * @param itemController  The ItemController whose database the AI players need.
     * @return true if all AI players initialised successfully.
     */
    bool initAI(ItemController& itemController);
    
    /**
     * Initialises the game world: loads houses and the enemy from JSON,
     * builds the player array (one human + three AI), links all players in a
     * circular neighbour ring, and finishes AI initialisation using the
     * provided item database.
     *
     * @param itemController  The ItemController whose database is needed for
     *                        AI player initialisation.
     * @param assets          The AssetManager containing animation metadata and
     *                        asset definitions needed for enemy initialisation.
     * @return true if all resources loaded and initialised successfully.
     */
    bool init(ItemController& itemController, const std::shared_ptr<cugl::AssetManager>& assets);

    /**
     * Releases all owned resources and resets every pointer to nullptr.
     * Safe to call even if init() was never called.
     */
    void dispose();

    /**
     * Resets all players' inventories to their default state.
     * Does not reload assets or rebuild the player array.
     * Called at the start of every round via GameScene::reset().
     */
    void reset();

    /** Convinient way to update the game state and inventory by just providing the struct from the network controller. USED BY CLIENTS */
    void networkUpdate(GameStateMessage updatedState);

    /** Convinient functions to handle updates recieved from the network. USED BY THE HOST
     *  Updates the gameState object by applying all the damage present in the messages of `attacks`
     */
    void attackUpdates(std::vector<AttackMessage> attacks);

    /** Updates the gameState object by handling all healing requests in the messages in `heals` */
    void healUpdates(std::vector<HealMessage> heals);

    /**
     * Applies all queued boss heal messages to the enemy's current health.
     * Called by the host each frame after processing incoming network messages.
     * Currently used exclusively for Gaia's rock item, which heals the boss
     * instead of dealing damage.
     *
     * @param bossHeals  The queued boss heal updates to apply this frame.
     */
    void bossHealUpdates(std::vector<BossHealMessage> bossHeals);

    /**
     * Applies support effect messages received from clients to the authoritative game state.
     *
     * @param supportEffects  The queued support-effect updates to apply this frame.
     */
    void supportEffectUpdates(std::vector<SupportEffectMessage> supportEffects);
    
    /**
     * Applies enemy effect messages received from clients to the authoritative game state.
     *
     * @param enemyEffects  The queued enemy-effect updates to apply this frame.
     */
    void enemyEffectUpdates(std::vector<EnemyEffectMessage> enemyEffects);
    
#pragma mark - Player Access

    /**
     * Returns all players in the party, including human and AI slots.
     *
     * @return A reference to the player array.
     */
    std::vector<std::shared_ptr<Player>>& getPlayers() { return _players; }

    /** @copydoc getPlayers() */
    const std::vector<std::shared_ptr<Player>>& getPlayers() const { return _players; }

    /**
     * Returns a raw pointer to the player assigned to the local machine.
     * Points into _players and is never independently owned.
     *
     * @return The local player, or nullptr if setLocalPlayer() has not been called.
     */
    Player* getLocalPlayer() const { return _localPlayer; }

    /**
     * Assigns the local player by index into the player array.
     * Must be called once after init(). In a networked session this is
     * called again with the host-assigned slot index after the lobby starts.
     *
     * @param assignedIndex  Zero-based index into the player array.
     */
    void setLocalPlayer(int assignedIndex);

    /**
     * Returns the player associated with a given network player ID.
     * Used by the networking layer to route incoming action messages to
     * the correct player instance.
     *
     * @param playerId  The network-assigned player ID.
     * @return          The matching Player pointer, or nullptr if not found.
     */
    Player* getPlayerById(int playerId) const;
    
    /**
     * Returns a raw pointer to the player at the given slot index.
     *
     * @param  slot  Zero-based index into the player array.
     * @return      The Player at that slot, or nullptr if out of range.
     */
    Player* getPlayerBySlot(int slot) const;
    
    /** Returns the character loader, needed when constructing replacement players. */
    const HouseLoader& getHouseLoader() const { return _houseLoader; }

#pragma mark - Enemy Access

    /**
     * Returns the enemy for this game session.
     *
     * @return A shared pointer to the Enemy, or nullptr if not yet initialised.
     */
    std::shared_ptr<Enemy> getEnemy() const { return _enemy; }
    
    /**
     * Assigns the enemy for the game session.
     *
     * @param enemyId  the unique ID of the chosen enemy.
     */
    void setEnemy(std::string enemyID);
    
    /**
     * Assigns the enemy for the game session with animation assets loaded.
     * Ensures animation metadata is properly loaded.
     * 
     * @param enemyID  the unique ID of the chosen enemy.
     * @param assets   the AssetManager containing animation data.
     */
    void setEnemy(std::string enemyID, const std::shared_ptr<cugl::AssetManager>& assets);

#pragma mark - Game State Checking
    /* Returns whether or not the players won based on the current game state*/
    bool didWin();

    /* Returns whether or not the players lost based on the current game state*/
    bool didLose();
    
    /**
     * Replaces the player at the given slot with an EasyPlayerAI, optionally
     * preserving their house. Re-wires the neighbour ring and updates the
     * player ID map. Note: caller must call ai->init() after this to set _db.
     *
     * @param slot   The 0-based slot index of the player to demote.
     * @param house  The house ID to assign to the new AI, or "" for none.
     */
    void demoteToAI(int slot, const std::string& house = "");
    
    /**
     * Swaps two player slots in the local player array.
     * Called on the host after NetworkController::swapSlots() to keep
     * _players in sync with the updated network slot assignments.
     * Re-wires neighbour pointers for the affected slots after the swap.
     *
     * @param slotA  First 0-based slot index.
     * @param slotB  Second 0-based slot index.
     */
    void swapPlayers(int slotA, int slotB);

    /**
     * Reorders the player array according to a permutation supplied by the boss
     * scramble mechanic.  After reordering, all neighbour pointers in the
     * circular ring are re-wired and the player ID map is rebuilt.
     *
     * The permutation is expressed as a "where does slot i go?" mapping:
     *   newSlot = permutation[oldSlot]
     * e.g. permutation = {2, 0, 3, 1} moves
     *   old slot 0 → new slot 2
     *   old slot 1 → new slot 0
     *   old slot 2 → new slot 3
     *   old slot 3 → new slot 1
     *
     * Host only — clients must receive the permutation over the network and
     * call this with the same array so all peers stay in sync.
     *
     * @param permutation  A length-4 array where permutation[i] is the new
     *                     slot index that the player currently at slot i
     *                     should occupy.  Must be a valid permutation of
     *                     {0, 1, 2, 3}; behaviour is undefined otherwise.
     */
    void applyPlayerScramble(const std::array<int, 4>& mapping);

private:

    /**
     * All players in this party — both human-controlled (Player) and
     * AI-controlled (PlayerAI) — stored polymorphically as shared_ptr<Player>.
     * Index 0 is the human player; indices 1+ are AI bots.
     * PlayerAI must inherit from Player for virtual dispatch to work.
     */
    std::vector<std::shared_ptr<Player>> _players;

    /**
     * Raw pointer to the player belonging to the local machine.
     * Points into _players and is never independently owned.
     * Set via setLocalPlayer(); never reallocated after that.
     */
    Player* _localPlayer = nullptr;

    /**
     * Maps network player IDs to their corresponding Player instances.
     * Populated during init() in parallel with _players.
     * The int key matches the network-assigned player ID (== array index
     * for now; may diverge once real lobby assignment is implemented).
     */
    std::unordered_map<int, Player*> _playerIdMap;

    /** The enemy for this game session. */
    std::shared_ptr<Enemy> _enemy;
    
    /** Asset manager for loading animation metadata. */
    std::shared_ptr<cugl::AssetManager> _assets;

    /** Loads house definitions from JSON for player construction. */
    HouseLoader _houseLoader;

    /** Item database used by the host to resolve authoritative attack magnitudes. */
    const ItemDatabase* _itemDatabase = nullptr;
};

#endif /* __GAME_STATE_H__ */
