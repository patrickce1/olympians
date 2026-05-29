#ifndef __BOSS_SELECT_SCENE_H__
#define __BOSS_SELECT_SCENE_H__

#include <cugl/cugl.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include "../InputController.h"
#include "../EnemyLoader.h"
#include "../NetworkController.h"
#include "../AudioController.h"
#include "../ButtonHelpers.h"

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

    /** The audio controller shared across all scenes */
    AudioController* _audio = nullptr;
    
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
     * Maps each card's local X position (from the JSON Float layout) to its
     * card index. Keys are the exact X values from the scene asset file:
     * 0, 216, 432, 648 for cards 0-3 respectively.
     */
    std::map<float, int> _xPosToBoss;
    
    /**
     * Maps each card index to its target container X position (the negative
     * of the card's local X). Used to look up the exact container position
     * that centres a given card without iterating _xPosToBoss in reverse.
     */
    std::map<int, float> _bossToTargetX;
    
    /**
     * The X position of the card container at the moment the current touch
     * began. Combined with _swipeTouchStartX to compute drag deltas in
     * handleSwipeTracking() without accumulated drift.
     */
    float _swipeContainerStartX = 0.0f;
    
    /** Position of the touch on the first frame it was detected. */
    cugl::Vec2 _swipeTouchInitialPos = cugl::Vec2::ZERO;
    
    /** Number of frames the finger has been moving horizontally. */
    int _swipeHoldFrames = 0;

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
     * @param audio    The audio controller used for various sounds. 
     *
     * @return true if the controller is initialized properly, false otherwise.
     */
    bool init(const std::shared_ptr<cugl::AssetManager>& assets, const std::shared_ptr<NetworkController>& networkController, AudioController* audio);
    
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
     * Moves the card container directly under the finger each frame while
     * a swipe is active. Computes the delta from the touch-down position and
     * applies it to the container's position at the start of the drag.
     * Clamps the container so it cannot be dragged past the first or last card.
     *
     * @param input  The input controller for this frame.
     */
    void handleSwipeTracking(InputController& input);

    /**
     * Called on finger lift. Delegates to snapToNearestBoss() to find and
     * animate to the closest card to the current container position.
     * Clears all swipe tracking state before returning.
     *
     * @param input  The input controller for this frame.
     */
    void handleSwipeRelease(InputController& input);

    /**
     * Finds the card whose X position in _xPosToBoss is closest to
     * `releaseContainerX`, updates _currentIndex to that card's index,
     * updates the glow overlays and dot indicators, and initiates a lerp
     * animation to that card's exact centred container position.
     *
     * @param releaseContainerX  The container's X position at the moment
     *                           the finger lifted, in the container's
     *                           parent's local space.
     */
    void snapToNearestBoss(float releaseContainerX);
};

#endif /* __BOSS_SELECT_SCENE_H__ */

