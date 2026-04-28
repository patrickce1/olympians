//Networking class
#ifndef __NETWORKING_CONTROLLER__
#define __NETWORKING_CONTROLLER__

#include <cugl/cugl.h>
#include "NetworkMessage.h"
#include "scenes/GameState.h"

/*
The networking controller creates an abstraction for sending messages over the network.

It handles sending and recieving bytes of data and decoding them into easy to read structs and getter methods

It does not handle actually updating the state of the game. It is up to the individual scenes to update themselves based on
the messages that they extract from the NetworkController.

IMPORTANT: before collecting updates, make sure you call the getNetworkUpdates() and when you are done processing all data, make sure to
call clearQueues() so that old messages don't show up in the next frame
*/

class NetworkController {
public:

	enum Status {
        FAILED,
        WAITING,
        CONNECTED,
        STARTED,
        ONGOING
	};
    
     /**
      * Creates a new host scene with the default values.
      *
      * This constructor does not allocate any objects or start the game.
      * This allows us to use the object without a heap pointer.
      */
    NetworkController() {}

    /**
     * Disposes of all (non-static) resources allocated to this mode.
     *
     * This method is different from dispose() in that it ALSO shuts off any
     * static resources, like the input controller.
     */
    ~NetworkController() { dispose(); }

    //disposes of all non-static resoueces allocated to this mode.
    void dispose();

    /**
    * Initializes the NetworkController with the server configuration from assets.
    * Must be called once after assets have finished loading, before any
    * network calls are made.
    *
    * @param assets    The loaded asset manager.
    * @return          true if initialization succeeded, false if assets is null
    *                  or the server config could not be loaded.
    */
    bool init(const std::shared_ptr<cugl::AssetManager>& assets);

    /** Returns the current state of the connection. Check the Status enum for possible values */
    Status checkConnection();

    /** Tells the network controller to
     *  Calling this function will populate the message queues and variables with new information
     */
    void getNetworkUpdates();

    /*Clears all message queues. 
    * You MUST do this before tou call getNetworkUpdates() again, 
    unless you want all past messages still in the queue*/
    void clearQueues();

    /**
     * Connects to the game server as specified in the assets file
     *
     * The {@link #init} method set the configuration data. This method simply uses
     * this to create a new {@Link NetworkConnection}.
     *
     * @param room  The room ID to use. Should be a 5-digit base 10 number
     *
     * To check the status of the connection, use {@link #checkConnection()}
     */
    void joinRoom(const std::string room);

    /** Creates a lobby for other players to join,
     * IMPORTANT: the program could be still negotiating connection
     * by the end of the function. Use {@link #checkConnection()} to ensure status of connection
     */
    void hostRoom();

    /*Returns a string with the room id if the room exists. Nullptr otherwise*/
    std::string getRoom();

    /*Disconnects the player. If it's the host, moves the host*/
    void disconnect();

    /*Because the original host can disconnect, this is used to keep track of host migration*/
    bool isHost();

    /* Atomic style update functions. The following are ONLY SENT TO THE HOST*/
    
    /**
     * Sends an attack message to the host with the given damage value.
     * Called by non-host clients when the local player attacks the boss.
     *
     * @param damage    The amount of damage dealt to the boss.
     * @param playerIndex Which player is dealing damage to the boss
    */
    void broadcastDamage(float damageAmount, int playerIndex);

    /**
      * Sends a message about the boss being healed by a player
      * For now, intended to only be used 
      * 
      * @param healAmount is the amount of health healed
      */
    void broadcastBossHeal(float healAmount);

    /**
     * Sends a message to the corresponding player that an item with the given definition has been passed to them.
     * If sent to an AI player, the host handles it; otherwise, the receiving player handles it on their end.
     *
     * @param itemDefID     The item definition ID of the item being passed.
     * @param playerID      The player's position in the circle (0-based).
     * @param passDirection The direction the item is being passed: 1 for left, 2 for right.
     */
    void broadcastPass(const std::string& itemDefID, int playerID, int passDirection);

    /** Sends a message to the host that the player located at playerID in the cicle got healed for healAmount. */
    void broadcastHeal(float healAmount, int playerID);

    /**
     * Sends a Gaia spawn message to the player at the given slot.
     * If the target is a real player, sends directly to their network UUID.
     * Otherwise, does nothing because host is expected to locally handle spawning in the hands of AI player
     *
     * @param playerID  The 0-based index of the player to send the Gaia spawn to.
     */
     void broadcastGaiaSpawn(int playerID);

    /**
     * Sends a support effect application to the host for authoritative processing.
     *
     * @param effectType The kind of support effect that was applied.
     * @param magnitude  The resolved magnitude of the effect.
     * @param duration   The timed duration of the effect, or 0 for instant effects.
     * @param playerID   The 0-based index of the player receiving the effect.
     */
    void broadcastSupportEffect(SupportEffectType effectType, float magnitude, float duration, int playerID);
    
    /**
     * Sends an enemy-affecting attack effect to the host for authoritative processing.
     *
     * @param effectType The kind of enemy effect that was applied.
     * @param magnitude  The resolved magnitude of the effect.
     * @param duration   The timed duration of the effect, or 0 for instant effects.
     * @param playerIndex The attacking player's slot, used for side-relative effects.
     * @param applyToAllSides Whether the enemy effect should be applied to all four boss sides.
     */
    void broadcastEnemyEffect(EnemyEffectType effectType, float magnitude, float duration, int playerIndex, bool applyToAllSides);

    /** The following are USED ONLY BY THE HOST */
    /** Send the GameState state as the new authoritative version of the game to all players */
    void broadcastGameState(const GameState& state);

    /** Send a message to all clients that the game has been lost */
    void broadcastLostGame();

    /** Send a message to all clients that the game has been won */
    void broadcastWonGame();

    /*Client-Side Lobby Messages*/
    /*Sends player username to the host*/
    void broadcastJoinedLobby();

    /*Host-Side Lobby Messages*/
    /*Notifies all clients that the game has started*/
    void broadcastGameStart();

    /*Sends an update notifying players about changes to the lobby (new players joining/leaving)*/
    void broadcastLobbyState();
    
    /**
     * Sends the local player's house selection to the host.
     *
     * Note: because sendToHost() does not loop back to the sender,
     * the host must call setLocalHouse() separately after this to
     * update their own slot.
     *
     * @param house  The ID of the selected house (e.g. "athena").
     *               Passing an empty string clears the selection.
     */
    void broadcastSelectedHouse(const std::string& house);
    
    /*HOST ONLY. Notifies all clients that the host has exited the lobby and the session is over.*/
    void broadcastSessionTerminated();

    /*Returns true if a SESSION_TERMINATED message was received this network cycle. CLIENT ONLY.*/
    bool wasSessionTerminated() const { return _sessionTerminated; }

    /*Getters for the queues and game state used during the gameplay*/
    /*Returns all the networking messages about attacks we recieved after calling getNetworkUpdate()*/
    const std::vector<AttackMessage>& getAttackUpdates() const { return attacks; }

    /*Returns all the networking messages about item passing we recieved after calling getNetworkUpdate()*/
    const std::vector<PassMessage>& getPassUpdates() const { return passes; }

    /*Returns all the networking messages about healing we recieved after calling getNetworkUpdate()*/
    const std::vector<HealMessage>& getHealUpdates() const { return heals; }

    /*Returns all the networking messages about players healing the boss we recieved after called getNetworkUpdate().*/
    const std::vector<BossHealMessage>& getBossHealUpdates() const { return bossHeals; }

    /*Returns all support effect messages received after calling getNetworkUpdate().*/
    const std::vector<SupportEffectMessage>& getSupportEffectUpdates() const { return supportEffects; }

    /*Returns all enemy effect messages received after calling getNetworkUpdate().*/
    const std::vector<EnemyEffectMessage>& getEnemyEffectUpdates() const { return enemyEffects; }

    /**Returns the number of Gaia item spawn messages we recieved after calling getNetworkUpdate()*/
    int getNumGaiaSpawns() const { return gaiaSpawns; }

    /*Returns the most recent version of the authoritative game state*/
    GameStateMessage getStateUpdate() { return _latestGameState; }

    /*Tells us if the host sent a message saying the game was lost*/
    bool checkGameLost() { return _gameLost; }

    /*Tells us if the host sent a message saying the game was won*/
    bool checkGameWon() { return _gameWon; }

    /**Functions used during the lobby scene*/

    /*Checks if the game has started. Used during the lobby scene by clients*/
    bool checkGameStarted();

    /*Sets the player username. Used in the Client and Host scenes*/
    void setPlayerName(const std::string& name);

    /*Returns the username we set*/
    std::string getPlayerName() const { return _playerName; }

    /*returns the local player's position in the circle*/
    int getLocalPlayerNumber();
    
    /*returns player's position in the circle given their networkID*/
    int getPlayerNumberByID(const std::string& networkID);

    /*returns if this numbered player is a real one or AI*/
    bool checkRealPlayer(int playerID);

    /*Returns the list of networked players, carrying their network ID and username*/
    const std::unordered_map<int, NetworkedPlayer>& getNetworkedPlayers() const { return _slotToPlayer; }
    
    /**
     * Returns true if the given houseID is already claimed by any player
     * other than the local player.
     *
     * @param houseID  The house ID to check.
     * @return         true if another player has claimed it, false otherwise.
     */
    bool isHouseTaken(const std::string& houseID) const;

    /**
     * Returns the set of houseIDs currently claimed by players other than
     * the local player. Used by HouseSelectScene to grey out unavailable cards.
     *
     * @return  A vector of taken house ID strings.
     */
    std::vector<std::string> getTakenHouses() const;
    
    /**
     * Registers a disconnect callback on the NetcodeConnection so that when
     * any peer closes, their slot is immediately pushed into _disconnectedSlots.
     * Should be called once after the network connection is established.
     */
    void registerDisconnectCallback();
    
    /**
     * Broadcasts a PLAYER_DISCONNECT message to all clients.
     *
     * @param slotIndex  The 0-based player slot that disconnected.
     */
    void broadcastPlayerDisconnected(int slotIndex);
    
    /** Returns slots that disconnected since the last clearQueues(). */
    const std::vector<int>& getDisconnectedSlots() const { return _disconnectedSlots; }
    
    /**
     * Sets the house selection for the local player (host only).
     *
     * Because sendToHost() does not loop back to the sender, the host cannot
     * receive its own SELECT_HOUSE message via the normal network path. This
     * method writes the house ID directly into the host's slot in the online
     * players list and broadcasts the updated lobby state to all clients so
     * they stay in sync.
     *
     * Should be called on the host immediately after broadcastSelectedHouse()
     * when the host locks in their house selection.
     *
     * @param houseID  The ID of the house the host selected (e.g. "athena").
     *                 Must match a valid entry in the HouseLoader.
     */
    void setLocalHouse(const std::string& houseID);
    
    /** Returns the enemy ID of the chosen boss for the game. */
    std::string getEnemy() { return _enemy; };
    
    /**
     * Returns the house ID assigned to the given AI slot from the host's
     * authoritative AI house map. Used by LobbyScene and HouseSelectScene
     * to sync AI slot house selections into GameState each frame, and by
     * updateTakenHouseCards() to grey out houses claimed by AI slots.
     *
     * Only meaningful on the host, where _aIHouses is written directly.
     * On clients, _aIHouses is populated via AI_HOUSE_SELECT messages and
     * the _aIHouses block embedded in each LOBBY_UPDATE broadcast.
     *
     * @param slotIndex  The 0-based game slot index of the AI player.
     * @return           The house ID assigned to that slot, or "" if unset.
     */
    std::string getAIHouse(int slotIndex) const;
    
    /**
     * Sets the enemy of the game using their unique Enemy ID. Should be called once after
     * the host chooses a boss.
     *
     * @param enemyID  The unique of the boss from enemies.json
     */
    void setEnemy(const std::string& enemyID) { _enemy = enemyID; }
    
    /** Returns true if every player in the lobby has selected a house. */
    bool allPlayersSelectedHouse() const;
    
    /**
     * Broadcasts the host's selected boss enemy to all connected clients.
     * Should be called by the host immediately after the player confirms
     * their boss selection in the boss select screen.
     *
     * Clients will update their local _enemy field upon receiving this
     * message, which is then read by getEnemy() to update the lobby UI.
     *
     * @param enemyID  The unique identifier of the selected enemy (e.g. "cyclops", "cerberus").
     *                 Must match a valid entry in the enemy JSON definition file.
     */
    void broadcastBossSelection(const std::string& enemyID);
    
    /**
     * Broadcasts the host's house selection for an AI slot to all clients.
     * Clients will update that slot's houseID in their local _uiudToSlot
     * list upon receiving this message.
     *
     * @param slotIndex  The 0-based AI slot index being configured.
     * @param houseID    The selected house ID, or "" to clear the selection.
     */
    void broadcastAIHouseSelection(int slotIndex, const std::string& houseID);
    
    /**
     * Clears the host's AI house assignment for the given slot.
     * Called when a real player joins a slot that was previously
     * configured as AI, so the assignment does not bleed back
     * after the player leaves.
     *
     * @param slotIndex  The 0-based slot index to clear.
     */
    void clearAIHouse(int slotIndex);
    
    /**
     * Returns true if the host dropped unexpectedly. Checks both the explicit
     * _hostDisconnected flag and polls the connection state directly each frame,
     * since CUGL's onDisconnect callback is unreliable when receive() is called
     * every frame. CLIENT ONLY — always false on the host.
     */
    bool wasHostDisconnected() const;
    
    /**
     * Broadcasts the host's current scene state to all clients every frame.
     * Clients use this to mirror the host's scene transitions, ensuring no
     * client gets left behind if they missed the original transition signal.
     *
     * @param sceneState  0 = PreGameEntryScene, 1 = GameScene
     */
    void broadcastHostsCurrentScene(int sceneState);

    /**
     * Returns the most recent scene state broadcast by the host.
     * Used by clients to detect when the host has transitioned scenes
     * and advance accordingly.
     *
     * @return  0 = PreGameEntryScene, 1 = GameScene, 2 = LobbyScene -1 = unknown (not yet received)
     */
    int getHostsCurrentScene() const { return _hostsCurrentScene; }

    /**
     * Broadcast a message to player at 
    */

protected:
    //This enum is used internally by this class to figure out how to decode the data recieved over the network
    
    //These enum types are made explicit because we send the enums over as integers, and we don't want to take any
    //chances for different compilers deciding to assign different numbers to these
    enum MessageType {
        BOSS_DAMAGE = 0,
        PLAYER_HEAL = 1,
        PLAYER_PASS = 2,
        GAME_UPDATE = 3,
        HOSTS_CURRENT_SCENE = 4,
        LOBBY_UPDATE = 5,
        PLAYER_JOIN = 6,
        SELECT_HOUSE = 7,
        PLAYER_DISCONNECT = 8,
        GAME_LOST = 9,
        GAME_WON = 10,
        SESSION_TERMINATED = 11,
        BOSS_SELECT = 12,
        AI_HOUSE_SELECT = 13,
        PLAYER_SUPPORT_EFFECT = 14,
        ENEMY_EFFECT = 15,
        BOSS_HEAL = 16,
        GAIA_SPAWN = 17
    };

    /** Our network connection */
    std::shared_ptr<cugl::netcode::NetcodeConnection> _network;

    /** Serializer and desializer that lets us make bytes more readable and easy to decode across the network */
    cugl::netcode::NetcodeSerializer _serializer;
    cugl::netcode::NetcodeDeserializer _deserializer;

    /** Keeps track of the id of the game we are in */
    std::string _gameid;

    /** Keeps track of the connection status */
    Status _status;

    /** The network configuration */
    cugl::netcode::NetcodeConfig _config;
    
private:
    /** Lists that keep track of the updates sent by players to the host */
    std::vector<AttackMessage> attacks;
    std::vector<BossHealMessage> bossHeals;
    std::vector<PassMessage> passes;
    std::vector<HealMessage> heals;
    std::vector<SupportEffectMessage> supportEffects;
    std::vector<EnemyEffectMessage> enemyEffects;

    /** Integer that keeps track of how many messages a client recieved to spawn in Gaia rocks*/
    int gaiaSpawns;

    GameStateMessage _latestGameState;
    //win/loss booleans
    bool _gameWon;
    bool _gameLost;
    
    // A vector storing the slots containing all the disconnected players that haven't been reassigned.
    std::vector<int> _disconnectedSlots;

    // The last scene state broadcast by the host. -1 = unknown, 0 = pregame, 1 = game.
    int _hostsCurrentScene = -1;

    /** Maps each player's network UUID to their fixed game slot index. */
    std::unordered_map<std::string, int> _uuidToSlot;
    
    /** Maps each game slot index to that player's networked data. */
    std::unordered_map<int, NetworkedPlayer> _slotToPlayer;
    
    // True if host sent SESSION_TERMINATED this network cycle
    bool _sessionTerminated = false;

    //Player's chosen username
    std::string _playerName;
    
    // Enemy for the game
    std::string _enemy;
    
    //Used internally to handle the different types of networking messages that come in 
    void handleMessage(const std::string& senderID, const std::vector<std::byte>& message);
    
    /** Houses chosen by the host for AI slots, keyed by game slot index */
    std::unordered_map<int, std::string> _aIHouses;
};

#endif /* __NETWORKING_CONTROLLER__ */
