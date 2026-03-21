#ifndef __HOUSE_SELECT_SCENE_H__
#define __HOUSE_SELECT_SCENE_H__

#include <cugl/cugl.h>
#include "../House.h"
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
        ABORT
    };
    
protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;
    
    /** The network controller shared across all scenes*/
    std::shared_ptr<NetworkController> _network;

    /** The button for locking/unlocking chosen house */
    std::shared_ptr<cugl::scene2::Button> _lockButton;
    
    /** The back button for the houseSelect scene */
    std::shared_ptr<cugl::scene2::Button> _backOut;
    
    /** The player icon (for updating) */
    std::shared_ptr<cugl::scene2::SceneNode> _playerIcon;
    
    /** The player icon image (for updating) */
    std::shared_ptr<cugl::scene2::PolygonNode> _playerIconImage;
    
    /** The player icon glow state (for toggle) */
    std::shared_ptr<cugl::scene2::SceneNode> _playerIconGlow;
    
    /** Whether the played has locked down a house.*/
    bool _locked = false;
    
    /** The house selection node list */
    std::vector<std::shared_ptr<cugl::scene2::SceneNode>> _items;
    
    /** The house selection indicator list */
    std::vector<std::shared_ptr<cugl::scene2::SceneNode>> _indicators;
    
    /** The current index of the god shown in the house selection screen*/
    int _currentIndex = 4;
    
    /** The house selection navigation left button */
    std::shared_ptr<cugl::scene2::Button> _leftButton;
    
    /** The house selection navigation right button */
    std::shared_ptr<cugl::scene2::Button> _rightButton;
    
    /** The house selection container **/
    std::shared_ptr<cugl::scene2::SceneNode> _container; // holds items
    
    /** Whether the house selection screen is sliding to another index */
    bool _isAnimating = false;
    
    /** How long sliding to new house index takes */
    float _slideDuration = 0.3f;
    
    /** The vector target position of the selection container */
    cugl::Vec2 _slideTarget = cugl::Vec2();
    
    /** The current status */
    Status _status;

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
     * @param assets              The loaded asset manager used to retrieve scene resources
     * @param networkController   The network controller used for multiplayer communication
     *
     * @return true if the scene was successfully initialized; false otherwise
     */
    bool init(const std::shared_ptr<cugl::AssetManager>& assets,
              const std::shared_ptr<NetworkController>& networkController);
    
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
     * Updates the player's respective icon in the diamond based on the house card
     * they are currently on. If the player has locked their house, there is no change.
     *
     * @param currentIndex The index of the card we are at.
     */
    void updateSelectedIcon(int newIndex);
};

#endif /* __HOUSE_SELECT_SCENE_H__ */
