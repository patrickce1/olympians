//
//  HostScene.h
//  olympians
//
//  Created by Danielle Imogu on 3/4/26.
//
#ifndef __HOST_SETUP_SCENE_H__
#define __HOST_SETUP_SCENE_H__
#include <cugl/cugl.h>
#include <vector>

/**
 * This class provides the interface to make a new game.
 *
 * Most games have a since "matching" scene whose purpose is to initialize the
 * network controller.  We have separate the host from the client to make the
 * code a little more clear.
 */
class HostSetupScene : public cugl::scene2::Scene2 {
public:
    /**
     * The configuration status
     *
     * This is how the application knows to switch to the next scene.
     */
    enum Status {
        /** Host is waiting on a connection */
        WAIT,
        /** Time to start the game */
        START,
        /** Game was aborted; back to main menu */
        ABORT
    };
    
protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;

    /** The menu button for starting a game */
    std::shared_ptr<cugl::scene2::Button> _startgame;
    /** The back button for the menu scene */
    std::shared_ptr<cugl::scene2::Button> _backout;
    /** The player label (for updating) */
    std::shared_ptr<cugl::scene2::TextField> _hostName;
    
    /** The boss selection node list */
    std::vector<std::shared_ptr<cugl::scene2::SceneNode>> _items;
    int _currentIndex = 1;
    /** The boss selction navigation buttons */
    std::shared_ptr<cugl::scene2::Button> _leftButton;
    std::shared_ptr<cugl::scene2::Button> _rightButton;
    /** The boss selection container **/
    std::shared_ptr<cugl::scene2::SceneNode> _container; // holds items
    
    bool _isAnimating = false;
    float _slideDuration = 0.3f;
    cugl::Vec2 _slideTarget = cugl::Vec2();
    
    
    
    /** The current status */
    Status _status;

public:
#pragma mark -
#pragma mark Constructors
    /**
     * Creates a new host scene with the default values.
     *
     * This constructor does not allocate any objects or start the game.
     * This allows us to use the object without a heap pointer.
     */
    HostSetupScene() : cugl::scene2::Scene2() {}
    
    /**
     * Disposes of all (non-static) resources allocated to this mode.
     *
     * This method is different from dispose() in that it ALSO shuts off any
     * static resources, like the input controller.
     */
    ~HostSetupScene() { dispose(); }
    
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
     * @param assets    The (loaded) assets for this game mode
     *
     * @return true if the controller is initialized properly, false otherwise.
     */
    bool init(const std::shared_ptr<cugl::AssetManager>& assets);
    
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
     * Reconfigures the start button for this scene
     *
     * This is necessary because what the buttons do depends on the state of the
     * networking.
     */
    void configureStartButton();
    
    void slideTo(int index);
    

    
};

#endif /* __HOST_SCENE_H__ */
