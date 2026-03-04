//Networking class
#ifndef __NETWORKING_CONTROLLER__
#define __NETWORKING_CONTROLLER__

#include <cugl/cugl.h>

class NetworkController {
public:

	enum Status {
        //idk
		IDLE,

        //Waiting on connection
        JOIN,

        //Game is starting
        WAIT,

        //Game got aborted
        START,

        //idk
        ABORT
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
    * Connects to the game server as specified in the assets file
    *
    * The {@link #init} method set the configuration data. This method simply uses
    * this to create a new {@Link NetworkConnection}. It also immediately calls
    * {@link #checkConnection} to determine the scene state.
    *
    * @param room  The room ID to use
    *
    * @return true if the connection was successful
    */
    bool joinRoom(const std::string room);

    /*Creates a lobby for other players to join, 
    IMPORTANT: the program could be still negotiating connection
    by the end of the function, so don't assume the lobby is created after running hostRoom()*/
    void hostRoom();

    /*Returns a string with the room id if the room exists. Nullptr otherwise*/
    std::string getRoom();

    /*Disconnects the player. If it's the host, moves the host*/
    void disconnect();

    /*TODO: Add more functions that relate to sending updates out
    We need to figure out as a team how we want to represent updates
    to game state across players. One could be more atomic updated like
    sendDamage(), another would be as a big batch to send out an updated
    state of the world according to our view
    */

protected:
	std::shared_ptr<cugl::netcode::NetcodeConnection> _network;
    std::string _gameid;
    Status _status;

    /** The network configuration */
    cugl::netcode::NetcodeConfig _config;


private:
	
};


#endif /* __NETWORKING_CONTROLLER__ */