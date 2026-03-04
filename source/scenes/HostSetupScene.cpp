//
//  HostSetupScene.cpp
//  olympians
//
//  Created by Danielle Imogu on 3/4/26.
//
#include <cugl/cugl.h>
#include <iostream>
#include <sstream>

#include "HostSetupScene.h"

using namespace cugl;
using namespace cugl::netcode;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852


#pragma mark -
#pragma mark Provided Methods
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
bool HostSetupScene::init(const std::shared_ptr<cugl::AssetManager>& assets) {
    // Initialize the scene to a locked width
    if (assets == nullptr) {
        return false;
    } else if (!Scene2::initWithHint(Size(0,SCENE_HEIGHT))) {
        return false;
    }
    
    // Start up the input handler
    _assets = assets;
    
    Size dimen = getSize();
    
    // Acquire the scene built by the asset loader and resize it the scene
    std::shared_ptr<scene2::SceneNode> scene = _assets->get<scene2::SceneNode>("hostSetupScene");
    scene->setContentSize(dimen);
    scene->doLayout(); // Repositions the HUD

    _startgame = std::dynamic_pointer_cast<scene2::Button>(_assets->get<scene2::SceneNode>("hostSetupScene.start"));
    _backout = std::dynamic_pointer_cast<scene2::Button>(_assets->get<scene2::SceneNode>("hostSetupScene.back"));
    _hostName = std::dynamic_pointer_cast<scene2::TextField>(_assets->get<scene2::SceneNode>("hostSetupScene.hostName.text"));
    _leftButton = std::dynamic_pointer_cast<scene2::Button>(_assets->get<scene2::SceneNode>("hostSetupScene.leftscroll"));
    _rightButton = std::dynamic_pointer_cast<scene2::Button>(_assets->get<scene2::SceneNode>("hostSetupScene.rightscroll"));
    
    _container = _assets->get<scene2::SceneNode>("hostSetupScene.Role Carousel");
    
    if (_container) {
        // Three placeholder bosses
        _items.push_back(_container->getChild(0));
        _items.push_back(_container->getChild(1));
        _items.push_back(_container->getChild(2));
    }
    
    _status = Status::WAIT;
    
    // Program the buttons
    _backout->addListener([this](const std::string& name, bool down) {
        if (down) {
            _status = Status::ABORT;
        }
    });
    
    _leftButton->addListener([this](const std::string& name, bool down){
        if (!down) slideTo(_currentIndex - 1);
    });

    _rightButton->addListener([this](const std::string& name, bool down){
        if (!down) slideTo(_currentIndex + 1);
    });
    
    std::shared_ptr<cugl::scene2::Label> placeName = std::dynamic_pointer_cast<scene2::Label>(_assets->get<scene2::SceneNode>("hostSetupScene.hostName.placeholder"));
    placeName->setText("Enter Name");
    
    _hostName->addTypeListener([this, placeName](const std::string& name, const std::string& value) {
        placeName->setVisible(value.empty());
    });
    
    addChild(scene);
    setActive(false);
    return true;
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void HostSetupScene::dispose() {
    if (_active) {
        removeAllChildren();
        _active = false;
    }
}

/**
 * Sets whether the scene is currently active
 *
 * This method should be used to toggle all the UI elements.  Buttons
 * should be activated when it is made active and deactivated when
 * it is not.
 *
 * @param value whether the scene is currently active
 */
void HostSetupScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            _status = WAIT;
            _startgame->activate();
            _leftButton->activate();
            _rightButton->activate();
            _hostName->activate();
//            configureStartButton();
            _backout->activate();
        } else {
            _startgame->deactivate();
            _leftButton->deactivate();
            _rightButton->deactivate();
            _backout->deactivate();
            _hostName->deactivate();
            // If any were pressed, reset them
            _startgame->setDown(false);
            _backout->setDown(false);
            _leftButton->setDown(false);
            _rightButton->setDown(false);
        }
    }
}


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
void HostSetupScene::updateText(const std::shared_ptr<scene2::Button>& button, const std::string text) {
    auto label = std::dynamic_pointer_cast<scene2::Label>(button->getChildByName("up")->getChildByName("label"));
    label->setText(text);

}

#pragma mark -
#pragma mark Student Methods
/**
 * The method called to update the scene.
 *
 * We need to update this method to constantly talk to the server
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 */
void HostSetupScene::update(float timestep) {
    if (_isAnimating) {
           Vec2 current = _container->getPosition();
           Vec2 next = current.lerp(_slideTarget, 0.2f); // 0.2 = smoothing factor

           if (current.distance(_slideTarget) < 1.0f) {
               _container->setPosition(_slideTarget);
               _isAnimating = false;
           } else {
               _container->setPosition(next);
           }
       }
}


/**
 * Reconfigures the start button for this scene
 *
 * This is necessary because what the buttons do depends on the state of the
 * networking.
 */
void HostSetupScene::configureStartButton() {
    updateText(_startgame,"Start Game");
    _startgame->activate();
}

void HostSetupScene::slideTo(int newIndex) {
    if (_isAnimating) return;
    if (newIndex < 0 || newIndex >= _items.size()) return;

    _isAnimating = true;
    
    float shiftAmount = 177.22f + 24.17f; // = 201.39f per item
    
    int deltaIndex = newIndex - _currentIndex;

    Vec2 currentPos = _container->getPosition();

    float targetX = currentPos.x - (deltaIndex * shiftAmount);

    _slideTarget = Vec2(targetX, currentPos.y);

    _currentIndex = newIndex;

}
