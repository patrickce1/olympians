#ifndef __SCENE_LOADER_H__
#define __SCENE_LOADER_H__
#include <cugl/cugl.h>
#include "scenes/GameScene.h"
#include "scenes/MenuScene.h"
#include "InputController.h"
#include "PlayerTests.h"
#include "EnemyTests.h"

/**
 * Scene loader class responsible for loading assets and managing scene transitions
 */
class SceneLoader : public cugl::Application {
protected:
    /* This enum keeps track of which scene/mode we are in right now
     * Will have to be expanded as we add more scenes*/
    enum class State {
        LOAD,
        MENU,
        GAME
    };

    State _currentScene;

    /** The loaders to (synchronously) load in assets */
    std::shared_ptr<cugl::AssetManager> _assets;

    /** A 3152 style SpriteBatch to render the scene MOST LIKELY NEEDS CHANGING, I THINK WE'RE NOT SUPPOSED TO USE THIS METHOD? */
    std::shared_ptr<cugl::graphics::SpriteBatch>  _batch;

    /** A logger for debugging, can be removed if we feel like this is not necessary */
    std::shared_ptr<cugl::Logger> _logger;
    
    InputController _input;

    /* All the scenes in the game*/
    std::shared_ptr<cugl::scene2::LoadingScene> _loadingScene;
    MenuScene _menuScene;
    GameScene _gameScene;
    //more scenes to come...

public:
    /**
     * Creates, but does not initialize, a new application.
     *
     * This is configuring things before most of the backend is initialized.
     * Do NOT use this constructor for anything other than initializing attributes
     * because most of the cugl backend is not properly initialized at this point
     */
    SceneLoader();

    /**
     * Disposes this application, releasing all resources.
     *
     * This destructor is called by SDL when the application quits. It simply
     * calls the dispose() method in Application. 
     */
    ~SceneLoader() {}

    /**
     * The method called after the backend is initialized.
     *
     * This method is called once CUGL methods are safe to access, but before
     * the application starts to run. This is the method in which all
     * user-defined program intialization should take place. You should not
     * create a new init() method.
     *
     * When overriding this method, you should call the parent method as the
     * very last line.  This ensures that the state will transition to FOREGROUND,
     * causing the application to run.
     */
    virtual void onStartup() override;

    /**
     * The method called when the application is ready to quit.
     *
     * This is the method to dispose of all resources allocated by this
     * application.  As a rule of thumb, everything created in onStartup()
     * should be deleted here.
     *
     * When overriding this method, you should call the parent method as the
     * very last line. This ensures that the state will transition to NONE,
     * causing the application to be deleted.
     */
    virtual void onShutdown() override;

    /**
     * The method called when the application window is resized
     *
     * This method will always be called after the size attributes for the
     * application have been updated. You can query the new window size from
     * methods like {@link #getDisplayBounds} and {@link #getDrawableBounds}.
     *
     * Note that this method will be called if either the application display
     * orientation or the safe area changes, even if the actual window size
     * remains unchanged.
     */
    virtual void onResize() override;

    /**
     * The method called to update the application data.
     *
     * This is part of your core loop and should be replaced with your custom
     * implementation. This method should contain any code that is not a
     * graphics API call.
     *
     * When overriding this method, you do not need to call the parent method
     * at all. The default implmentation does nothing.
     *
     * @param dt    The amount of time (in seconds) since the last frame
     */
    virtual void update(float dt) override;

    /**
     * The method called to draw the application to the screen.
     *
     * This is part of your core loop and should be replaced with your custom
     * implementation. This method should contain all drawing commands and
     * other uses of the graphics API.
     *
     * When overriding this method, you do not need to call the parent method
     * at all. The default implmentation does nothing.
     */
    virtual void draw() override;

    /*Individual update method for game scene*/
    void updateGameScene(float dt);

};

#endif /* __SCENE_LOADER_H__ */
