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
    setClearColor(Color4("#1e1410ff"));

    // Create an asset manager to load all assets
    _assets = AssetManager::alloc();

    _network = std::make_shared<NetworkController>();

    // You have to attach the individual loaders for each asset type
    _assets->attach<Texture>(TextureLoader::alloc()->getHook());
    _assets->attach<Sound>(SoundLoader::alloc()->getHook());
    _assets->attach<Font>(FontLoader::alloc()->getHook());
    _assets->attach<JsonValue>(JsonLoader::alloc()->getHook());
    _assets->attach<WidgetValue>(WidgetLoader::alloc()->getHook());
    _assets->attach<scene2::SceneNode>(Scene2Loader::alloc()->getHook());

    // This reads the given JSON file and uses it to load all other assets
    _assets->loadDirectory("json/scenes/loading.json");

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
    
    //NETWORK
    netcode::NetworkLayer::start(netcode::NetworkLayer::Log::INFO);

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
//   PlayerTests::runAll(
//       "json/houses.json",
//       "json/items.json",
//       "json/enemies.json",
//       "json/playerAI.json"
//   );
//
//   EnemyTests::runAll(
//      "json/enemies.json",
//      "json/houses.json"
//   );
//
//   ItemTests::runAll(
//       "json/items.json",
//       "json/houses.json",
//       "json/enemies.json"
//   );

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
    _houseSelectScene.dispose();
    _bossSelectScene.dispose();
    _preGameEntryScene.dispose();
    _loadingScene = nullptr;
    Logger::close("debug");
    netcode::NetworkLayer::stop();
    _assets->unloadAll();
    _assets->dispose();

    // Delete all smart pointers
    _batch = nullptr;
    _assets = nullptr;
    _network = nullptr;

    // Deativate input
#if defined CU_TOUCH_SCREEN
    Input::deactivate<Touchscreen>();
#else
    Input::deactivate<Mouse>();
#endif
    Input::deactivate<TextInput>();
    Input::deactivate<Keyboard>();
    
    // Dispose audio controller
    _audio.dispose();

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

                //NETWORK
                _network->init(_assets); //assets loaded, load network controller
                
                // Initialize and start audio controller
                if (_audio.init(_assets)) {
                    _audio.startAudioEngine();
                    _audio.playMusic("lobby");
                } else {
                    CULog("Warning: Failed to initialize audio controller");
                }

                
                if (_menuScene.init(_assets)) {
                    _menuScene.setSpriteBatch(_batch);
                    _menuScene.setActive(true);
                    _loadingScene->setActive(false);
                    _currentScene = State::MENU;
                } else {
                    CULog("Failed to initialize MenuScene");
                }
                
                if (_hostSetupScene.init(_assets, _network)) {
                    _hostSetupScene.setSpriteBatch(_batch);
                } else {
                    CULog("Failed to initialize HostSetupScene");
                }
                
                if (_clientScene.init(_assets, _network)) {
                    _clientScene.setSpriteBatch(_batch);
                } else {
                    CULog("Failed to initialize ClientScene");
                }
                
                if (_gameScene.init(_assets, _network, &_audio)) {
                    _gameScene.setSpriteBatch(_batch);
                } else {
                    CULog("Failed to initialize GameScene");
                }
                
                if (_lobbyScene.init(_assets, _network, &_gameScene.getGameState(), &_gameScene.getItemController())) {
                    _lobbyScene.setSpriteBatch(_batch);
                } else {
                    CULog("Failed to initialize LobbyScene");
                }
                
                if (_houseSelectScene.init(_assets, _network, &_gameScene.getGameState())) {
                    _houseSelectScene.setSpriteBatch(_batch);
                } else {
                    CULog("Failed to initialize HouseSelectScene");
                }
                
                if (_bossSelectScene.init(_assets, _network)) {
                    _bossSelectScene.setSpriteBatch(_batch);
                } else {
                    CULog("Failed to initialize BossSelectScene");
                }
                
                if (_preGameEntryScene.init(_assets, _network, &_gameScene.getGameState())) {
                    _preGameEntryScene.setSpriteBatch(_batch);
                } else {
                    CULog("Failed to initialize PreGameEntryScene");
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
                    _audio.playMusic("lobby");
                    _lobbyScene.setActive(true);
                    _clientScene.setActive(false);
                    _currentScene = State::LOBBY;
                    break;
                case ClientScene::Status::HOST:
                    CULog("Transitioning to HostSetupScene...");
                    _hostSetupScene.setActive(true);
                    _clientScene.setActive(false);
                    _currentScene = State::HOSTSETUP;
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
                    _audio.playMusic("lobby");
                    _lobbyScene.setActive(true);
                    _hostSetupScene.setActive(false);
                    _currentScene = State::LOBBY;
                    break;
                case HostSetupScene::Status::CLIENT:
                    CULog("Transitioning to ClientScene...");
                    _clientScene.setActive(true);
                    _hostSetupScene.setActive(false);
                    _currentScene = State::CLIENT;
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
                    CULog("Transitioning to PreGameEntryScene...");
                    _audio.playMusic("lobby");
                    _preGameEntryScene.setActive(true);
                    _lobbyScene.setActive(false);
                    _currentScene = State::PREGAMEENTRY;
                    break;
                case LobbyScene::Status::SELECT:
                    CULog("Transitioning to HouseSelectScene...");
                    _houseSelectScene.setTargetSlot(_lobbyScene.getPendingSlotToBeOpened());
                    _houseSelectScene.setActive(true);
                    _lobbyScene.setActive(false);
                    _currentScene = State::HOUSESELECT;
                    break;
                case LobbyScene::Status::BOSSSELECT:
                    CULog("Transitioning to BossSelectScene...");
                    _bossSelectScene.setActive(true);
                    _lobbyScene.setActive(false);
                    _currentScene = State::BOSSSELECT;
                    break;
                case LobbyScene::Status::ABORT:
                    _gameScene.resetGameState();
                    _houseSelectScene.setPendingReset(true);
                    if (_network->isHost()) {
                        CULog("Host backed out of lobby — returning to HostSetupScene...");
                        _hostSetupScene.setActive(true);
                        _lobbyScene.setActive(false);
                        _currentScene = State::HOSTSETUP;
                    } else {
                        // Client voluntarily left — preserve game ID so they don't retype it.
                        CULog("Client backed out of lobby — returning to ClientScene...");
                        _clientScene.setActive(true, true); // preserveGameId = true
                        _lobbyScene.setActive(false);
                        _currentScene = State::CLIENT;
                    }
                    break;
                // Host broadcast SESSION_TERMINATED
                case LobbyScene::Status::HOST_LEFT:
                    CULog("Host left lobby — returning client to HostSetupScene...");
                    _gameScene.resetGameState();
                    _houseSelectScene.setPendingReset(true);
                    _lobbyScene.setActive(false);
                    _hostSetupScene.setActive(true);
                    _hostSetupScene.showHostDisconnectedError();
                    _currentScene = State::HOSTSETUP;
                    break;
                //Host unexpectedly disconnected
                case LobbyScene::Status::HOST_DISCONNECTED:
                    CULog("Host disconnected in lobby — returning client to HostSetupScene...");
                    _gameScene.resetGameState();
                    _houseSelectScene.setPendingReset(true);
                    _lobbyScene.setActive(false);
                    _hostSetupScene.setActive(true);
                    _hostSetupScene.showHostDisconnectedError();
                    _currentScene = State::HOSTSETUP;
                    break;
                default:
                    break;;
            }
            break;
        case State::HOUSESELECT:
            _houseSelectScene.update(dt);
            switch (_houseSelectScene.getStatus()) {
                case HouseSelectScene::Status::PRE_GAMESCENE_START:
                    CULog("Transitioning to PreGameScene from HouseSelect...");
                    _audio.playMusic("battle");
                    _preGameEntryScene.setActive(true);
                    _houseSelectScene.setActive(false);
                    _currentScene = State::PREGAMEENTRY;
                    break;
                case HouseSelectScene::Status::ABORT:
                    if (_network->checkConnection() != NetworkController::Status::CONNECTED) {
                        _gameScene.resetGameState();
                        _houseSelectScene.setPendingReset(true);
                        _hostSetupScene.setActive(true);
                        _houseSelectScene.setActive(false);
                        _currentScene = State::HOSTSETUP;
                    } else {
                        _lobbyScene.setActive(true);
                        _houseSelectScene.setActive(false);
                        _currentScene = State::LOBBY;
                    }
                    break;
                case HouseSelectScene::Status::HOST_DISCONNECTED:
                    CULog("Host disconnected in HouseSelect — returning to HostSetupScene...");
                    _gameScene.resetGameState();
                    _houseSelectScene.setPendingReset(true);
                    _houseSelectScene.setActive(false);
                    _hostSetupScene.setActive(true);
                    _hostSetupScene.showHostDisconnectedError();
                    _currentScene = State::HOSTSETUP;
                    break;
                default:
                    break;
            }
            break;
        case State::BOSSSELECT:
            _bossSelectScene.update(dt);
            switch (_bossSelectScene.getStatus()) {
                case BossSelectScene::Status::PRE_GAMESCENE_START:
                    CULog("Transitioning to PreGameScene from BossSelect...");
                    _audio.playMusic("battle");
                    _preGameEntryScene.setActive(true);
                    _bossSelectScene.setActive(false);
                    _currentScene = State::PREGAMEENTRY;
                    break;
                case BossSelectScene::Status::ABORT:
                    if (_network->checkConnection() != NetworkController::Status::CONNECTED) {
                        _gameScene.resetGameState();
                        _houseSelectScene.setPendingReset(true);
                        _hostSetupScene.setActive(true);
                        _bossSelectScene.setActive(false);
                        _currentScene = State::HOSTSETUP;
                    } else {
                        _lobbyScene.setActive(true);
                        _bossSelectScene.setActive(false);
                        _currentScene = State::LOBBY;
                    }
                    break;
                default:
                    break;
            }
            break;
        case State::PREGAMEENTRY:
            _preGameEntryScene.update(dt);
            switch (_preGameEntryScene.getStatus()) {
                case PreGameEntryScene::Status::START:
                    CULog("Transitioning to GameScene from PreGameEntryScene...");
                    _audio.playMusic("battle");
                    _gameScene.setActive(true);
                    _preGameEntryScene.setActive(false);
                    _currentScene = State::GAME;
                    break;
                case PreGameEntryScene::Status::PLAYER_DISCONNECTED:
                    CULog("Player disconnected in PreGameEntry — returning to LobbyScene...");
                    _lobbyScene.setDisconnectBanner(
                        _preGameEntryScene.getDisconnectMessage());
                    _lobbyScene.setActive(true);
                    _preGameEntryScene.setActive(false);
                    _currentScene = State::LOBBY;
                    break;
                case PreGameEntryScene::Status::ABORT:
                    CULog("Transitioning to LobbyScene from PreGameEntryScene...");
                    _lobbyScene.setActive(true);
                    _preGameEntryScene.setActive(false);
                    _currentScene = State::LOBBY;
                    break;
                case PreGameEntryScene::Status::HOST_DISCONNECTED:
                    CULog("Host disconnected in PreGameEntry — returning client to HostSetupScene...");
                    _audio.playMusic("lobby");
                    _gameScene.resetGameState();
                    _houseSelectScene.setPendingReset(true);
                    _preGameEntryScene.setActive(false);
                    _hostSetupScene.setActive(true);
                    _hostSetupScene.showHostDisconnectedError();
                    _currentScene = State::HOSTSETUP;
                    break;
                default:
                    break;
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
            //check if we won or lost and return to lobby if we did
            //this will be changed to a proper win/lose scene later
            switch (_gameScene.getStatus()) {
                case GameScene::Status::LOST:
                    _audio.playMusic("lobby");
                    _lobbyScene.setActive(true);
                    _gameScene.setActive(false);
                    _currentScene = State::LOBBY;
                    _gameScene.reset();
                    break;
                case GameScene::Status::WON:
                    _audio.playMusic("lobby");
                    _lobbyScene.setActive(true);
                    _gameScene.setActive(false);
                    _currentScene = State::LOBBY;
                    _gameScene.reset();
                    break;
                case GameScene::Status::PLAYING:
                    break;
                case GameScene::Status::HOST_DISCONNECTED:
                    CULog("Host disconnected in game — returning client to HostSetupScene...");
                    _audio.playMusic("lobby");
                    _gameScene.setActive(false);
                    _gameScene.reset();
                    _hostSetupScene.setActive(true);
                    _hostSetupScene.showHostDisconnectedError();
                    _currentScene = State::HOSTSETUP;
                    break;
                }
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
        case State::HOUSESELECT:
            _houseSelectScene.render();
            break;
        case State::BOSSSELECT:
            _bossSelectScene.render();
            break;
        case State::PREGAMEENTRY:
            _preGameEntryScene.render();
            break;
    }
}


void SceneLoader::updateGameScene(float dt) {
    _gameScene.update(dt, _input);
    //scene switching logic goes here
}
