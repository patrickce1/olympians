//Implementation of SceneLoader

#include "SceneLoader.h"
#include <cugl/core/CUBase.h>
#include <cugl/core/util/CULogger.h>

//  Unit tests 
void runEnemyTests();

// This keeps us from having to write cugl:: all the time
using namespace cugl;
using namespace cugl::scene2;
using namespace cugl::graphics;


/** This is the main application and so we need this macro at the start */
CU_ROOTCLASS(SceneLoader)

// The height is a suggestion, but the width is mandatory
#define GAME_WIDTH 393
#define GAME_HEIGHT 852

/**
 * Creates, but does not initialize, a new application.
 *
 * This constructor is where you set all your configuration values such
 * as the game name, the FPS, and so on. Many of these need to be set
 * before the backend is initialized.
 *
 * With that said, it is unsafe to do anything in this constuctor other than
 * initialize attributes. That is because this constructor is called before
 * the backend is initialized, and so much CUGL API calls will fail. Any
 * initialization that requires access to CUGL must happen in onStartup().
 */
    SceneLoader::SceneLoader() : Application(), _currentScene(State::LOAD) {
    // Pre-launch configuration. Nothing here can be reassigned later.
    setName("Olympians");
    setOrganization("Greek Frog Studios");
    setHighDPI(true);       // A must on mobile devices
    setResizable(true);     // Ignored on mobile
    setVSync(true);         // Generally a good idea to prevent choppiness

    // This one can MAYBE reassigned after launch
    setDisplaySize(GAME_WIDTH, GAME_HEIGHT);

    // This can always be reset
    setFPS(120.0);
}

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
void SceneLoader::onStartup() {

    // Create a sprite batch (and background color) to render the scene
    _batch = SpriteBatch::alloc();
//    setClearColor(Color4(229, 229, 229, 255));

    // Create an asset manager to load all assets
    _assets = AssetManager::alloc();

    // You have to attach the individual loaders for each asset type
    _assets->attach<Texture>(TextureLoader::alloc()->getHook());
    _assets->attach<Font>(FontLoader::alloc()->getHook());
    _assets->attach<JsonValue>(JsonLoader::alloc()->getHook());
    _assets->attach<WidgetValue>(WidgetLoader::alloc()->getHook());
    _assets->attach<scene2::SceneNode>(Scene2Loader::alloc()->getHook());

    // This reads the given JSON file and uses it to load all other assets
    _assets->loadDirectory("json/loading.json");

    // Activate mouse or touch screen input as appropriate
    // We have to do this BEFORE the scene, because the scene has a button
#if defined (CU_TOUCH_SCREEN)
    Input::activate<Touchscreen>();
#else
    Input::activate<Mouse>();
#endif

    gameScene.init(_assets);
    gameScene.setSpriteBatch(_batch);
    gameScene.setActive(true);

    _loadingScene = scene2::LoadingScene::alloc(_assets, "json/assets.json");
    _loadingScene->setSpriteBatch(_batch);
    _loadingScene->setActive(true);
    _loadingScene->start();
    _currentScene = State::LOAD;
    
    // Build the scene from these assets
    Application::onStartup();

    // in SceneLoader::onStartup(), just to verify zones fire
    _input.init(); //The input controller starts.
    cugl::Rect screen = getDisplayBounds();
    float w = screen.size.width;
    float h = screen.size.height;
    CULog("Screen size: %f x %f", w, h);
    _input.setActive(true); //We can actually tap.
    
    CULog("Is active: %d", _input.isActive());
    _input.setBossZone(cugl::Rect(w * 0.15f, 0, w * 0.7f, h * 0.5f)); // top middle (between ally zones)
    _input.setAllyZoneLeft(cugl::Rect(0, 0, w * 0.15f, h * 0.5f)); // top left 15% strip
    _input.setAllyZoneRight(cugl::Rect(w * 0.85f, 0, w * 0.15f, h * 0.5f)); // top right 15% strip
    _input.setPasssZoneLeft(cugl::Rect(0, h * 0.5f, w * 0.15f, h * 0.5f));   // bottom left %15 strip
    _input.setPassZoneRight(cugl::Rect(w * 0.85f, h * 0.5f, w * 0.15f, h * 0.5f)); // bottom right %15 strip
   
    // Create the logger
    _logger = Logger::open("debug");

    // This guarantees we write to file and screen evently
    _logger->setLogLevel(Logger::Level::INFO_MSG);
    _logger->setConsoleLevel(Logger::Level::INFO_MSG);

    // Report the safe area
    Rect bounds = Display::get()->getSafeBounds();
    _logger->log("Safe Area %sx%s", bounds.origin.toString().c_str(),
        bounds.size.toString().c_str());
    bounds = getSafeBounds();
    _logger->log("Safe Area %sx%s", bounds.origin.toString().c_str(),
        bounds.size.toString().c_str());
    bounds = getDisplayBounds();
    _logger->log("Full Area %sx%s", bounds.origin.toString().c_str(),
        bounds.size.toString().c_str());
}

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
void SceneLoader::onShutdown() {
    _input.dispose();
    _gameScene.dispose();
    _loadingScene = nullptr;
    Logger::close("debug");

    // Delete all smart pointers
    _batch = nullptr;
    _assets = nullptr;

    // Deativate input
#if defined CU_TOUCH_SCREEN
    Input::deactivate<Touchscreen>();
#else
    Input::deactivate<Mouse>();
#endif
    Application::onShutdown();
}

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
void SceneLoader::onResize() {
    // When we resize, we have to resize whichever scene is active
}

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
 * 
 * Scene loader's main job during update is to detect if a switch between scenes is necessary. 
 * Otherwise, it should maintain the current scene.
 */
void SceneLoader::update(float dt) {
    switch (_currentScene) {
        case State::LOAD:
            _loadingScene->update(dt);
            
            if (_loadingScene->isPending()) {
                CULog("Assets finished loading. Initializing GameScene...");

                _gameScene.init(_assets);
                _gameScene.setSpriteBatch(_batch);
                _gameScene.setActive(true);

                _loadingScene->setActive(false);
                _currentScene = State::GAME;
            }
            
            break;
        case State::GAME:
            InputController::Action action = _input.getAction();

                        switch (action) {
                            case InputController::Action::PASS_RIGHT:
                                CULog("[ACTION] PASS_RIGHT");
                                break;
                            case InputController::Action::PASS_LEFT:
                                CULog("[ACTION] PASS_LEFT");
                                break;
                            case InputController::Action::DROP_BOSS:
                                CULog("[ACTION] DROP_BOSS");
                                break;
                            case InputController::Action::DROP_ALLY_LEFT:
                                CULog("[ACTION] DROP_ALLY_LEFT");
                                break;
                            case InputController::Action::DROP_ALLY_RIGHT:
                                CULog("[ACTION] DROP_ALLY_RIGHT");
                                break;
                            default:
                                break;
                        }
            _gameScene.update(dt,_input);
            break;
    }
    _input.resetAction();

}

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
void SceneLoader::draw() {
    // This takes care of begin/end
    switch (_currentScene) {
        case State::LOAD:
            _loadingScene->render();
            break;
        case State::GAME:
            _gameScene.render();
            break;
    }
}


void SceneLoader::updateGameScene(float dt) {
    _gameScene.update(dt,_input);
    //scene switching logic goes here
}
