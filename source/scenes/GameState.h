#ifndef __GAME_STATE_H__
#define __GAME_STATE_H__

#include <vector>
#include <unordered_map>
#include <memory>
#include "../Player.h"
#include "../Enemy.h"
#include "../CharacterLoader.h"
#include "../items/ItemController.h"
#include "../playerAI/PlayerAI.h"
#include "../playerAI/EasyPlayerAI.h"

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
     * Initialises the game world: loads characters and the enemy from JSON,
     * builds the player array (one human + three AI), links all players in a
     * circular neighbour ring, and finishes AI initialisation using the
     * provided item database.
     *
     * @param itemController  The ItemController whose database is needed for
     *                        AI player initialisation.
     * @return true if all resources loaded and initialised successfully.
     */
    bool init(ItemController& itemController);

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

#pragma mark - Enemy Access

    /**
     * Returns the enemy for this game session.
     *
     * @return A shared pointer to the Enemy, or nullptr if not yet initialised.
     */
    std::shared_ptr<Enemy> getEnemy() const { return _enemy; }

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

    /** Loads character definitions from JSON for player construction. */
    CharacterLoader _characterLoader;
};

#endif /* __GAME_STATE_H__ */
