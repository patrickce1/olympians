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
    std::shared_ptr<cugl::scene2::Button> _backButton;
    
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

    /** Player card nodes used for drag hit-testing and temporary movement. */
    std::vector<std::shared_ptr<cugl::scene2::SceneNode>> _playerCards;

    /** Home positions for each player card while idle. */
    std::vector<cugl::Vec2> _playerCardHomePositions;

    /** A container that stores labels and other info for visualizing the house and username choices of players */
    std::shared_ptr<cugl::scene2::SceneNode> _playerInfoContainer;
    
    /** The glowing blinker for the local player's icon(bottom icon) to notify them to pick house  */
    std::shared_ptr<cugl::scene2::SceneNode> _localPlayerIconIndicator;
    
    /** Whether the server sent a disconnect status update this frame and it has not been carried out yet*/
    bool _pendingDisconnect = false;
    
    /** The current status */
    Status _status;

    /** The timer for the blinking player icon border */
    float _blinkTimer = 0.0f;
    
    /** Whether the player icon border is visible */
    bool _blinkOn = true;
    
    /** Whether the local player has selected a house */
    bool _hasSelectedHouse;

    /** True once this activation has sent the PLAYER_JOIN message. */
    bool _sentJoinMessage = false;

    /** The state of the game */
    GameState* _gameState = nullptr;

    /** Touchscreen input device used for lobby drag interactions. */
    cugl::Touchscreen* _touch = nullptr;

    /** Mouse input device used for desktop drag interactions. */
    cugl::Mouse* _mouse = nullptr;

    /** Listener key for touch callbacks. */
    Uint32 _touchListenerKey = 0;

    /** Listener key for mouse callbacks. */
    Uint32 _mouseListenerKey = 0;

    /** True while pointer is currently held down. */
    bool _pointerDown = false;

    /** True once the current pointer interaction becomes a drag. */
    bool _isDraggingCard = false;

    /** Set when a drag occurred; used to suppress icon tap-to-select on release. */
    bool _didDragCard = false;

    /** The index of the card currently being dragged, or -1 when none. */
    int _draggedCardIndex = -1;

    /** Pointer position where current press began. */
    cugl::Vec2 _pointerStartPos;

    /** Offset from pointer to card origin at drag start. */
    cugl::Vec2 _dragOffset;

    /** True when a drag start has been armed by a playerIcon button-down event. */
    bool _pendingDragInit = false;

    /** True while two cards are visually sliding between slots after a swap drop. */
    bool _isSwapAnimating = false;

    /** Elapsed time for the current swap animation (seconds). */
    float _swapAnimElapsed = 0.0f;

    /** Duration of the visual swap animation (seconds). */
    float _swapAnimDuration = 0.14f;

    /** Display indices of the cards currently being animated. */
    int _swapAnimDisplayA = -1;
    int _swapAnimDisplayB = -1;

    /** Starting positions of both cards for the current swap animation. */
    cugl::Vec2 _swapAnimStartA = cugl::Vec2::ZERO;
    cugl::Vec2 _swapAnimStartB = cugl::Vec2::ZERO;

    /** Model indices queued to swap once the animation finishes. */
    int _pendingModelSwapA = -1;
    int _pendingModelSwapB = -1;

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

    /**
     * Initializes touch and mouse listeners used for lobby drag-and-drop.
     */
    void setupDragInput();

    /**
     * Removes any touch/mouse listeners registered by setupDragInput().
     */
    void disposeDragInput();

    /**
     * Starts a potential drag if the pointer pressed on a player card.
     *
     * @param scenePos  Pointer location in scene coordinates.
     */
    void handlePointerDown(const cugl::Vec2& scenePos);

    /**
     * Updates the currently dragged card position.
     *
     * @param scenePos  Pointer location in scene coordinates.
     */
    void handlePointerDrag(const cugl::Vec2& scenePos);

    /**
     * Ends drag handling and performs a slot swap if dropped over another card.
     *
     * @param scenePos  Pointer location in scene coordinates.
     */
    void handlePointerUp(const cugl::Vec2& scenePos);

    /**
     * Returns the card index at a scene position.
     *
     * @param scenePos  Pointer location in scene coordinates.
     * @param ignore    Card index to skip during hit-test.
     * @return          Card index, or -1 if no card is hit.
     */
    int findCardAt(const cugl::Vec2& scenePos, int ignore = -1) const;

    /**
     * Converts a display-slot index (0..N-1 in lobby UI order) to the
     * underlying model/network slot index.
     *
     * @param displayIndex  The lobby card index in display order.
     * @return              The backing model slot, or -1 if unavailable.
     */
    int displayIndexToModelIndex(int displayIndex) const;

    /**
     * Swaps two players selected by their display-slot indices.
     *
     * @param displayA  First lobby card index.
     * @param displayB  Second lobby card index.
     */
    void swapPlayersByDisplayIndex(int displayA, int displayB);

    /**
     * Starts a visual slide animation for two display slots.
     *
     * @param displayA  First display-slot index.
     * @param displayB  Second display-slot index.
     * @param modelA    First backing model index.
     * @param modelB    Second backing model index.
     */
    void beginSwapAnimation(int displayA, int displayB, int modelA, int modelB);

    /**
     * Advances any in-progress swap animation and commits model/network swap
     * once the animation reaches completion.
     *
     * @param timestep  Delta time in seconds.
     */
    void updateSwapAnimation(float timestep);
};

#endif /* __LOBBY_SCENE_H__ */
