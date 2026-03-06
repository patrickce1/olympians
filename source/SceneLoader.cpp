//Implementation of SceneLoader
#include "SceneLoader.h"

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
    Input::activate<Keyboard>();
    Input::activate<TextInput>();


    _loadingScene = scene2::LoadingScene::alloc(_assets, "json/assets.json");
    _loadingScene->setSpriteBatch(_batch);
    _loadingScene->setActive(true);
    _loadingScene->start();
    _currentScene = State::LOAD;
    
    // Build the scene from these assets
    Application::onStartup();

    // in SceneLoader::onStartup(), just to verify zones fire
    _input.init(); //The input controller starts.
    
    _input.setActive(true); //We can actually tap.
    
    CULog("Input is active: %d", _input.isActive());

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
    
    // ── Run unit tests ──────────────────────────
//    PlayerTests::runAll(
//        "json/characters.json",
//        "json/items.json",
//        "json/enemies.json",
//        "json/playerAI.json"
//    );
//
//    EnemyTests::runAll(
//        "json/enemies.json",
//        "json/characters.json"
//    );

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
    _clientScene.dispose();
    _hostSetupScene.dispose();
    _menuScene.dispose();
    _lobbyScene.dispose();
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
    Input::deactivate<TextInput>();
    Input::deactivate<Keyboard>();
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
                CULog("Assets finished loading. Initializing MenuScene...");

                if (_menuScene.init(_assets)) {
                    _menuScene.setSpriteBatch(_batch);
                    _menuScene.setActive(true);
                    _loadingScene->setActive(false);
                    _currentScene = State::MENU;
                } else {
                    CULog("Failed to initialize MenuScene");
                }
                
                if (_hostSetupScene.init(_assets)) {
                    _hostSetupScene.setSpriteBatch(_batch);
                } else {
                    CULog("Failed to initialize HostSetupScene");
                }
                
                if (_clientScene.init(_assets)) {
                    _clientScene.setSpriteBatch(_batch);
                } else {
                    CULog("Failed to initialize ClientScene");
                }
                
                if (_gameScene.init(_assets)) {
                    _gameScene.setSpriteBatch(_batch);
                } else {
                    CULog("Failed to initialize GameScene");
                }
                if (_lobbyScene.init(_assets)) {
                    _lobbyScene.setSpriteBatch(_batch);
                } else {
                    CULog("Failed to initialize LobbyScene");
                }
            }

            break;
        case State::MENU:
            _menuScene.update(dt);
            switch (_menuScene.consumeAction()) {
                case MenuScene::Action::START_GAME:
                    CULog("Transitioning to HostSetupScene...");
                    _hostSetupScene.setActive(true);
                    _menuScene.setActive(false);
                    _currentScene = State::HOSTSETUP;
                    break;
                case MenuScene::Action::OPEN_SETTINGS:
                    CULog("SettingsScene placeholder pressed");
                    break;
                case MenuScene::Action::JOIN_GAME:
                    CULog("Transitioning to ClientScene...");
                    _clientScene.setActive(true);
                    _menuScene.setActive(false);
                    _currentScene = State::CLIENT;
                    break;
                case MenuScene::Action::NONE:
                default:
                    break;
            }
            break;
        case State::CLIENT:
            _clientScene.update(dt);
            switch (_clientScene.getStatus()) {
                case ClientScene::Status::START:
                    CULog("Transitioning to LobbyScene...");
                    _lobbyScene.setActive(true);
                    _clientScene.setActive(false);
                    _currentScene = State::LOBBY;
                    break;
                case ClientScene::Status::ABORT:
                    CULog("Transitioning to MenuScene...");
                    _menuScene.setActive(true);
                    _clientScene.setActive(false);
                    _currentScene = State::MENU;
                    break;
                default:
                    break;
            }
            break;
        case State::HOSTSETUP:
            _hostSetupScene.update(dt);
            switch (_hostSetupScene.getStatus()) {
                case HostSetupScene::Status::START:
                    CULog("Transitioning to LobbyScene...");
                    _lobbyScene.setActive(true);
                    _hostSetupScene.setActive(false);
                    _currentScene = State::LOBBY;
                    break;
                case HostSetupScene::Status::ABORT:
                    CULog("Transitioning to MenuScene...");
                    _menuScene.setActive(true);
                    _hostSetupScene.setActive(false);
                    _currentScene = State::MENU;
                    break;
                default:
                    break;
            }
            break;
        case State::LOBBY:
            _lobbyScene.update(dt);
            switch (_lobbyScene.getStatus()) {
                case LobbyScene::Status::START:
                    CULog("Transitioning to GameScene...");
                    _gameScene.setActive(true);
                    _lobbyScene.setActive(false);
                    _currentScene = State::GAME;
                    break;
                case LobbyScene::Status::ABORT:
                    CULog("Transitioning to MenuScene...");
                    _menuScene.setActive(true);
                    _lobbyScene.setActive(false);
                    _currentScene = State::MENU;
                    break;
                default:
                    break;;
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
            _gameScene.update(dt, _input);
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
        case State::HOSTSETUP:
            _hostSetupScene.render();
            break;
        case State::CLIENT:
            _clientScene.render();
            break;
        case State::LOBBY:
            _lobbyScene.render();
            break;
        case State::MENU:
            _menuScene.render();
            break;
        case State::GAME:
            _gameScene.render();
            break;
    }
}


void SceneLoader::updateGameScene(float dt) {
    _gameScene.update(dt, _input);
    //scene switching logic goes here
}
