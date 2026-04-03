#ifndef __LOBBY_SCENE_H__
#define __LOBBY_SCENE_H__

#include <cugl/cugl.h>
#include <iostream>
#include <sstream>
#include "../NetworkController.h"
#include "../NetworkMessage.h"

/**
 * This class provides the interface to join an existing game.
 *
 * Most games have a since "matching" scene whose purpose is to initialize the
 * network controller.  We have separate the host from the client to make the
 * code a little more clear.
 */
class LobbyScene : public cugl::scene2::Scene2 {
public:
    /**
     * The configuration status
     *
     * This is how the application knows to switch to the next scene.
     */
    enum Status {
        IDLE,
        WAIT,
        SELECT,
        BOSSSELECT,
        START,
        ABORT
    };
    
protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;

    /** The network controller shared across all scenes*/
    std::shared_ptr<NetworkController> _network;

    /** The button for entering a game */
    std::shared_ptr<cugl::scene2::Button> _enterGame;
    
    /** The back button for the menu scene */
    std::shared_ptr<cugl::scene2::Button> _backOut;
    
    /** The game id label */
    std::shared_ptr<cugl::scene2::Label> _gameId;
    
    /** Current boss id */
    std::string _currentBoss = "";
    
    /** Circular boss image */
    std::shared_ptr<cugl::scene2::PolygonNode> _bossImage;
    
    /** Circular boss image button to go to boss select scene */
    std::shared_ptr<cugl::scene2::Button> _bossLobbyButton;
    
    /** Player usernames (to update when they join) */
    std::vector<std::shared_ptr<cugl::scene2::Label>> _playerSlots;
    
    /** Player icon buttons (to update when they select house) */
    std::vector<std::shared_ptr<cugl::scene2::Button>> _playerImages;

    /** A container that stores labels and other info for visualizing the house and username choices of players */
    std::shared_ptr<cugl::scene2::SceneNode> _playerInfoContainer;
    
    /** The glowing blinker for the local player's icon(bottom icon) to notify them to pick house  */
    std::shared_ptr<cugl::scene2::SceneNode> _localPlayerIconIndicator;
    
    /** The current status */
    Status _status;

    /** The timer for the blinking player icon border */
    float _blinkTimer = 0.0f;
    
    /** Whether the player icon border is visible */
    bool _blinkOn = true;
    
    /** Whether the local player has selected a house */
    bool _hasSelectedHouse;

    /** The state of the game */
    GameState* _gameState = nullptr;

public:
#pragma mark -
#pragma mark Constructors
    /**
     * Creates a new client scene with the default values.
     *
     * This constructor does not allocate any objects or start the game.
     * This allows us to use the object without a heap pointer.
     */
    LobbyScene() : cugl::scene2::Scene2() {}
    
    /**
     * Disposes of all (non-static) resources allocated to this mode.
     *
     * This method is different from dispose() in that it ALSO shuts off any
     * static resources, like the input controller.
     */
    ~LobbyScene() { dispose(); }
    
    /**
     * Disposes of all (non-static) resources allocated to this mode.
     */
    void dispose() override;
    
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
     * @param assets                             The (loaded) assets for this game mode
     * @param networkController     The network controller shared across all scenes
     * @param gameState                       The state of the game
     *
     * @return true if the controller is initialized properly, false otherwise.
     */
    bool init(const std::shared_ptr<cugl::AssetManager>& assets,
              const std::shared_ptr<NetworkController>& networkController,
              GameState* gameState);
    
    /**
     * Retrieves and stores references to the lobby UI elements.
     *
     * This method looks up important UI components from the scene graph,
     * including the start button, back button, and game ID label, and stores
     * them for later interaction.
     */
    void setupUI();
    
    /**
     * Attaches input listeners to the lobby UI buttons.
     *
     * This method assigns button callbacks that update the lobby scene status
     * when the user presses the start or back buttons.
     */
    void setupListeners();

    /**
     * Sets whether the scene is currently active
     *
     * This method should be used to toggle all the UI elements.  Buttons
     * should be activated when it is made active and deactivated when
     * it is not.
     * 
     * It also resets the status to IDLE when value == true, to indicate we are back in the lobby scene
     *
     * @param value whether the scene is currently active
     */
    virtual void setActive(bool value) override;

    /**
     * Returns the scene status.
     *
     * Any value other than WAIT will transition to a new scene.
     *
     * @return the scene status
     *
     */
    Status getStatus() const { return _status; }
    
    /**
     * The method called to update the scene.
     *
     * We need to update this method to constantly talk to the server
     *
     * @param timestep  The amount of time (in seconds) since the last frame
     */
    void update(float timestep) override;

private:
    /**
     * Updates the text in the given button.
     *
     * Techincally a button does not contain text. A button is simply a scene graph
     * node with one child for the up state and another for the down state. So to
     * change the text in one of our buttons, we have to descend the scene graph.
     * This method simplifies this process for you.
     *
     * @param button    The button to modify
     * @param text      The new text value
     */
    void updateText(const std::shared_ptr<cugl::scene2::Button>& button, const std::string text);
    
    /**
     * Updates the image of the boss circle based on the selected enemy.
     *
     * @param enemyID The identifier of the enemy whose background should be displayed.
     */
    void updateLobbyBossImage(std::string enemyID);
    
    /**
     * Syncs _gameState player names and house selections with the current
     * networked player list. Called every frame during the lobby.
     */
    void updateNetworkOrder();
    
    /**
     Updates the _selectedHouse variable if the local player has selected a house in the
     house select screen.
     */
    void updateLocalPlayerSelectedHouse();

    /**
     * Remaps the full player list from GameState so the local player always
     * appears last (bottom slot of the UI). Walks the circular player array
     * starting one step to the right of the local player, so that left/right
     * neighbour relationships are preserved visually. Includes both real and
     * AI players since both are stored in GameState.
     *
     * This is purely a display remapping — no game or network state is changed.
     *
     * @return  A reordered list of raw Player pointers with the local player last.
     */
    std::vector<Player*> remapPlayersForDisplay();

    /**
     * Updates the username labels in the lobby UI to match the given player list.
     * The list is expected to already be in display order (local player last)
     * as produced by remapPlayersForDisplay().
     *
     * @param players  The display-ordered list of players to read names from.
     */
    void updateLobbyText(std::vector<Player*> players);

    /**
     * Updates the player icon images in the lobby UI based on each player's
     * selected house. The list is expected to already be in display order
     * (local player last) as produced by remapPlayersForDisplay().
     *
     * @param players  The display-ordered list of players to read house names from.
     */
    void updateLobbyPlayerIcons(std::vector<Player*> players);
};

#endif /* __LOBBY_SCENE_H__ */
