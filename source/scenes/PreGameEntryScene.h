#ifndef __PRE_GAME_ENTRY_SCENE_H__
#define __PRE_GAME_ENTRY_SCENE_H__

#include <cugl/cugl.h>
#include <iostream>
#include <sstream>
#include "../NetworkController.h"
#include "../NetworkMessage.h"

/**
 * This class provides the interface to make the pre game entry scene.
 */
class PreGameEntryScene : public cugl::scene2::Scene2 {
public:
    /**
     * The configuration status
     *
     * This is how the application knows to switch to the next scene.
     */
    enum Status {
        IDLE,
        START,
        ERROR_DISPLAY,
        ABORT,
        PLAYER_DISCONNECTED,
        HOST_DISCONNECTED
    };
    
protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;

    /** The network controller shared across all scenes */
    std::shared_ptr<NetworkController> _network;
    
    /** Action Timeline for node animations e.g clouds */
    std::shared_ptr<cugl::ActionTimeline> _timeline;
    
    /** The top-right cloud image to be eased in */
    std::shared_ptr<cugl::scene2::SceneNode> _topClouds;
    
    /** The center-bottom cloud image to be eased in*/
    std::shared_ptr<cugl::scene2::SceneNode> _bottomClouds;
    
    /** The original Position of the top cloud */
    cugl::Vec2 _topCloudPos;
    
    /** The original Position of the bottom cloud */
    cugl::Vec2 _bottomCloudPos;

    /** The loading bar for entering a game */
    std::shared_ptr<cugl::scene2::ProgressBar> _loadingBar;
    
    /** Player usernames  */
    std::vector<std::shared_ptr<cugl::scene2::Label>> _playerNames;
    
    /** House names  */
    std::vector<std::shared_ptr<cugl::scene2::Label>> _houseNames;
    
    /** Player tiles that show the houses that they'll be playing under*/
    std::vector<std::shared_ptr<cugl::scene2::PolygonNode>> _playerTiles;
    
    /** The current status */
    Status _status;
    
    /** Progress fo the loading bar */
    float _loadingProgress = 0.0f;

    /** Real progress reported by incremental game-resource preparation. */
    float _loadingTarget = 0.0f;
    bool _loadingComplete = false;

    /** The state of the game */
    GameState* _gameState = nullptr;
    
    /** Seconds elapsed since the error popup was shown. */
    float _errorTimer;
    
    /** Optional error-popup node (may be nullptr if absent from JSON scene). */
    std::shared_ptr<cugl::scene2::SceneNode> _errorPopup;
    
    /** Player name set when a disconnect is detected during the countdown.
     *  SceneLoader reads this to show a lobby banner before switching scenes. */
    std::string _disconnectMessage;

public:
#pragma mark -
#pragma mark Constructors
    /**
     * Creates a new pre game entry scene with the default values.
     *
     * This constructor does not allocate any objects or start the game.
     * This allows us to use the object without a heap pointer.
     */
    PreGameEntryScene() : cugl::scene2::Scene2() {}
    
    /**
     * Disposes of all (non-static) resources allocated to this mode.
     *
     * This method is different from dispose() in that it ALSO shuts off any
     * static resources, like the input controller.
     */
    ~PreGameEntryScene() { dispose(); }
    
    /**
     * Disposes of all (non-static) resources allocated to this mode.
     */
    void dispose() override;
    
    /**
     * Initializes the scene contents, and starts the scene
     *
     * In previous labs, this method "started" the scene.  But in this
     * case, we only use to initialize the scene user interface.  We
     * do not activate the user interface yet, as an active user
     * interface will still receive input EVEN WHEN IT IS HIDDEN.
     *
     * That is why we have the method {@link #setActive}.
     *
     * @param assets             The (loaded) assets for this game mode
     * @param networkController  The network controller shared across all scenes
     * @param gameState          The state of the game
     *
     * @return true if the controller is initialized properly, false otherwise.
     */
    bool init(const std::shared_ptr<cugl::AssetManager>& assets,
              const std::shared_ptr<NetworkController>& networkController, GameState* gameState);
    
    /**
     * Retrieves and stores references to the pre game entry scene UI elements.
     *
     * This method looks up important UI components from the scene graph,
     * including the loading bar, player labels, and house tiles, and storing
     * them for later interaction.
     */
    void setupUI();

    /**
     * Sets whether the scene is currently active
     *
     * This method should be used to toggle all the UI elements.
     *
     * @param value whether the scene is currently active
     */
    virtual void setActive(bool value) override;

    /**
     * Returns the scene status.
     *
     * Any value other than IDLE and LOADING will transition to a new scene.
     *
     * @return the scene status
     *
     */
    Status getStatus() const { return _status; }

    /**
     * Returns true once the entry animation has settled and it is safe to do
     * the synchronous game-scene preparation behind this scene.
     */
    bool isReadyToLoadGame() const {
        return _status == Status::IDLE && _timeline &&
               !_timeline->isActive("bottom_clouds");
    }

    /** Updates the real asset-loading percentage reported by GameScene. */
    void setLoadingProgress(float progress);
    
    /**
     * The method called to update the scene.
     *
     * @param timestep  The amount of time (in seconds) since the last frame
     */
    void update(float timestep) override;
    
    /** Returns the name of the player who disconnected (empty if none). */
    const std::string& getDisconnectMessage() const { return _disconnectMessage; }
    
    /**
     * Syncs the latest network state into GameState and detects player disconnects.
     * For each slot: if still a real player, updates their username and house; if
     * it was a real player but is no longer connected, stores their name in
     * _disconnectMessage, sets status to PLAYER_DISCONNECTED, and returns early so
     * SceneLoader can route everyone back to the lobby; if it was always an AI,
     * updates its house assignment from the network's authoritative AI house map.
     *
     * Called every frame so that clients who arrived from HouseSelectScene or
     * BossSelectScene (which do not run this sync) are caught up before
     * GameScene starts.
     */
    void updateNetworkOrder();

private:
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
     * Updates the username and house labels in the pre game entry UI to match the given player list.
     * The list is expected to already be in display order (local player last)
     * as produced by remapPlayersForDisplay().
     *
     * @param players  The display-ordered list of players to read names from.
     */
    void updateEntryScreenText(std::vector<Player*> players);

    /**
     * Updates the player tile images in the pre game entry UI based on each player's
     * selected house. The list is expected to already be in display order
     * (local player last) as produced by remapPlayersForDisplay().
     *
     * @param players  The display-ordered list of players to read house names from.
     */
    void updateEntryScreenTiles(std::vector<Player*> players);
    
    /**
     * Animates the entry clouds from off-screen positions into their final
     * layout positions using the scene's ActionTimeline system.
     *
     * Both the top and bottom cloud layers are first positioned outside
     * the visible screen bounds in `setActive()`, then smoothly transitioned
     * into their target positions using easing-based MoveTo actions.
     */
    void animateCloudsIn();
 
    /**
     * Displays the error popup with the given message and enters ERROR_DISPLAY.
     * @param message  Human-readable error text.
     */
    void showError(const std::string& message);
 
    /** Hides the error popup and resets to IDLE. */
    void dismissError();
};

#endif /* __PRE_GAME_ENTRY_SCENE_H__ */
