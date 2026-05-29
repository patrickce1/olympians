#ifndef __SCENE_LOADER_H__
#define __SCENE_LOADER_H__
#include <cugl/cugl.h>
#include "scenes/GameScene.h"
#include "scenes/ClientScene.h"
#include "scenes/HostSetupScene.h"
#include "scenes/LoadingScene.h"
#include "scenes/MenuScene.h"
#include "scenes/LobbyScene.h"
#include "scenes/HouseSelectScene.h"
#include "scenes/BossSelectScene.h"
#include "scenes/SettingsScene.h"
#include "scenes/WinLoseScene.h"
#include "scenes/PreGameEntryScene.h"
#include "scenes/CodexScene.h"
#include "InputController.h"
#include "AudioController.h"
#include "tests/PlayerTests.h"
#include "tests/EnemyTests.h"
#include "tests/ItemTests.h"
#include "NetworkController.h"
#include <algorithm>
#include <functional>
#include <cugl/core/CUBase.h>
#include <cugl/core/util/CULogger.h>

/**
 * Scene loader class responsible for loading assets and managing scene transitions
 */
class SceneLoader : public cugl::Application
{
protected:
    /* This enum keeps track of which scene/mode we are in right now
     * Will have to be expanded as we add more scenes*/
    enum class State
    {
        LOAD,
        HOSTSETUP,
        CLIENT,
        LOBBY,
        MENU,
        HOUSESELECT,
        BOSSSELECT,
        WINLOSE,
        CODEX,
        PREGAMEENTRY,
        GAME
    };
    
    /**
     * Whether the current scene is paused because settings is open.
     * Used to gate update() calls on the underlying scene.
     */
    bool _paused = false;

    /** The current scene */
    State _currentScene;

    /** The phases of a fade transition between two scenes. */
    enum class TransitionPhase
    {
        NONE,
        FADE_OUT,
        FADE_IN
    };

    /** The current phase of the scene-to-scene fade transition. */
    TransitionPhase _transitionPhase = TransitionPhase::NONE;

    /** Opacity of the black fade overlay (0 = clear, 1 = fully black). */
    float _fadeAlpha = 0.0f;

    /** The scene swap to run once the screen is fully black. */
    std::function<void()> _pendingSwitch;

    /** The loaders to (synchronously) load in assets */
    std::shared_ptr<cugl::AssetManager> _assets;

    /** A 3152 style SpriteBatch to render the scene MOST LIKELY NEEDS CHANGING, I THINK WE'RE NOT SUPPOSED TO USE THIS METHOD? */
    std::shared_ptr<cugl::graphics::SpriteBatch> _batch;

    /** A logger for debugging, can be removed if we feel like this is not necessary */
    std::shared_ptr<cugl::Logger> _logger;

    /*Input controller. Used to extract input data*/
    InputController _input;

    /*Audio controller. Used to manage all audio playback*/
    AudioController _audio;

    /*Network controller used across scenes. Used for recieving and processing networking messages*/
    std::shared_ptr<NetworkController> _network;

    /* All the scenes in the game*/
    /*The opening scene players see while the game loads*/
    std::shared_ptr<AppLoadingScene> _loadingScene;

    /*The main menu screen*/
    MenuScene _menuScene;

    /*The scene where the game takes place*/
    GameScene _gameScene;

    /*The scene players get when they hit "join game".
     *Allows players to join a room and set their username*/
    ClientScene _clientScene;

    /*The scene where the host sets up the lobby
     *Allows host to set the boss for the play session*/
    HostSetupScene _hostSetupScene;

    /*The scene where all joined players are displayed, with the boss in the middle*/
    LobbyScene _lobbyScene;

    /*The scene where the player choose what house they want to represent*/
    HouseSelectScene _houseSelectScene;

    /*The scene where the host changes what boss they want to play with and where other player can view all the different bosses */
    BossSelectScene _bossSelectScene;
    
    /*The persistent settings overlay, shown on top of any active scene*/
    SettingsScene _settingsScene;

    /*The scene where the players learn whether they won or lost */
    WinLoseScene _winLoseScene;
    
    /*The scene where the players learn more about the items */
    CodexScene _codexScene;

    /*The scene where the players see the final choice of house and wait to enter the game scene. */
    PreGameEntryScene _preGameEntryScene;

    // more scenes to come...

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

    /**
     * Selects and applies the appropriate boss theme music for the given game scene.
     *
     * This function determines which boss is active in the provided GameScene
     * and triggers the corresponding audio track using the AudioController.
     * It should be called whenever a boss is chosen or when entering gameplay
     * to ensure the correct theme is playing.
     *
     * @param scene    The GameScene instance containing the current boss context
     */
    void selectBossTheme(GameScene& scene);

    /**
     * Begins a fade transition into another scene.
     *
     * The screen fades to black, then `applySwitch` performs the actual scene
     * swap while hidden, then the new scene fades back in. Any call made while
     * a transition is already running is ignored, so it is safe to invoke this
     * every frame that a scene keeps reporting the same status.
     *
     * @param applySwitch  The scene-swap logic to run while the screen is black
     */
    void requestTransition(std::function<void()> applySwitch);

    /**
    * Advances the active fade transition. Will fade out visually if the current _transitionPhase is TransitionPhase::FADE_OUT and will fade in if  TransitionPhase::FADE_IN
    *
    * @param dt The time (in seconds) since the last frame
    */
    void updateTransition(float dt);
};

#endif /* __SCENE_LOADER_H__ */
