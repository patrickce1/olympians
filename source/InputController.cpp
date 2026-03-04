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
    if (!_touch){
        return false;
    }
    //Necessary to work on phones, convert to the phones space dimensions.
    cugl::Rect full = cugl::Application::get()->getDisplayBounds();
    cugl::Rect drawable = cugl::Application::get()->getDrawableBounds();
    _scale = full.size.width / drawable.size.width;
    CULog("LOGICAL SCALE: %f",_scale);
    // Acquire a unique listener key for registering/unregistering callbacks
    _listenerKey = _touch->acquireKey();
    
    // Register callback for when a new touch contact begins
    _touch->addBeginListener(_listenerKey, [this](const cugl::TouchEvent& e, bool focus) {
            onTouchBegan(e, focus);
        });

    // Register callback for when an active touch contact moves
    _touch->addMotionListener(_listenerKey, [this](const cugl::TouchEvent& e, const cugl::Vec2& prev, bool focus) {
            onTouchMoved(e, prev, focus);
        });

    // Register callback for when a touch contact is lifted
    _touch->addEndListener(_listenerKey, [this](const cugl::TouchEvent& e, bool focus) {
            onTouchEnded(e, focus);
        });
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
        _touch->removeBeginListener(_listenerKey);
        _touch->removeMotionListener(_listenerKey);
        _touch->removeEndListener(_listenerKey);
    }
    _touch = nullptr; //deallocate the touchscreen.
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
    cugl::Vec2 pos = event.position * _scale;
    // Convert to scene coordinates (bottom-left origin)
    pos.y = cugl::Application::get()->getDisplayBounds().size.height - pos.y;
    //inactive input or we are already tracking.
    if (!_active or _activeTouchID != -1) {
        return;
    }
    //Determine the id of the event, and find the position of that tap.
    _activeTouchID = event.touch;
    _touchStart = pos;
    _dragPos = pos;
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
    //Determine the position in phone coordinates
    cugl::Vec2 pos = event.position * _scale;
    // Convert to scene coordinates (bottom-left origin)
    pos.y = cugl::Application::get()->getDisplayBounds().size.height - pos.y;
    if (!_active || event.touch != _activeTouchID){ //We are not ready for a touch.
        return;
    }
    if (!_dragging && pos.distance(_touchStart)> DRAG_THRESHOLD){
        _dragging = true; //Set to dragging if the finger has moved enough pixels, and not already dragging.
    }
    if (_dragging){
        _dragPos = pos; //Maintain that we are dragging, and track updated position.
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
    
    cugl::Vec2 pos = event.position * _scale;
    // Convert to scene coordinates (bottom-left origin)
    pos.y = cugl::Application::get()->getDisplayBounds().size.height - pos.y;

    if (event.touch != _activeTouchID){
        return;
    }
    if (_dragging){
        float dx = pos.x - _touchStart.x;
        float dy = pos.y - _touchStart.y;
        float elapsed = event.timestamp.ellapsedMillis(_touchStartTime);
        //Classify as a horizontal swipe if displacement dominates vertically and exceeds the swipe threshold.
        //A horizontal swipe (Maybe we need to add velocity so that not all horizontal swipes pass.
        if (std::abs(dx) > SWIPE_THRESHOLD && std::abs(dx) > std::abs(dy)){
            if (dx > 0 && _rightPass.contains(pos)){
                _action = Action::PASS_RIGHT;
            }
            else if (dx < 0 && _leftPass.contains(pos)){
                _action = Action::PASS_LEFT;
            }
        } else if (_bossZone.contains(pos)){
            _action = Action::DROP_BOSS;
        } else if (_allyLeft.contains(pos)){
            _action = Action::DROP_ALLY_LEFT;
        } else if (_allyRight.contains(pos)){
            _action = Action::DROP_ALLY_RIGHT;
        } else {
            _action = Action::NONE;
        }
    }
    _activeTouchID = -1;
    _dragging = false;
    _touchEnded = true;
}

