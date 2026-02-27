#ifndef __INPUT_CONTROLLER_H__
#define __INPUT_CONTROLLER_H__
#include <cugl/cugl.h>

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
    //Types of actions that the player will be able to use.
    
    enum class Action{
        NONE, //doing nothing
        DRAG, //Dragging
        DROP_BOSS, //dropped in boss zone (attack)
        DROP_ALLY_LEFT, //Heal/support
        DROP_ALLY_RIGHT,
        PASS_LEFT, //Passed item
        PASS_RIGHT,
        PAUSE, //Clicked pause
    };
    
    // MARK: - Constructors
    /**Uninitialized Input Controller*/
    InputController() = default;
    
    //Destroy the inputcontroller
    ~InputController() {
        dispose();
    }
    
    /**Life actions**/
    bool init();
    
    void dispose();
    
    /**Scene Control**/
    //Activate touch controls.
    void setActive(bool active);
    //Returns whether touch is active monitoring.
    bool isActive() const{
        return _active;
    }
    
    /**ZONES
     Where players will drag to activate different actions*/
    void setBossZone(cugl::Rect r) {
        _bossZone = r;
    }
    void setAllyZoneLeft(cugl::Rect r) {
        _allyLeft = r;
    }
    void setAllyZoneRight(cugl::Rect r) {
        _allyRight = r;
    }
    void setPasssZoneLeft(cugl::Rect r) {
        _leftPass = r;
    }
    void setPassZoneRight(cugl::Rect r) {
        _rightPass = r;
    }
    
    /**Queries**/
    //Return what the player did
    Action getAction() const {
        return _action;
    }
    //Seet player action to none. Our touch has not started.
    void resetAction() {
        _action = Action::NONE;
        _touchEnded = false;
    }
    
    //Only meaningful when isDragging == True and Given in Screen Coordinates.
    cugl::Vec2 getDragPos() const {
        return _dragPos;
    }
    //Original location of the drag start.
    cugl::Vec2 getTouchStart() const {
        return _touchStart;
    }
    //Return if player is dragging
    bool isDragging() const {
        return _dragging;
    }
    //Return if the touch has ended
    bool touchEnded() const {
        return _touchEnded;
    }
    bool isTouching() const {
        return _activeTouchID != -1 && !_touchEnded;
    }
private:
    /** Minimum horizontal travel (px) to classify a release as a swipe. */
    static constexpr float SWIPE_THRESHOLD = 75.0f;
    /** Minimum travel (px) from touch start before a drag is recognized. */
    static constexpr float DRAG_THRESHOLD = 2.0f;
    cugl::Timestamp _touchStartTime;
    float SWIPE_MAX_DURATION = 300.0f; // milliseconds
    
    /**STATES**/
    bool _active = false;
    bool _dragging = false;
    bool _touchEnded = false;
    Action _action = Action::NONE;
    cugl::TouchID _activeTouchID = -1;
    cugl::Vec2 _touchStart;
    cugl::Vec2 _dragPos;
    Uint32 _listenerKey = 17;
    
    cugl::Rect _bossZone;
    cugl::Rect _allyLeft;
    cugl::Rect _allyRight;
    cugl::Rect _leftPass;
    cugl::Rect _rightPass;
    
    float _scale; //Used to normalize for different screen sizes
    
    /**The CUGL TOUCHSCREEN**/
    cugl::Touchscreen* _touch = nullptr;
    
    /**Listeners**/
    void onTouchBegan(const cugl::TouchEvent& event, bool focus);
    void onTouchMoved(const cugl::TouchEvent& event, const cugl::Vec2& previous, bool focus);
    void onTouchEnded(const cugl::TouchEvent& event, bool focus);
};
#endif // !__INPUT_CONTROLLER_H__
