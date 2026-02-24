//
//  InputController.cpp
//  olympians
//
//  Created by Zion Singleton on 2/23/26.
//

#include "InputController.h"

bool InputController::init(){
    _touch = cugl::Input::get<cugl::Touchscreen>();
    if (!_touch){
        return false;
    }
    
    _listenerKey = _touch->acquireKey();
    
    _touch->addBeginListener(_listenerKey, [this](const cugl::TouchEvent& e, bool focus) {
            onTouchBegan(e, focus);
    });
    _touch->addMotionListener(_listenerKey, [this](const cugl::TouchEvent& e,const cugl::Vec2& prev, bool focus) {
        onTouchMoved(e, prev, focus);
    });
    _touch->addEndListener(_listenerKey, [this](const
        cugl::TouchEvent& e, bool focus) {
        onTouchEnded(e, focus);
    });
    return true;
}

void InputController::dispose() {
    if (_touch){
        _touch->removeBeginListener(_listenerKey);
        _touch->removeMotionListener(_listenerKey);
        _touch->removeEndListener(_listenerKey);
    }
    _touch = nullptr;
}
void InputController::setActive(bool active){
    _active = active;
    if (!active){
        _dragging = false;
        _activeTouchID = -1;
        InputController::resetAction();
    }
}
void InputController::onTouchBegan(const cugl::TouchEvent &event, bool focus){
//    CULog("onTouchBegan fired - active: %d pos: %f %f", _active, event.position.x, event.position.y);

    //inactive input or we are already tracking.
    if (!_active or _activeTouchID != -1) {
        return;
    }
    _activeTouchID = event.touch;
    _touchStart = event.position;
    _dragging = false;
    _action = Action::NONE;
}

void InputController::onTouchMoved(const cugl::TouchEvent &event, const cugl::Vec2 &previous, bool focus){
//    CULog("onTouchMoved fired - dragging: %d pos: %f %f", _dragging, event.position.x, event.position.y);

    if (!_active || event.touch != _activeTouchID){
        return;
    }
    if (!_dragging && event.position.distance(_touchStart)> DRAG_THRESHOLD){
        _dragging = true;
    }
    if (_dragging){
        _dragPos = event.position;
        _action = Action::DRAG;
    }
}

void InputController::onTouchEnded(const cugl::TouchEvent &event, bool focus){
//    CULog("onTouchEnded fired - dragging: %d pos: %f %f", _dragging, event.position.x, event.position.y);

    if (_active || event.touch != _activeTouchID){
        return;
    }
    if (_dragging){
        float dx = event.position.x - _touchStart.x;
        float dy = event.position.y - _touchStart.y;
        //A horizontal swipe (Maybe we need to add velocity so that not all horizontal swipes pass.
        if (std::abs(dx) > SWIPE_THRESHOLD && std::abs(dx) > std::abs(dy)){
            if (dx > 0 and _rightPass.contains(event.position)){
                _action = Action::PASS_RIGHT;
            }
            else if (dx < 0 and _leftPass.contains(event.position)){
                _action = Action::PASS_LEFT;
            }
        } else if (_bossZone.contains(event.position)){
            _action = Action::DROP_BOSS;
        } else if (_allyLeft.contains(event.position)){
            _action = Action::DROP_ALLY_LEFT;
        } else if (_allyRight.contains(event.position)){
            _action = Action::DROP_ALLY_RIGHT;
        } else {
            _action = Action::NONE;
        }
    }
    else {
        //This was a tap.
    }
    _activeTouchID = -1;
    _dragging = false;
}

