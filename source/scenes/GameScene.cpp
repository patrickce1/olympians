#include <cugl/cugl.h>
#include <iostream>
#include <sstream>

#include "GameScene.h"

using namespace cugl;
using namespace cugl::scene2;
using namespace std;

#pragma mark -
#pragma mark Level Layout

/** Example height for now, change as needed */
#define SCENE_HEIGHT  720


#pragma mark -
#pragma mark Constructors
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
bool GameScene::init(const std::shared_ptr<cugl::AssetManager>& assets) {
    // Initialize the scene to a locked width
    if (assets == nullptr) {
        return false;
    }
    else if (!Scene2::initWithHint(Size(0, SCENE_HEIGHT))) {
        return false;
    }

    // Start up the input handler
    _assets = assets;
    Size dimen = getSize();
    
    // The comments are outline of how loading a scene from json should work. This DOES NOT WORK YET. Danielle should set this up
    // Acquire the scene built by the asset loader and resize it the scene. 
    //std::shared_ptr<scene2::SceneNode> scene = _assets->get<scene2::SceneNode>("gameScene");

    //scene->setContentSize(dimen);
    //scene->doLayout(); // Repositions the HUD
   
    //addChild(scene);
    setActive(false);
    return true;
}

void GameScene::update(float dt) {
    //nothing for now
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void GameScene::dispose() {
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
void GameScene::setActive(bool value) {
    if (isActive() != value) {
        Scene2::setActive(value);
        if (value) {
            //put anything that needs to be activated as part of the scene
        }
        else {
            //deactivate any children that are part of the scene
        }
    }
}
