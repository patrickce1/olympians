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
      * Initializes the controller contents, and starts the game
      *
      * In previous labs, this method "started" the scene.  But in this
      * case, we only use to initialize the scene user interface.  We
      * do not activate the user interface yet, as an active user
      * interface will still receive input EVEN WHEN IT IS HIDDEN.
      *
      * That is why we have the method {@link #setActive}.
      *
      * @param assets    The (loaded) assets for this game mode
      *
      * @return true if the controller is initialized properly, false otherwise.
     */
    bool init(const std::shared_ptr<cugl::AssetManager>& assets);

    /*Returns the current state of the connection. Check the Status enum for possible values*/
    Status checkConnection();

    /*Tells the network controller to 
    Calling this function will populate the message queues and variables with new information*/
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
    * @param room  The room ID to use
    *
    * To check the status of the connection, use {@link #checkConnection()}
    */
    void joinRoom(const std::string room);

    /*Creates a lobby for other players to join, 
    IMPORTANT: the program could be still negotiating connection
    by the end of the function. Use {@link #checkConnection()} to ensure status of connection*/
    void hostRoom();

    /*Returns a string with the room id if the room exists. Nullptr otherwise*/
    std::string getRoom();

    /*Disconnects the player. If it's the host, moves the host*/
    void disconnect();

    /*Because the original host can disconnect, this is used to keep track of host migration*/
    bool isHost();

    /* Atomic style update functions. The following are ONLY SENT TO THE HOST*/
    /*Tells the host that the boss has been damaged for damageAmount*/
    void broadcastDamage(float damageAmount);

    /*Sends a message to the corresponding player that item with the definition itemDefID has been passed to them.
    * If sent to an AI player, the host handles it, otherwise, handled by the player on their end.
    * The playerID tells us which numbered player they are in the cicle*/
    void broadcastPass(const std::string& itemDefID, int playerID);

    /*Sends a message to the host that the player located at playerID in the cicle got healed for healAmount.*/
    void broadcastHeal(float healAmount, int playerID);

    /*The following are USED ONLY BY THE HOST*/
    /*Send the GameState state as the new authoritative version of the game to all players*/
    void broadcastGameState(const GameState& state);

    /*Client-Side Lobby Messages*/
    /*Sends player username to the host*/
    void broadcastJoinedLobby();

    /*Host-Side Lobby Messages*/
    /*Notifies all clients that the game has started*/
    void broadcastGameStart();

    /*Sends an update notifying players about changes to the lobby (new players joining/leaving)*/
    void broadcastLobbyState();

    /*Getters for the queues and game state used during the gameplay*/
    const std::vector<AttackMessage>& getAttackUpdates() const { return attacks; }
    const std::vector<PassMessage>& getPassUpdates() const { return passes; }
    const std::vector<HealMessage>& getHealUpdates() const { return heals; }
    GameStateMessage getStateUpdate() { return _latestGameState; }

    /**Functions used during the lobby scene*/

    /*Checks if the game has started. Used during the lobby scene by clients*/
    bool checkGameStarted();

    /*Sets the player username. Used in the Client and Host scenes*/
    void setPlayerName(const std::string& name);

    /*Returns the username we set*/
    std::string getPlayerName() const { return _playerName; }

    /*returns the local player's position in the circle*/
    int getLocalPlayerNumber();

    /*returns if this numbered player is a real one or AI*/
    bool checkRealPlayer(int playerID);

    /*Returns the list of networked players, carrying their network ID and username*/
    const std::vector<NetworkedPlayer> getNetworkedPlayers();

protected:
    //This enum is used internally by this class to figure out how to decode the data recieved over the network
    
    //These enum types are made explicit because we send the enums over as integers, and we don't want to take any
    //chances for different compilers deciding to assign different numbers to these
    enum MessageType {
        BOSS_DAMAGE = 0,
        PLAYER_HEAL = 1,
        PLAYER_PASS = 2,
        GAME_UPDATE = 3,
        GAME_START = 4,
        LOBBY_UPDATE = 5,
        PLAYER_JOIN = 6,
    };

    /*Our network connection*/
	std::shared_ptr<cugl::netcode::NetcodeConnection> _network;

    /*Serializer and desializer that lets us make bytes more readable and easy to decode across the network*/
    cugl::netcode::NetcodeSerializer _serializer;
    cugl::netcode::NetcodeDeserializer _deserializer;

    /*Keeps track of the id of the game we are in*/
    std::string _gameid;

    /*Keeps track of the connection status*/
    Status _status;

    /** The network configuration */
    cugl::netcode::NetcodeConfig _config;
    
private:
    /* Lists that keep track of the updates sent by players to the host */
    std::vector<AttackMessage> attacks;
    std::vector<PassMessage> passes;
    std::vector<HealMessage> heals;
    GameStateMessage _latestGameState;

    //Boolean that tells us if the game has been started by the host
    bool gameStarted;

    //Stores the most recent player order that we got. The host's version of this is authoritative
    std::vector<NetworkedPlayer> _onlinePlayers;

    //Player's chosen username
    std::string _playerName;
    
    //Used internally to handle the different types of networking messages that come in 
    void handleMessage(const std::string& senderID, const std::vector<std::byte>& message);
};

#endif /* __NETWORKING_CONTROLLER__ */