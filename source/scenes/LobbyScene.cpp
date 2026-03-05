//
//  LobbyScene.cpp
//  olympians
//
//  Created by Danielle Imogu on 3/4/26.
//
#include <cugl/cugl.h>
#include <iostream>
#include <sstream>

#include "LobbyScene.h"

using namespace cugl;
using namespace cugl::netcode;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Regardless of logo, lock the height to this */
#define SCENE_HEIGHT  852


/**
 * Initializes the controller contents, and starts the game
 *
 * The constructor does not allocate any objects or memory.  This allows
 * us to have a non-pointer reference to this controller, reducing our
 * memory allocation.  Instead, allocation happens in this method.
 *
 * @param assets    The (loaded) assets for this game mode
 *
 * @return true if the controller is initialized properly, false otherwise.
 */
bool LobbyScene::init(const std::shared_ptr<cugl::AssetManager>& assets) {
    // Initialize the scene to a locked width
    if (assets == nullptr) {
        return false;
    } else if (!Scene2::initWithHint(Size(0,SCENE_HEIGHT))) {
        return false;
    }
    
    // Start up the input handler
    _assets = assets;
    
    Size dimen = getSize();
    
    std::shared_ptr<scene2::SceneNode> scene = _assets->get<scene2::SceneNode>("lobbyScene");
    
    scene->setContentSize(dimen);
    scene->doLayout(); // Repositions the HUD
    
    _entergame = std::dynamic_pointer_cast<scene2::Button>(_assets->get<scene2::SceneNode>("lobbyScene.start"));
//    _backout = std::dynamic_pointer_cast<scene2::Button>(_assets->get<scene2::SceneNode>("clientScene.back"));
    _gameid = std::dynamic_pointer_cast<scene2::Label>(_assets->get<scene2::SceneNode>("lobbyScene.header.gameId"));
    
    _entergame->addListener([this](const std::string& name, bool down) {
        if (down) {
            _status = Status::START;
        }
    });
    
//    _backout->addListener([this](const std::string& name, bool down) {
//        if (down) {
//            _status = Status::ABORT;
//        }
//    });
    
    _status = Status::IDLE;
    
    addChild(scene);
    setActive(false);
    return true;
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void LobbyScene::dispose() {
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
void LobbyScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            _status = IDLE;
            _entergame->activate();
//            _backout->activate();
        } else {
//            _backout->deactivate();
            // If any were pressed, reset them
            _entergame->deactivate();
            _entergame->setDown(false);
//            _backout->setDown(false);
        }
    }
}

/**
 * The method called to update the scene.
 *
 * We need to update this method to constantly talk to the server
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 */
void LobbyScene::update(float timestep) {
    // IMPLEMENT ME

}

