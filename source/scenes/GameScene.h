//
//  NLMenuScene.h
//  Network Lab
//
//  This class presents the initial menu scene.  It allows the player to
//  choose to be a host or a client.
//
//  Author: Walker White, Aidan Hobler
//  Version: 2/8/22
//
#ifndef __GAME_SCENE_H__
#define __GAME_SCENE_H__
#include <cugl/cugl.h>


/**
 * This class presents the menu to the player.
 *
 * There is no need for an input controller, as all input is managed by
 * listeners on the scene graph.  We only need getters so that the main
 * application can retrieve the state and communicate it to other scenes.
 */
class GameScene : public cugl::scene2::Scene2 {
public:
    /*getters that represent game state should go here*/

protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;

public:
#pragma mark -
#pragma mark Constructors
    /**
     * Creates a new  menu scene with the default values.
     *
     * This constructor does not allocate any objects or start the game.
     * This allows us to use the object without a heap pointer.
     */
    GameScene() : cugl::scene2::Scene2() {}

    /**
     * Disposes of all (non-static) resources allocated to this mode.
     *
     * This method is different from dispose() in that it ALSO shuts off any
     * static resources, like the input controller.
     */
    ~GameScene() { dispose(); }

    /**
     * Disposes of all (non-static) resources allocated to this mode.
     */
    void dispose() override;

    /**
     * Initializes the controller contents.
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
    bool init(const std::shared_ptr<cugl::AssetManager>& assets);

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

};

#endif /* __GAME_SCENE_H__ */
