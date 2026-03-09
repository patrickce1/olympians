//Networking class
#ifndef __NETWORKING_CONTROLLER__
#define __NETWORKING_CONTROLLER__

#include <cugl/cugl.h>
#include "scenes/GameState.h"

/*
The networking controller creates an abstraction for sending messages over the network.

It handles sending and recieving bytes of data and decoding them into easy to read structs. 

It does not handle actually updating the state of the game. It is up to the individual scenes to update themselves based on
the messages that they extract from the NetworkController.
*/

class NetworkController {
public:
    /*
    BELOW IS THE SPECIFICATION FOR HOW DATA IS ORGANIZED BASED ON MESSAGE TYPE
    BOSS_DAMAGE
        This type of message includes data about what damage was dealt to the boss.
        The data payload for this is just an integer representing how much damage was done to the boss.
    PLAYER_HEAL
        This type of message includes data about what player was healed and for how much health
    PLAYER_PASS
        This type of message includes data about what player was sent
    GAME_UPDATE
        This is the biggest type of message sent over the network. The host sends out this message to tell players of the new world state.
        How this payload
    */
    /*
    These are the types of messages we send/recieve when in the lobby
    */
    struct JoinMessage {
        std::string playerName;
    };

    /*
    Types of messages we send/recieve when fighting the boss
    */
    struct AttackMessage {
        float damage;
    };

    struct HealMessage {
        float heal;
        int playerID;
    };

    struct PassMessage {
        std::string itemID;
        int playerID;
    };

    struct GameStateMessage {
        //boss health
        float bossHealth;
        int bossDirection;

        //player health
        float player1HP;
        float player2HP;
        float player3HP;
        float player4HP;

        //future info will be added as the game expands
    };

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

    /**
      Basic functions.
     */
    Status checkConnection();
    void getNetworkUpdates();
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


    /* Atomic style update functions. The following are ONLY SEND TO THE HOST*/

    void broadcastDamage(float damageAmount);

    void broadcastPass(const std::string& itemDefID, int playerID);

    void broadcastHeal(float healAmount, const std::string& playerID);

    /*Client-Side Lobby Messages*/
    /*Sends player username to the host*/
    void broadcastJoinedLobby();

    /**/
    void broadcastGameStart();

    void broadcastLobbyState();

    const std::vector<AttackMessage>& getAttackUpdates() const { return attacks; }
    const std::vector<PassMessage>& getPassUpdates() const { return passes; }
    const std::vector<HealMessage>& getHealUpdates() const { return heals; }

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

    /*Returns any updates on player order. Each networked player is represented by NetworkID, Username*/
    const std::vector<std::pair<std::string, std::string>> checkLobbyOrder();

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

	std::shared_ptr<cugl::netcode::NetcodeConnection> _network;
    cugl::netcode::NetcodeSerializer _serializer;
    cugl::netcode::NetcodeDeserializer _deserializer;

    std::string _gameid;
    Status _status;

    /** The network configuration */
    cugl::netcode::NetcodeConfig _config;
    
private:
    /*Lists that keep track of the updates sent by players to the host*/
    std::vector<AttackMessage> attacks;
    std::vector<PassMessage> passes;
    std::vector<HealMessage> heals;

    //Used for sending a message
    bool gameStarted;

    //stores the most recent order that we got. The host's version of this is authoritative
    std::vector<std::pair<std::string, std::string>> _onlinePlayers;

    //Player's chosen username
    std::string _playerName;
    
    //
    void handleMessage(const std::string& senderID, const std::vector<std::byte>& message);
};

#endif /* __NETWORKING_CONTROLLER__ */