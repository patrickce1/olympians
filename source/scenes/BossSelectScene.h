#ifndef __BOSS_SELECT_SCENE_H__
#define __BOSS_SELECT_SCENE_H__

#include <cugl/cugl.h>
#include <iostream>
#include <sstream>
#include <vector>
#include "../EnemyLoader.h"
#include "../NetworkController.h"

/**
 * This class provides the interface to make the boss select scene.
 */
class BossSelectScene : public cugl::scene2::Scene2 {
public:
    /**
     * The configuration status
     *
     * This is how the application knows to switch to the next scene.
     */
    enum Status {
        /**  */
        WAIT,
        /** Selection was aborted; back to lobby */
        ABORT,
        /** Game scene has been started by host**/
        GAMESCENE_START
    };
    
protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;

    /** The network controller shared across all scenes*/
    std::shared_ptr<NetworkController> _network;
    
    /** The back button for the boss select scene */
    std::shared_ptr<cugl::scene2::Button> _backButton;
    
    /** The lock button to change boss in the boss select scene */
    std::shared_ptr<cugl::scene2::Button> _lockButton;
    
    /** The boss selection node list */
    std::vector<std::shared_ptr<cugl::scene2::SceneNode>> _bossCards;
    
    /** The boss selection indicator list */
    std::vector<std::shared_ptr<cugl::scene2::SceneNode>> _bossCarouselDotIndicators;
    
    /** The current index of the boss shown in the boss selection screen*/
    int _currentIndex = 1;
    
    /** The boss selection navigation left button */
    std::shared_ptr<cugl::scene2::Button> _leftButton;
    
    /** The boss selection navigation right button */
    std::shared_ptr<cugl::scene2::Button> _rightButton;
    
    /** The boss selection container **/
    std::shared_ptr<cugl::scene2::SceneNode> _bossSelectionCardContainer; // holds items
    
    /** Whether the boss selection screen is sliding to another index */
    bool _isAnimating = false;
    
    /** The vector target position of the selection container */
    cugl::Vec2 _slideTarget = cugl::Vec2();
    
    /** The current status */
    Status _status;
    
    /** Loads enemy definitions from JSON for boss selection. */
    EnemyLoader _enemyLoader;
    
    /** Key for the touchscreen listener. */
    Uint32 _touchKey;
    
    /** Active touch ID for swiping. */
    Sint64 _activeTouch;
    
    /** The starting position of a touch gesture for swiping. */
    cugl::Vec2 _touchStartPos;

    /** Container position at touch begin, used for drag interpolation. */
    cugl::Vec2 _touchStartContainerPos;

    /** Whether an active touch is currently dragging the carousel. */
    bool _isTouchDragging = false;

    /** Absolute snap baseline for carousel x-position. */
    cugl::Vec2 _carouselBasePos;

    /** The index corresponding to _carouselBasePos. */
    int _carouselBaseIndex = 1;

public:
#pragma mark -
#pragma mark Constructors
    /**
     * Creates a new bost select scene with the default values.
     *
     * This constructor does not allocate any objects or start the game.
     * This allows us to use the object without a heap pointer.
     */
    BossSelectScene() : cugl::scene2::Scene2() {}
    
    /**
     * Disposes of all (non-static) resources allocated to this mode.
     *
     * This method is different from dispose() in that it ALSO shuts off any
     * static resources, like the input controller.
     */
    ~BossSelectScene() { dispose(); }
    
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
     * @param networkController The network controller shared across all scenes
     *
     * @return true if the controller is initialized properly, false otherwise.
     */
    bool init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController);
    
    /**
     * Retrieves and stores references to the BossSelectScene UI elements.
     *
     * This method looks up UI components from the scene graph including the
     * lock button, back button, carousel navigation
     * buttons, and the role carousel container. It also initializes the
     * carousel item list.
     */
    void setupUI();
    
    /**
     * Attaches input listeners to the boss select buttons.
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
     * @param timestep  The amount of time (in seconds) since the last frame
     */
    void update(float timestep) override;
    

private:
    
    /**
     * Starts a swipe gesture for the boss carousel.
     *
     * Captures the active touch ID, pointer start position, and carousel
     * start position for drag-relative movement.
     *
     * @param event  The touch begin event.
     */
    void beginCarouselSwipe(const cugl::TouchEvent& event);

    /**
     * Updates carousel position while an active swipe is in progress.
     *
     * Applies drag resistance and clamps movement to first/last card bounds.
     *
     * @param event  The touch motion event.
     */
    void updateCarouselSwipe(const cugl::TouchEvent& event);

    /**
     * Ends the active swipe gesture and snaps to a valid selection.
     *
     * If drag distance passes the commit threshold, advances one card in
     * swipe direction; otherwise returns to the current card.
     *
     * @param event  The touch end event.
     */
    void endCarouselSwipe(const cugl::TouchEvent& event);

    /**
     * Returns the absolute target x-position for the given card index.
     *
     * This anchor mapping is used by both drag clamping and snap targets.
     *
     * @param index  The card index in the carousel.
     *
     * @return the absolute x-position anchor for that index.
     */
    float getTargetXForIndex(int index) const;

    /**
     * Snaps the carousel to a valid card based on drag displacement.
     *
     * Uses a thresholded one-step commit model to reduce accidental changes.
     */
    void snapToNearestIndex();
    
    /**
     * Reconfigures the lock button for this scene
     *
     * This is necessary because what the buttons do depends on the state of the
     * networking.
     */
    void configureLockButton();
    
    /**
     * Initiates a slide animation to center the item at `newIndex`.
     *
     * Does nothing if an animation is already in progress or if the
     * index is out of bounds. Otherwise computes the target container
     * position and stores it in `_slideTarget`.
     *
    * @param newIndex  The index of the item to slide to.
     */
    void slideTo(int newIndex);
    
    /**
     * Updates the circular indicators at the bottom of what card in the carousel
     * we are currently at.
     *
     * @param currentIndex The index of the card we are at.
     */
    void updateCarouselDots(int currentIndex);
    
    /** Loads boss definitions from the enemies JSON to use in selection. */
    bool loadBosses();
};

#endif /* __BOSS_SELECT_SCENE_H__ */

