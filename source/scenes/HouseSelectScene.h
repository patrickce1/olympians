#ifndef __HOUSE_SELECT_SCENE_H__
#define __HOUSE_SELECT_SCENE_H__

#include <cugl/cugl.h>
#include "../HouseLoader.h"
#include "../NetworkController.h"
#include "../NetworkMessage.h"
#include <iostream>
#include <sstream>
#include <vector>

/**
 * This class provides the interface to make the house select scene.
 */
class HouseSelectScene : public cugl::scene2::Scene2 {
public:
    /**
     * The configuration status
     *
     * This is how the application knows to switch to the next scene.
     */
    enum Status {
        /** Player is browsing and has not locked in a house yet */
        WAITING,
        /** Player has locked in a house; ready to proceed */
        LOCKED,
        /** Player canceled or left house select; back to lobby */
        ABORT,
        /** Game scene has been started by host*/
        PRE_GAMESCENE_START,
        /** The host has disconnected*/
        HOST_DISCONNECTED
    };
    
    /**
     * Primes the scene to perform a full UI reset on its next activation.
     * Should be called when the session has ended and the player is being
     * returned to the main menu — not during normal lobby navigation.
     *
     * @param reset  true to schedule a reset on the next setActive(true) call.
     */
    void setPendingReset(bool reset) { _pendingReset = reset; }
    
protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;
    
    /** The network controller shared across all scenes*/
    std::shared_ptr<NetworkController> _network;

    /** The button for locking/unlocking chosen house */
    std::shared_ptr<cugl::scene2::Button> _lockButton;
    
    /** The back button for the houseSelect scene */
    std::shared_ptr<cugl::scene2::Button> _backButton;
    
    /** The player icon (for updating) */
    std::shared_ptr<cugl::scene2::SceneNode> _playerIcon;
    
    /** The image node inside the left teammate's icon diamond in the house select screen. */
    std::shared_ptr<cugl::scene2::PolygonNode> _leftPlayerIcon;

    /** The image node inside the right teammate's icon diamond in the house select screen. */
    std::shared_ptr<cugl::scene2::PolygonNode> _rightPlayerIcon;

    /** The image node inside the top teammate's icon diamond in the house select screen. */
    std::shared_ptr<cugl::scene2::PolygonNode> _upPlayerIcon;
    
    /** The player icon image (for updating) */
    std::shared_ptr<cugl::scene2::PolygonNode> _playerIconImage;
    
    /** The player icon glow state (for toggle) */
    std::shared_ptr<cugl::scene2::SceneNode> _playerIconGlow;
    
    /** Whether the played has locked down a house.*/
    bool _locked = false;
    
    /**
     * Whether the scene should perform a full UI reset on its next activation.
     *
     * Set to true via setPendingReset() when the local player is returned to
     * the main menu due to session termination — either because the host backed
     * out of the lobby or because the host disconnected while the client was
     * in this scene. When false, setActive(true) preserves the player's current
     * carousel position and lock state, allowing seamless navigation back and
     * forth between the lobby and house select during an active session.
     *
     * Automatically reset to false after the reset fires in setActive(true).
     */
    bool _pendingReset = false;
    
    /** The house selection node list */
    std::vector<std::shared_ptr<cugl::scene2::SceneNode>> _houseCards;
    
    /** The house selection indicator list */
    std::vector<std::shared_ptr<cugl::scene2::SceneNode>> _houseCarouselDotIndicators;
    
    /** The current index of the god shown in the house selection screen*/
    int _currentIndex = 4;
    
    /** The house selection navigation left button */
    std::shared_ptr<cugl::scene2::Button> _leftButton;
    
    /** The house selection navigation right button */
    std::shared_ptr<cugl::scene2::Button> _rightButton;
    
    /** The house selection container that contains the card and direction buttons**/
    std::shared_ptr<cugl::scene2::SceneNode> _houseSelectionCardContainer;
    
    /** The background boss image of the selection screen */
    std::shared_ptr<cugl::scene2::PolygonNode> _backgroundImage;
    
    /** The ID of the current boss/enemy */
    std::string _currentBoss = "";
    
    /** Whether the house selection screen is sliding to another index */
    bool _isAnimating = false;
    
    /** How long sliding to new house index takes */
    float _slideDuration = 0.3f;
    
    /** The vector target position of the selection container */
    cugl::Vec2 _slideTarget = cugl::Vec2();
    
    /** The current status */
    Status _status;
    
    /** Loads house definitions from JSON for house selection. */
    HouseLoader _houseLoader;
    
    /**The state of the game**/
    GameState* _gameState = nullptr;
    
    /**
     * The game slot this house select instance is currently configuring.
     * -1 means the local player's own slot (normal mode).
     * Any other value means the host is selecting on behalf of an AI slot.
     */
    int _targetSlot = -1;
    
    /**
     * Persisted UI state for a single house-select slot. Stored between
     * activations so the carousel position and lock state are restored
     * when the host or player reopens house select for that slot.
     */
    struct SlotState {
        int  carouselIndex = 4;   // which card was showing
        bool locked        = false;
    };

    /** Per-slot persisted state, keyed by game slot index. -1 = local player. */
    std::unordered_map<int, SlotState> _slotStates;

public:
#pragma mark -
#pragma mark Constructors
    /**
     * Creates a new house select scene with the default values.
     *
     * This constructor does not allocate any objects or start the game.
     * This allows us to use the object without a heap pointer.
     */
    HouseSelectScene() : cugl::scene2::Scene2() {}
    
    /**
     * Disposes of all (non-static) resources allocated to this mode.
     *
     * This method is different from dispose() in that it ALSO shuts off any
     * static resources, like the input controller.
     */
    ~HouseSelectScene() { dispose(); }
    
    /**
     * Disposes of all (non-static) resources allocated to this mode.
     */
    void dispose() override;
    
    /**
     * Initializes the house selection scene.
     *
     * This method sets up all UI elements, binds necessary callbacks,
     * and stores references to shared resources such as the asset manager
     * and network controller. It prepares the scene for use but does not
     * make it active or responsive to input.
     *
     * Activation and input handling are controlled separately via setActive().
     *
     * @param assets                           The loaded asset manager used to retrieve scene resources
     * @param networkController   The network controller used for multiplayer communication
     * @param gameState                     The state of the game
     *
     * @return true if the scene was successfully initialized; false otherwise
     */
    bool init(const std::shared_ptr<cugl::AssetManager>& assets,
                                const std::shared_ptr<NetworkController>& networkController,
                                GameState* gameState);
    
    /**
     * Retrieves and stores references to the house select UI elements.
     *
     * This method looks up UI components from the scene graph including the
     * start button, back button, host name text field, carousel navigation
     * buttons, and the role carousel container. It also initializes the
     * carousel item list and configures the placeholder label.
     */
    void setupUI();
    
    /**
     * Attaches input listeners to the house select buttons.
     *
     * This method assigns callbacks for starting the game, returning to the
     * previous menu, and navigating the role selection carousel.
     */
    void setupListeners();
    
    /**
     * Sets whether the scene is currently active
     *
     * This method should be used to toggle all the UI elements.  Buttons
     * should be activated when it is made active and deactivated when
     * it is not.
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
    
    /**
     * Sets the game slot this scene should configure on its next activation.
     * Pass -1 to configure the local player's own slot (default behaviour).
     * Pass an AI slot index to let the host select on behalf of that AI player.
     * Must be called before setActive(true).
     *
     * @param slot  The 0-based game slot index, or -1 for the local player.
     */
    void setTargetSlot(int slot) { _targetSlot = slot; }
    
    /**
     * Commits a house lock for the current carousel selection. Writes the
     * chosen house to the correct slot in GameState and broadcasts it over
     * the network. If _targetSlot is -1, writes to the local player's slot;
     * otherwise writes to the AI slot the host is configuring.
     *
     * @param selectedHouse  The house definition the player locked in.
     */
    void commitHouseLock(const HouseLoader::HouseDef& selectedHouse);

    /**
     * Clears the house selection for the current target slot and broadcasts
     * the change. Only has an effect in AI slot mode (_targetSlot != -1).
     */
    void commitHouseUnlock();
    
    /**
     * Updates the teammate icon diamond for _targetSlot with the house at
     * the given carousel index. Called during AI slot mode so the host can
     * preview the selection without modifying their own icon.
     *
     * @param currentIndex  The carousel index whose house to preview.
     */
    void updateAIPreviewIcon(int currentIndex);
    
    /**
     * Returns the carousel index for the given slot when the scene opens.
     * If the player in that slot has a house selected, returns the index of
     * that house so the carousel always opens facing their current selection.
     * Falls back to the saved carousel state if they have no house yet.
     *
     * @param targetSlot  The slot to open (-1 for the local player, or a
     *                    0-based AI slot index).
     * @return            The carousel index to slide to on activation.
     */
    int getInitialCarouselIndex(int targetSlot);

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
     * Reconfigures the lock button for this scene
     *
     * This is necessary because what the buttons do depends on the state the player's choice
     */
    void configureLockButton();
    
    /**
     * Initiates a slide animation to center the item at `newIndex`.
     *
     * Does nothing if an animation is already in progress or if the
     * index is out of bounds. Otherwise computes the target container
     * position and stores it in `_slideTarget`.
     *
     * @param newIndex The index of the item to slide to.
     */
    void slideTo(int index);
    
    /**
     * Updates the circular carousel indicators at the bottom of what card in the carousel
     * we are currently at.
     *
     * @param currentIndex The index of the card we are at.
     */
    void updateCarouselDots(int newIndex);
    
    /**
     * Updates the local player's icon in the diamond based on the house card
     * they are currently on. If commitToGameState is true, also updates the
     * local player's house in GameState — should only be true when the player
     * locks in their selection.
     *
     * @param currentIndex                The index of the card we are at.
     * @param commitToGameState     Whether to write the house selection to GameState.
     */
    void updateSelectedIcon(int currentIndex, bool commitToGameState = false);
    
    /**
     * Updates the background image of the boss display based on the selected enemy.
     *
     * @param enemyID The identifier of the enemy whose background should be displayed.
     */
    void updateBossBGImage(std::string enemyID);
    
    /** Loads houses definitions from the house JSON to use in house selection. */
    bool loadHouses();
    
    /**
     * Syncs _gameState player names and house selections with the current
     * networked player list. Called every frame during house selection so
     * teammate icons stay up to date as other players lock in their houses.
     */
    void updateNetworkOrder();
    
    /**
     * Updates the three teammate icon images using the same circular remapping
     * as LobbyScene, so each neighbour slot always reflects the correct player
     * relative to the local player. Called every frame in update().
     */
    void updateTeammateIcons();
    
    /**
     * Greys out any house cards that have already been claimed by another
     * player. Called every frame in update() so the visual stays in sync
     * as other players lock in their selections.
     */
    void updateTakenHouseCards();
    
    /**
     * Refreshes the local player's icon diamond to reflect their actual
     * committed house selection when the scene activates. Prevents a stale
     * carousel preview texture from persisting across activations.
     *
     * Called unconditionally in setActive(true) before slideTo(), so the
     * icon always shows the last locked house rather than whatever carousel
     * position was showing when the scene was last deactivated.
     */
    void refreshLocalPlayerIcon();
    
    /**
     * Returns true if the local player has a house selected in the network.
     */
    bool hasLocalPlayerSelectedHouse() const;
    
};

#endif /* __HOUSE_SELECT_SCENE_H__ */
