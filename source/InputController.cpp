#include "InputController.h"

/**
 * Initializes the InputController by acquiring the touchscreen device
 * and registering touch event listeners.
 *
 * @return true if the touchscreen was successfully acquired; false otherwise.
 */
bool InputController::init(){
    // Attempt to acquire the touchscreen input device
    _touch = cugl::Input::get<cugl::Touchscreen>();
    
    //Attempt to acquire the mouse device
    _mouse = cugl::Input::get<cugl::Mouse>();

    CULog("Touch device: %s", _touch ? "acquired" : "null");
    CULog("Mouse device: %s", _mouse ? "acquired" : "null");
    
    //Ensure we have an input.
    if (!_touch && !_mouse){
        return false;
    }

    if (_touch){
        // Acquire a unique listener key for registering/unregistering callbacks on the touchscreen.
        _touchListenerKey = _touch->acquireKey();
        
        // Register callback for when a new touch contact begins
        _touch->addBeginListener(_touchListenerKey, [this](const cugl::TouchEvent& event, bool focus) {
            onTouchBegan(event, focus);
        });
        
        // Register callback for when an active touch contact moves
        _touch->addMotionListener(_touchListenerKey, [this](const cugl::TouchEvent& event, const cugl::Vec2& prev, bool focus) {
            onTouchMoved(event, prev, focus);
        });
        
        // Register callback for when a touch contact is lifted
        _touch->addEndListener(_touchListenerKey, [this](const cugl::TouchEvent& event, bool focus) {
            onTouchEnded(event, focus);
        });
    }
    
    if (_mouse){
        
        
        // Acquire a unique listener key for registering/unregistering callbacks from the mouse.
        _mouseListenerKey = _mouse->acquireKey();
        
        //This enum is used to represent how sensative this device is to movement.
        //Movement events can be extremely prolific, especially if they do not require a button press. This enum is used limit how often these events received.
        _mouse->setPointerAwareness(cugl::Mouse::PointerAwareness::DRAG);

        // Register callback for when the mouse is clicked
        bool ok = _mouse->addPressListener(_mouseListenerKey, [this](const MouseEvent& event, Uint8 clicks, bool focus) {
            onMousePressed(event, clicks, focus);
        });
        CULog("Mouse press listener registered: %s", ok ? "yes" : "no");
        
        // Register callback for when the mouse is being dragged
        ok =_mouse->addDragListener(_mouseListenerKey, [this](const MouseEvent& event, const Vec2& previous, bool focus) {
            onMouseDragged(event, previous, focus);
        });
        CULog("Mouse drag listener registered: %s", ok ? "yes" : "no");

        
        // Register callback for when the mouse is released
        ok =_mouse->addReleaseListener(_mouseListenerKey, [this](const MouseEvent& event, Uint8 clicks, bool focus) {
            onMouseReleased(event, clicks, focus);
        });
        CULog("Mouse release listener registered: %s", ok ? "yes" : "no");

    }
    
    return true;

}

/**
 * Releases all resources held by the InputController.
 *
 * Removes all registered touch event listeners and clears the touchscreen
 */
void InputController::dispose() {
    if (_touch){
        // Unregister all touch event listeners using the acquire key
        _touch->removeBeginListener(_touchListenerKey);
        _touch->removeMotionListener(_touchListenerKey);
        _touch->removeEndListener(_touchListenerKey);
        _touch = nullptr; //deallocate the touchscreen.
        
    }
    if (_mouse) {
        _mouse->removePressListener(_mouseListenerKey);
        _mouse->removeReleaseListener(_mouseListenerKey);
        _mouse->removeDragListener(_mouseListenerKey);
        _mouse = nullptr; //deallocate the mouse.
    }
}

/**
 * Sets whether the InputController is actively processing input.
 *
 * When deactivated, any in-progress drag is cancelled, the tracked touch
 * is cleared, and the current action state is reset.
 *
 * @param active  true to enable input processing; false to disable it.
 */
void InputController::setActive(bool active){
    _active = active;
    if (!active){
        _touchEnded = true;
        // Cancel any in-progress drag gesture
        _dragging = false;
        _activeTouchID = -1;
        // Reset any pending or ongoing action state
        InputController::resetAction();
    }
}

/**
 * Callback invoked when a new touch contact begins.
 *
 * Records the starting touch position and begins tracking the contact.
 * Ignores the event if the controller is inactive or already tracking
 * another touch.
 *
 * @param event  The touch event containing the touch ID and position.
 * @param focus  Whether the application currently has input focus.
 * Focus is unused
 */

void InputController::onTouchBegan(const cugl::TouchEvent &event, bool focus){
    _touchStartTime = event.timestamp;
    //inactive input or we are already tracking.
    if (!_active || _activeTouchID != -1) {
        return;
    }
    //Determine the id of the event, and find the position of that tap.
    _activeTouchID = event.touch;
    _touchStart = event.position;
    _dragPos = event.position;
    _dragging = false;
    _touchEnded = false;
    _action = Action::NONE;
}

/**
 * Callback invoked when an active touch contact moves.
 *
 * Promotes the gesture to a drag once the contact has moved beyond
 * DRAG_THRESHOLD pixels from its starting position, then continuously
 * updates the drag position while dragging.
 *
 * @param event     The touch event containing the current touch ID and position.
 * @param previous  The position of the touch in the previous frame.
 * @param focus     Whether the application currently has input focus.
 * Focus is unused
 */
void InputController::onTouchMoved(const cugl::TouchEvent &event, const cugl::Vec2 &previous, bool focus){
    
    if (!_active || event.touch != _activeTouchID){ //We are not ready for a touch.
        return;
    }
    if (!_dragging && event.position.distance(_touchStart)> DRAG_THRESHOLD){
        _dragging = true; //Set to dragging if the finger has moved enough pixels, and not already dragging.
    }
    if (_dragging){
        _dragPos = event.position; //Maintain that we are dragging, and track updated position.
        _action = Action::DRAG;
    }
}

/**
 * Callback invoked when an active touch contact is lifted.
 *
 * Classifies the completed gesture as either a swipe, a drop into a
 * named zone, or a tap, and sets _action accordingly. Clears tracking
 * state regardless of the gesture outcome.
 *
 * @param event  The touch event containing the final touch ID and position.
 * @param focus  Whether the application currently has input focus.
 * focus is unused
 */
void InputController::onTouchEnded(const cugl::TouchEvent &event, bool focus){
    if (event.touch != _activeTouchID){
        return;
    }
    _touchEnded = true;
    _touchDuration = event.timestamp.ellapsedMillis(_touchStartTime);
    // Store raw screen positions only. GameScene converts to world space and classifies all actions.
    _releasePosition = event.position;
    _activeTouchID = -1;
    _dragging = false;
}

/**
 * Called when a mouse button is pressed.
 *
 * Records the starting mouse position and begins tracking the drag.
 * Ignores the event if the controller is inactive or already tracking
 * a mouse button press.
 *
 * @param event   The mouse event containing the button and position.
 * @param clicks  The number of recent clicks including this one (unused).
 * @param focus   Whether the listener currently has focus (unused).
 */
void InputController::onMousePressed(const cugl::MouseEvent& event, Uint8 clicks, bool focus) {
    CULog("onMousePressed fired: active=%d mouseDown=%d", _active, _mouseDown);
    if (!_active || _mouseDown) {
        return;
    }
    _touchStartTime = event.timestamp;
    _mouseDown = true;
    _touchStart = event.position;
    _dragPos = event.position;
    _dragging = false;
    _touchEnded = false;
    _action = Action::NONE;
}

/**
 * Called when the mouse is dragged (moved with a button held).
 *
 * Promotes the gesture to a drag once the mouse has moved beyond
 * DRAG_THRESHOLD pixels from its starting position, then continuously
 * updates the drag position while dragging.
 *
 * @param event     The mouse event containing the current position.
 * @param previous  The position of the mouse in the previous frame.
 * @param focus     Whether the listener currently has focus (unused).
 */
void InputController::onMouseDragged(const cugl::MouseEvent& event, const cugl::Vec2& previous, bool focus) {
    CULog("onMouseDragged: pos=(%.1f,%.1f) start=(%.1f,%.1f) dist=%.1f threshold=%.1f",
        event.position.x, event.position.y,
        _touchStart.x, _touchStart.y,
        event.position.distance(_touchStart),
        DRAG_THRESHOLD);    if (!_active || !_mouseDown) {
        return;
    }
    if (!_dragging && event.position.distance(_touchStart) > DRAG_THRESHOLD) {
        _dragging = true;
    }
    if (_dragging) {
        _dragPos = event.position;
        _action = Action::DRAG;
    }
}

/**
 * Called when a mouse button is released.
 *
 * Classifies the completed gesture as either a swipe, a drop into a
 * named zone, or a tap, and sets _action accordingly. Clears tracking
 * state regardless of the gesture outcome.
 *
 * @param event   The mouse event containing the final position.
 * @param clicks  The number of recent clicks including this one (unused).
 * @param focus   Whether the listener currently has focus (unused).
 */
void InputController::onMouseReleased(const cugl::MouseEvent& event, Uint8 clicks, bool focus) {
    CULog("onMouseReleased fired: mouseDown=%d", _mouseDown);
    if (!_mouseDown) {
        return;
    }
    _touchEnded = true;
    _touchDuration = event.timestamp.ellapsedMillis(_touchStartTime);
    _releasePosition = event.position;
    _mouseDown = false;
    _dragging = false;
}
