//Networking class
#ifndef __NETWORKING_CONTROLLER__
#define __NETWORKING_CONTROLLER__

#include <cugl/cugl.h>

class NetworkController {
public:

	enum Status {
        FAILED,
        WAITING,
        CONNECTED
        //more to come as networking expands, such as START or ONGOING
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
      * Checks what the status of the connection is currently. 
      * Check the Status enum to check what each returned value means
     */
    Status checkConnection();

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

    /*Need to add some functions related to getting info about players for waiting in the lobby.
      However, I can't figure the exact nature of the info we need to send until I get Danielle's pr
    */

    /*Because the original host can disconnect, this is used to keep track of host migration*/
    bool isHost();

    /* Outline for future atomic update-style functions */
    //void broadcastDamange(float damange);
    //void passItem(Item item, Player player);
    //void broadcastHeal(float heal, Player player);
    //GameState recieveUpdate();

    


protected:
	std::shared_ptr<cugl::netcode::NetcodeConnection> _network;
    std::string _gameid;
    Status _status;

    /** The network configuration */
    cugl::netcode::NetcodeConfig _config;


private:
	//nothing for now
};


#endif /* __NETWORKING_CONTROLLER__ */