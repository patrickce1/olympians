#ifndef __BOSS_SELECT_SCENE_H__
#define __BOSS_SELECT_SCENE_H__

#include <cugl/cugl.h>
#include <iostream>
#include <sstream>
#include <vector>
#include "../InputController.h"
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
        PRE_GAMESCENE_START
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
    
    /** How long sliding to new boss index takes */
    float _slideDuration = 0.3f;
    
    /** The vector target position of the selection container */
    cugl::Vec2 _slideTarget = cugl::Vec2();
    
    /** The current status */
    Status _status;
    
    /** Loads enemy definitions from JSON for boss selection. */
    EnemyLoader _enemyLoader;
    
    /** The initial position of the boss carousel. */
    cugl::Vec2 _baseCarouselPosition;
    
    // --- Swipe gesture state ---

    /** X position where the finger first touched down. */
    float _swipeTouchStartX = 0.0f;

    /** Whether a swipe gesture is currently being tracked. */
    bool _isSwiping = false;
    
    /**
     * The X position of the card container at the moment the current touch
     * began. Combined with _swipeTouchStartX to compute drag deltas in
     * handleSwipeTracking() without accumulated drift.
     */
    float _swipeContainerStartX = 0.0f;
    
    /**
     * Minimum horizontal pixel distance the finger must travel before
     * the gesture is committed as a swipe and a slide is triggered.
     */
    static constexpr float SWIPE_THRESHOLD = 80.0f;

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
     * @param input         The input controller instance
     */
    void update(float timestep, InputController& input);
    

private:
    
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
     * @param newIndex The index of the item to slide to.
     */
    void slideTo(int index);
    
    /**
     * Updates the circular indicators at the bottom of what card in the carousel
     * we are currently at.
     *
     * @param currentIndex The index of the card we are at.
     */
    void updateCarouselDots(int currentIndex);
    
    /** Loads boss definitions from the enemies JSON to use in selection. */
    bool loadBosses();
    
    /**
     * Records the touch-down position to begin tracking a potential swipe.
     *
     * Called every frame from update(). On the first frame a touch is
     * detected while no swipe is already in progress, stores the starting
     * X coordinate (screen space) in _swipeTouchStartX and sets _isSwiping.
     * No-op on subsequent frames or when a gesture is already active.
     *
     * @param input  The input controller for this frame.
     */
    void handleSwipeBegin(InputController& input);

    /**
     * Drags the card container live under the finger while a swipe is in
     * progress, giving immediate tactile feedback before the gesture commits.
     *
     * Called every frame from update(). Reads the current drag position,
     * computes how far the finger has moved from _swipeTouchStartX, and
     * repositions the card container by that delta. The container is clamped
     * to prevent dragging more than one card-width past either end of the
     * carousel. No-op when _isSwiping is false or a lerp animation is
     * already running.
     *
     * @param input  The input controller for this frame.
     */
    void handleSwipeTracking(InputController& input);

    /**
     * Resolves a completed swipe gesture into a carousel slide or a snap-back.
     *
     * Called every frame from update(). When touchEnded() is true, measures
     * total horizontal travel from _swipeTouchStartX to the release position.
     * If the distance exceeds SWIPE_THRESHOLD the carousel advances one card
     * in the swipe direction via slideTo(). Otherwise the container is snapped
     * back to the current slide target with no index change. Always clears
     * _isSwiping and _swipeTouchStartX before returning.
     *
     * @param input  The input controller for this frame.
     */
    void handleSwipeRelease(InputController& input);
};

#endif /* __BOSS_SELECT_SCENE_H__ */

