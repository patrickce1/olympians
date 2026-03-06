#ifndef __INPUT_CONTROLLER_H__
#define __INPUT_CONTROLLER_H__
#include <cugl/cugl.h>
#include <iostream>
#include <sstream>

using namespace cugl;
using namespace cugl::scene2;
using namespace std;

/**
 * Controller that handles all player touch input for the gameplay scene.
 *
 * This controller uses CUGL's Touchscreen listener API to detect and classify
 * gestures into game actions. It persists throughout the life of the game but
 * can be toggled on/off between scenes to prevent input bleed.
 *
 * Supported gestures (all drag-based):
 *   - Hold and drag        : move item within inventory
 *   - Release on boss zone : attack enemy
 *   - Release on ally zone : support teammate
 *   - Swipe left/right     : pass item to adjacent player
 *
 * Usage:
 *   1. Call init() once at app startup
 *   2. Call setActive(true/false) on scene transitions
 *   3. Call setBossZone/setAllyZone from your scene once layout is known
 *   4. Read getAction() and getDragPos() each frame in your scene update()
 *   5. Call resetAction() after consuming the action
 *   6. Call dispose() on app shutdown
 */
class InputController {
public:

    /**
     * Enumerates all discrete game actions that a touch gesture can resolve to.
     * Set by onTouchEnded() and read each frame via getAction().
     */
    enum class Action {
        NONE,           ///< No gesture in progress or gesture did not resolve.
        DRAG,           ///< Finger is actively dragging an item.
        DROP_BOSS,      ///< Item released onto the boss zone — triggers an attack.
        DROP_ALLY_LEFT, ///< Item released onto the left ally zone — triggers support.
        DROP_ALLY_RIGHT,///< Item released onto the right ally zone — triggers support.
        PASS_LEFT,      ///< Horizontal swipe left — passes item to the left neighbour.
        PASS_RIGHT,     ///< Horizontal swipe right — passes item to the right neighbour.
        PAUSE,          ///< Pause button tapped.
    };

#pragma mark - Constructors

    /** Constructs an uninitialised InputController. Call init() before use. */
    InputController() = default;

    /**
     * Destroys the InputController, releasing all touch listeners.
     * Equivalent to calling dispose().
     */
    ~InputController() { dispose(); }

#pragma mark - Lifecycle

    /**
     * Initialises the InputController by acquiring the Touchscreen device,
     * computing the display-to-drawable scale factor, and registering
     * begin/motion/end touch listeners.
     *
     * @return true if the Touchscreen was successfully acquired; false otherwise.
     */
    bool init();

    /**
     * Releases all resources held by this controller.
     *
     * Unregisters all touch listeners and clears the Touchscreen pointer.
     * Safe to call even if init() was never called.
     */
    void dispose();

#pragma mark - Activation

    /**
     * Sets whether this controller is actively processing touch input.
     *
     * When deactivated, any in-progress drag is cancelled, the tracked
     * touch ID is cleared, and the action state is reset to NONE.
     *
     * @param active  true to enable input processing; false to disable it.
     */
    void setActive(bool active);

    /**
     * Returns whether this controller is currently processing input.
     *
     * @return true if active; false otherwise.
     */
    bool isActive() const { return _active; }

#pragma mark - Zone Configuration

    /**
     * Sets the rectangular region that classifies a drag release as a boss attack.
     * Should be called once the scene layout is known, in scene coordinates.
     *
     * @param r  The boss drop zone rectangle.
     */
    void setBossZone(cugl::Rect r) { _bossZone = r; }

    /**
     * Sets the rectangular region that classifies a drag release as a
     * left-ally support action.
     *
     * @param r  The left ally drop zone rectangle.
     */
    void setAllyZoneLeft(cugl::Rect r) { _allyLeft = r; }

    /**
     * Sets the rectangular region that classifies a drag release as a
     * right-ally support action.
     *
     * @param r  The right ally drop zone rectangle.
     */
    void setAllyZoneRight(cugl::Rect r) { _allyRight = r; }

    /**
     * Sets the rectangular region used to validate a left-pass swipe gesture.
     *
     * @param r  The left pass zone rectangle.
     */
    void setPasssZoneLeft(cugl::Rect r) { _leftPass = r; }

    /**
     * Sets the rectangular region used to validate a right-pass swipe gesture.
     *
     * @param r  The right pass zone rectangle.
     */
    void setPassZoneRight(cugl::Rect r) { _rightPass = r; }

#pragma mark - State Queries

    /**
     * Returns the action resolved from the most recent completed gesture.
     * Will be NONE until a gesture completes, and should be reset each
     * frame after consumption via resetAction().
     *
     * @return The current action.
     */
    Action getAction() const { return _action; }

    /**
     * Returns true if a touch contact ended this frame.
     * Cleared by resetAction() after consumption.
     *
     * @return true if a touch ended; false otherwise.
     */
    bool touchEnded() const { return _touchEnded; }

    /**
     * Returns the current drag position in scaled screen coordinates.
     * Only meaningful while isDragging() returns true.
     *
     * @return The current drag position.
     */
    cugl::Vec2 getDragPos() const { return _dragPos; }

    /**
     * Returns the position where the current touch contact began,
     * in scaled screen coordinates.
     *
     * @return The touch start position.
     */
    cugl::Vec2 getTouchStart() const { return _touchStart; }

    /**
     * Returns whether the tracked touch has moved beyond DRAG_THRESHOLD
     * and is being classified as a drag gesture.
     *
     * @return true if dragging; false otherwise.
     */
    bool isDragging() const { return _dragging; }

    /**
     * Returns the position where the most recent touch contact was released,
     * in scaled screen coordinates. Valid after touchEnded() returns true.
     *
     * @return The release position.
     */
    cugl::Vec2 getReleasePosition() const { return _releasePosition; }

    /**
     * Returns whether a touch contact is currently active and has not yet ended.
     *
     * @return true if the user is currently touching the screen; false otherwise.
     */
    bool isTouching() const { return _activeTouchID != -1 && !_touchEnded; }

    /**
     * Overrides the current action with the given value.
     * Prefer resetAction() for clearing after consumption.
     *
     * @param action  The action to set.
     */
    void setAction(Action action) { _action = action; }

    /**
     * Resets the action to NONE and clears the touchEnded flag.
     * Should be called once per frame after the action has been consumed
     * to prevent the same gesture from firing on subsequent frames.
     */
    void resetAction() {
        _action = Action::NONE;
        _touchEnded = false;
    }

private:

    /** Minimum horizontal travel (px) to classify a release as a pass swipe. */
    static constexpr float SWIPE_THRESHOLD = 75.0f;

    /** Minimum travel (px) from touch start before a drag is recognised. */
    static constexpr float DRAG_THRESHOLD = 2.0f;

    /** Timestamp recorded when the current touch contact began. */
    cugl::Timestamp _touchStartTime;

    /** Duration in seconds of the current touch contact. */
    float _touchDuration;

    // ----- State -----

    /** Whether this controller is actively processing input. */
    bool _active = false;

    /** Whether the tracked touch has been promoted to a drag gesture. */
    bool _dragging = false;

    /** The action resolved from the most recent completed gesture. */
    Action _action = Action::NONE;

    /** The touch ID currently being tracked; -1 if no touch is active. */
    cugl::TouchID _activeTouchID = -1;

    /** Screen-space position where the current touch contact began. */
    cugl::Vec2 _touchStart;

    /** True for one frame after a touch contact ends; cleared by resetAction(). */
    bool _touchEnded = false;

    /** Screen-space position where the most recent touch contact was released. */
    cugl::Vec2 _releasePosition;

    /** Screen-space position of the finger during an active drag. */
    cugl::Vec2 _dragPos;

    /** Unique key used to register and unregister touch event listeners. */
    Uint32 _listenerKey = 17;

    // ----- Drop and swipe zones -----

    /** Zone that classifies a drop as a boss attack. */
    cugl::Rect _bossZone;

    /** Zone that classifies a drop as a left-ally support action. */
    cugl::Rect _allyLeft;

    /** Zone that classifies a drop as a right-ally support action. */
    cugl::Rect _allyRight;

    /** Zone that validates a left-pass swipe release position. */
    cugl::Rect _leftPass;

    /** Zone that validates a right-pass swipe release position. */
    cugl::Rect _rightPass;

    /**
     * Scale factor converting raw device touch coordinates to the
     * application's drawable coordinate space. Computed once in init().
     */
    float _scale;

    /** Pointer to the CUGL Touchscreen device. Not owned by this class. */
    cugl::Touchscreen* _touch = nullptr;

    // ----- Touch event callbacks -----

    /**
     * Called when a new touch contact begins.
     * Records the starting position and begins tracking the contact.
     *
     * @param event  The touch event containing the touch ID and position.
     * @param focus  Whether the application currently has input focus (unused).
     */
    void onTouchBegan(const cugl::TouchEvent& event, bool focus);

    /**
     * Called when an active touch contact moves.
     * Promotes the gesture to a drag once movement exceeds DRAG_THRESHOLD,
     * then continuously updates the drag position.
     *
     * @param event     The touch event containing the current position.
     * @param previous  The position of the touch in the previous frame.
     * @param focus     Whether the application currently has input focus (unused).
     */
    void onTouchMoved(const cugl::TouchEvent& event, const cugl::Vec2& previous, bool focus);

    /**
     * Called when an active touch contact is lifted.
     * Classifies the completed gesture as a swipe, zone drop, or tap,
     * sets _action and _releasePosition accordingly, and clears tracking state.
     *
     * @param event  The touch event containing the final position.
     * @param focus  Whether the application currently has input focus (unused).
     */
    void onTouchEnded(const cugl::TouchEvent& event, bool focus);
};

#endif // !__INPUT_CONTROLLER_H__
