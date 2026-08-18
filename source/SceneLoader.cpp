// Implementation of SceneLoader
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

/** Duration (in seconds) of each half (fade-out and fade-in) of a scene transition */
#define FADE_DURATION 0.22f

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
SceneLoader::SceneLoader() : Application(), _currentScene(State::LOAD)
{
    // Pre-launch configuration. Nothing here can be reassigned later.
    setName("Olympians");
    setOrganization("Greek Frog Studios");
    setHighDPI(true);   // A must on mobile devices
    setResizable(true); // Ignored on mobile
    setVSync(true);     // Generally a good idea to prevent choppiness

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
void SceneLoader::onStartup()
{

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
    _assets->loadDirectory("json/itemTextures.json");
    _assets->loadDirectory("json/houseInGameIcons.json");

    // Activate mouse or touch screen input as appropriate
    // We have to do this BEFORE the scene, because the scene has a button
#if defined(CU_TOUCH_SCREEN)
    Input::activate<Touchscreen>();
#else
    Input::activate<Mouse>();
#endif
    Input::activate<Keyboard>();
    Input::activate<TextInput>();

    _loadingScene = AppLoadingScene::alloc(_assets, "json/assets.json");
    _loadingScene->setSpriteBatch(_batch);
    _loadingScene->setActive(true);
    _loadingScene->start();
    _currentScene = State::LOAD;

    // Build the scene from these assets
    Application::onStartup();

    // NETWORK
    netcode::NetworkLayer::start(netcode::NetworkLayer::Log::INFO);

    // in SceneLoader::onStartup(), just to verify zones fire

    _input.init(); // The input controller starts.

    _input.setActive(true); // We can actually tap.

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
void SceneLoader::onShutdown()
{
    _input.dispose();
    _gameScene.dispose();
    _clientScene.dispose();
    _hostSetupScene.dispose();
    _menuScene.dispose();
    _lobbyScene.dispose();
    _houseSelectScene.dispose();
    _bossSelectScene.dispose();
    _winLoseScene.dispose();
    _codexScene.dispose();
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
void SceneLoader::onResize()
{
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
    updateTransition(dt);

    if (_transitionPhase != TransitionPhase::NONE) {
        _input.resetAction();
        return;
    }

    // Settings overlay always gets updated when active
    if (_settingsScene.isActive()) {
        _settingsScene.update(dt);
        if (_settingsScene.shouldClose()) {
            _settingsScene.setActive(false);
            _paused = false;
            if (_settingsScene.shouldStartTutorial()) {
                _gameScene.setForceTutorial();
                _lobbyScene.setForceTutorial();
                _hostSetupScene.setPendingTutorialStart();
                _currentScene = State::HOSTSETUP;
            } else {
                switch (_currentScene) {
                    case State::HOSTSETUP:
                        _hostSetupScene.setInputEnabled(true);
                        break;
                    case State::CLIENT:
                        _clientScene.setInputEnabled(true);
                        break;
                    case State::LOBBY:
                        _lobbyScene.setInputEnabled(true);
                        break;
                    case State::MENU:
                        _menuScene.setActive(false);
                        _menuScene.setActive(true);
                        break;
                    default:
                        break;
                }
            }
        }
        return;
    }
    
    switch (_currentScene) {
        case State::LOAD:
            _loadingScene->update(dt);
            if (_loadingScene->isComplete())
            {
                requestTransition([this]() {
                CULog("Assets finished loading. Initializing MenuScene...");

            // NETWORK
            _network->init(_assets); // assets loaded, load network controller
            _network->loadHouseUtilityRatings(_assets);
                
            // Load persisted player data before any scene is initialized so
            // MenuScene can check hasPlayerName() on first activation
            SavedDataManager::get().load();
            CULog("SavedDataManager: musicVolume=%.2f sfxVolume=%.2f",
                      SavedDataManager::get().getMusicVolume(),
                      SavedDataManager::get().getSFXVolume());
                
            // Apply persisted volume levels to the audio controller immediately
            if (_audio.init(_assets)) {
                _audio.startAudioEngine();
                _audio.setMusicVolumeMultiplier(SavedDataManager::get().getMusicVolume());
                _audio.setSFXVolumeMultiplier(SavedDataManager::get().getSFXVolume());
            } else{
                CULog("Warning: Failed to initialize audio controller");
            }
                
            CULog("SceneLoader: SavedDataManager loaded, playerName='%s'",
                  SavedDataManager::get().getPlayerName().c_str());

            if (_menuScene.init(_assets, &_audio)){
                _menuScene.setSpriteBatch(_batch);
                _menuScene.setActive(true);
                _loadingScene->setActive(false);
                _currentScene = State::MENU;
            } else{
                CULog("Failed to initialize MenuScene");
            }

            if (_hostSetupScene.init(_assets, _network, &_audio)){
                _hostSetupScene.setSpriteBatch(_batch);
            } else{
                CULog("Failed to initialize HostSetupScene");
            }

            if (_clientScene.init(_assets, _network, &_audio)){
                _clientScene.setSpriteBatch(_batch);
            } else{
                CULog("Failed to initialize ClientScene");
            }

            if (_gameScene.init(_assets, _network, &_audio)){
                _gameScene.setSpriteBatch(_batch);
            } else{
                CULog("Failed to initialize GameScene");
            }

            if (_lobbyScene.init(_assets, _network, &_gameScene.getGameState(), &_gameScene.getItemController(), &_audio)){
                _lobbyScene.setSpriteBatch(_batch);
            } else{
                CULog("Failed to initialize LobbyScene");
            }

            if (_houseSelectScene.init(_assets, _network, &_gameScene.getGameState(), &_audio)){
                _houseSelectScene.setSpriteBatch(_batch);
            } else{
                CULog("Failed to initialize HouseSelectScene");
            }

            if (_bossSelectScene.init(_assets, _network, &_audio)){
                _bossSelectScene.setSpriteBatch(_batch);
            } else{
                CULog("Failed to initialize BossSelectScene");
            }

            if (_winLoseScene.init(_assets, _network)){
                _winLoseScene.setSpriteBatch(_batch);
            } else{
                CULog("Failed to initialize BossSelectScene");
            }
            
            if (_codexScene.init(_assets, _network, &_audio)) {
                _codexScene.setSpriteBatch(_batch);
            } else {
                CULog("Failed to initialize CodexScene");
            }

            if (_preGameEntryScene.init(_assets, _network, &_gameScene.getGameState())){
                _preGameEntryScene.setSpriteBatch(_batch);
            } else{
                CULog("Failed to initialize PreGameEntryScene");
            }
            
            // Init the settings overlay once, after all assets are ready
            if (_settingsScene.init(_assets, &_audio)){
                _settingsScene.setSpriteBatch(_batch);
                
                _settingsScene.setOnMusicVolumeChange([this](float value) {
                    _audio.setMusicVolumeMultiplier(value);
                });

                _settingsScene.setOnSFXVolumeChange([this](float value) {
                    _audio.setSFXVolumeMultiplier(value);
                });
            }
                
            // Set sliders to match saved values without triggering callbacks
            _settingsScene.initSliderValues(
                SavedDataManager::get().getMusicVolume(),
                SavedDataManager::get().getSFXVolume()
            );
            _audio.playMusic("lobby");
                });
        }
        break;
    case State::MENU:
        _menuScene.update(dt);
        switch (_menuScene.getStatus()) {
            case MenuScene::Status::START_GAME:
                requestTransition([this]() {
                CULog("Transitioning to HostSetupScene...");
                _hostSetupScene.setActive(true);
                _menuScene.setActive(false);
                _menuScene.resetStatus();
                _currentScene = State::HOSTSETUP;
                });
                break;
            case MenuScene::Status::OPEN_SETTINGS:
                CULog("MenuScene: opening settings");
                _paused = true;
                _settingsScene.setActive(true);
                _menuScene.resetStatus();
                break;
            case MenuScene::Status::NAME_ONBOARDING:
            case MenuScene::Status::PENDING_ONBOARDING:
            case MenuScene::Status::PENDING_SAVE:
            case MenuScene::Status::NONE:
                break;
            default:
                break;
        }
        break;
    case State::CLIENT:
        _clientScene.update(dt);
        if (_clientScene.shouldOpenSettings()) {
            _clientScene.setInputEnabled(false);
            _settingsScene.setActive(true);
        }
        switch (_clientScene.getStatus()){
            case ClientScene::Status::START:
                requestTransition([this]() {
                CULog("Transitioning to LobbyScene...");
                _audio.playMusic("lobby");
                _lobbyScene.setActive(true);
                _clientScene.setActive(false);
                _currentScene = State::LOBBY;
                });
                break;
            case ClientScene::Status::HOST:
                CULog("Transitioning to HostSetupScene...");
                _hostSetupScene.setActive(true);
                _clientScene.setActive(false);
                _currentScene = State::HOSTSETUP;
                break;
            case ClientScene::Status::ABORT:
                requestTransition([this]() {
                CULog("Transitioning to MenuScene...");
                _menuScene.setActive(true);
                _clientScene.setActive(false);
                _currentScene = State::MENU;
                });
                break;
            default:
                break;
        }
        break;
    case State::HOSTSETUP:
        _hostSetupScene.update(dt, _input);
        if (_hostSetupScene.shouldOpenSettings()) {
            _hostSetupScene.setInputEnabled(false);
            _settingsScene.setActive(true);
        }
        switch (_hostSetupScene.getStatus()) {
            case HostSetupScene::Status::START:
                requestTransition([this]() {
                CULog("Transitioning to LobbyScene...");
                _audio.playMusic("lobby");
                _lobbyScene.setActive(true);
                _hostSetupScene.setActive(false);
                _currentScene = State::LOBBY;
                });
                break;
            case HostSetupScene::Status::CLIENT:
                CULog("Transitioning to ClientScene...");
                _clientScene.setActive(true);
                _hostSetupScene.setActive(false);
                _currentScene = State::CLIENT;
                break;
            case HostSetupScene::Status::ABORT:
                requestTransition([this]() {
                CULog("Transitioning to MenuScene...");
                _menuScene.setActive(true);
                _hostSetupScene.setActive(false);
                _currentScene = State::MENU;
                });
                break;
            default:
                break;
        }
        break;
    case State::LOBBY:
        _lobbyScene.update(dt, _input);
        switch (_lobbyScene.getStatus())
        {
        case LobbyScene::Status::PRE_GAME_START:
            CULog("Transitioning to PreGameEntryScene...");
            _audio.playMusic("cyclops_theme");
            _preGameEntryScene.setActive(true);
            _lobbyScene.setActive(false);
            _currentScene = State::PREGAMEENTRY;
            break;
        case LobbyScene::Status::GAME_START:
            requestTransition([this]() {
            CULog("Transitioning directly to GameScene from Lobby — host already in game...");
            _gameScene.setActive(true);
            _audio.playMusic("cyclops_theme");
            _lobbyScene.setActive(false);
            _currentScene = State::GAME;
            });
            break;
        case LobbyScene::Status::SELECT:
            requestTransition([this]() {
            CULog("Transitioning to HouseSelectScene...");
            _houseSelectScene.setTargetSlot(_lobbyScene.getPendingSlotToBeOpened());
            _houseSelectScene.setActive(true);
            _lobbyScene.setActive(false);
            _currentScene = State::HOUSESELECT;
            });
            break;
        case LobbyScene::Status::BOSSSELECT:
            requestTransition([this]() {
            CULog("Transitioning to BossSelectScene...");
            _bossSelectScene.setActive(true);
            _lobbyScene.setActive(false);
            _currentScene = State::BOSSSELECT;
            });
            break;
        case LobbyScene::Status::CODEX:
            requestTransition([this]() {
            CULog("Transitioning to CodexScene...");
            _codexScene.setActive(true);
            _lobbyScene.setActive(false);
            _currentScene = State::CODEX;
            });
            break;
        case LobbyScene::Status::ABORT:
            requestTransition([this]() {
            _gameScene.resetGameState();
            _houseSelectScene.setPendingReset(true);
            if (_network->isHost())
            {
                CULog("Host backed out of lobby — returning to HostSetupScene...");
                _hostSetupScene.setActive(true);
                _lobbyScene.setActive(false);
                _currentScene = State::HOSTSETUP;
            }
            else
            {
                // Client voluntarily left — preserve game ID so they don't retype it.
                CULog("Client backed out of lobby — returning to ClientScene...");
                _clientScene.setActive(true, true); // preserveGameId = true
                _lobbyScene.setActive(false);
                _currentScene = State::CLIENT;
            }
            });
            break;
        // Host broadcast SESSION_TERMINATED
        case LobbyScene::Status::HOST_LEFT:
            requestTransition([this]() {
            CULog("Host left lobby — returning client to HostSetupScene...");
            _gameScene.resetGameState();
            _houseSelectScene.setPendingReset(true);
            _lobbyScene.setActive(false);
            _hostSetupScene.setActive(true);
            _hostSetupScene.showHostDisconnectedError();
            _currentScene = State::HOSTSETUP;
            });
            break;
        // Host unexpectedly disconnected
        case LobbyScene::Status::HOST_DISCONNECTED:
            requestTransition([this]() {
            CULog("Host disconnected in lobby — returning client to HostSetupScene...");
            _gameScene.resetGameState();
            _houseSelectScene.setPendingReset(true);
            _lobbyScene.setActive(false);
            _hostSetupScene.setActive(true);
            _hostSetupScene.showHostDisconnectedError();
            _currentScene = State::HOSTSETUP;
            });
            break;
        default:
            break;
            ;
        }
        break;
    case State::HOUSESELECT:
        _houseSelectScene.update(dt, _input);
        switch (_houseSelectScene.getStatus())
        {
        case HouseSelectScene::Status::PRE_GAMESCENE_START:
            CULog("Transitioning to PreGameScene from HouseSelect...");
            _audio.playMusic("cyclops_theme");
            _preGameEntryScene.setActive(true);
            _houseSelectScene.setActive(false);
            _currentScene = State::PREGAMEENTRY;
            break;
        case HouseSelectScene::Status::ABORT:
            requestTransition([this]() {
            if (_network->checkConnection() != NetworkController::Status::CONNECTED)
            {
                _gameScene.resetGameState();
                _houseSelectScene.setPendingReset(true);
                _hostSetupScene.setActive(true);
                _hostSetupScene.showHostDisconnectedError();
                _houseSelectScene.setActive(false);
                _currentScene = State::HOSTSETUP;
            }
            else
            {
                _lobbyScene.setActive(true);
                _houseSelectScene.setActive(false);
                _currentScene = State::LOBBY;
            }
            });
            break;
        case HouseSelectScene::Status::HOST_DISCONNECTED:
            requestTransition([this]() {
            CULog("Host disconnected in HouseSelect — returning to HostSetupScene...");
            _gameScene.resetGameState();
            _houseSelectScene.setPendingReset(true);
            _houseSelectScene.setActive(false);
            _hostSetupScene.setActive(true);
            _hostSetupScene.showHostDisconnectedError();
            _currentScene = State::HOSTSETUP;
            });
            break;
        default:
            break;
        }
        break;
    case State::BOSSSELECT:
        _bossSelectScene.update(dt, _input);
        switch (_bossSelectScene.getStatus())
        {
        case BossSelectScene::Status::PRE_GAMESCENE_START:
            CULog("Transitioning to PreGameScene from BossSelect...");
            _audio.playMusic("cyclops_theme");
            _preGameEntryScene.setActive(true);
            _bossSelectScene.setActive(false);
            _currentScene = State::PREGAMEENTRY;
            break;
        case BossSelectScene::Status::ABORT:
            requestTransition([this]() {
            if (_network->checkConnection() != NetworkController::Status::CONNECTED)
            {
                _gameScene.resetGameState();
                _houseSelectScene.setPendingReset(true);
                _hostSetupScene.setActive(true);
                _hostSetupScene.showHostDisconnectedError();
                _bossSelectScene.setActive(false);
                _currentScene = State::HOSTSETUP;
            }
            else
            {
                _lobbyScene.setActive(true);
                _bossSelectScene.setActive(false);
                _currentScene = State::LOBBY;
            }
            });
            break;
        default:
            break;
        }
        break;
    case State::WINLOSE:
        _winLoseScene.update(dt);
        switch (_winLoseScene.getStatus())
        {
        case WinLoseScene::Status::ABORT:
            requestTransition([this]() {
            _audio.playMusic("lobby");
            if (_network->getEnemy() == "circe" && !SavedDataManager::get().getTutorialCompleted()) { //Tutorial should go back to the setup screen.
                _network->disconnect();
                SavedDataManager::get().setTutorialCompleted(true);
                SavedDataManager::get().save();
                _hostSetupScene.setPendingTutorialCompletePopup();
                _hostSetupScene.setActive(true);
                _currentScene = State::HOSTSETUP;
            } else {
                _lobbyScene.setActive(true);
                _currentScene = State::LOBBY;
            }
            _winLoseScene.setActive(false);
            });
            break;
        case WinLoseScene::Status::PRE_GAMESCENE_START:
            CULog("Transitioning to PreGameScene from WinLoseScene...");
            _audio.playMusic("cyclops_theme");
            _preGameEntryScene.setActive(true);
            _winLoseScene.setActive(false);
            _currentScene = State::PREGAMEENTRY;
            break;
        default:
            break;
        }
        break;
            
    case State::CODEX:
        _codexScene.update(dt, _input);
        switch (_codexScene.getStatus())
        {
        case CodexScene::Status::PRE_GAMESCENE_START:
            CULog("Transitioning to PreGameScene from CodexScene...");
            _audio.playMusic("cyclops_theme");
            _preGameEntryScene.setActive(true);
            _codexScene.setActive(false);
            _currentScene = State::PREGAMEENTRY;
            break;
        case CodexScene::Status::ABORT:
            requestTransition([this]() {
            if (_network->checkConnection() != NetworkController::Status::CONNECTED)
            {
                _gameScene.resetGameState();
                _houseSelectScene.setPendingReset(true);
                _hostSetupScene.setActive(true);
                _hostSetupScene.showHostDisconnectedError();
                _codexScene.setActive(false);
                _currentScene = State::HOSTSETUP;
            }
            else
            {
                _lobbyScene.setActive(true);
                _codexScene.setActive(false);
                _currentScene = State::LOBBY;
            }
            });
            break;
        default:
            break;
        }
        break;
            
    case State::PREGAMEENTRY:
        _preGameEntryScene.update(dt);
        // Build the selected boss's dynamic textures and sprites while the
        // pre-game screen is visible.  These CUGL calls must run on the main
        // thread, but moving them here prevents a load hitch at game entry.
        if (_preGameEntryScene.isReadyToLoadGame()) {
            _preGameEntryScene.setLoadingProgress(
                _gameScene.preloadEnemyAnimations());
        }
        switch (_preGameEntryScene.getStatus())
        {
        case PreGameEntryScene::Status::START:
            requestTransition([this]() {
            CULog("Transitioning to GameScene from PreGameEntryScene...");
            _gameScene.setActive(true);
            selectBossTheme(_gameScene);
            _preGameEntryScene.setActive(false);
            _currentScene = State::GAME;
            });
            break;
        case PreGameEntryScene::Status::PLAYER_DISCONNECTED:
            requestTransition([this]() {
            CULog("Player disconnected in PreGameEntry — returning to LobbyScene...");
            _gameScene.discardPreloadedEnemyAnimations();
            _audio.playMusic("lobby");
            _lobbyScene.setDisconnectBanner(
                _preGameEntryScene.getDisconnectMessage());
            _lobbyScene.setActive(true);
            _preGameEntryScene.setActive(false);
            _currentScene = State::LOBBY;
            });
            break;
        case PreGameEntryScene::Status::ABORT:
            requestTransition([this]() {
            CULog("Transitioning to LobbyScene from PreGameEntryScene...");
            _gameScene.discardPreloadedEnemyAnimations();
            _lobbyScene.setActive(true);
            _preGameEntryScene.setActive(false);
            _currentScene = State::LOBBY;
            });
            break;
        case PreGameEntryScene::Status::HOST_DISCONNECTED:
            requestTransition([this]() {
            CULog("Host disconnected in PreGameEntry — returning client to HostSetupScene...");
            _audio.playMusic("lobby");
            _gameScene.discardPreloadedEnemyAnimations();
            _gameScene.resetGameState();
            _houseSelectScene.setPendingReset(true);
            _preGameEntryScene.setActive(false);
            _hostSetupScene.setActive(true);
            _hostSetupScene.showHostDisconnectedError();
            _currentScene = State::HOSTSETUP;
            });
            break;
        default:
            break;
        }
        break;
    case State::GAME:
        InputController::Action action = _input.getAction();
        switch (action)
        {
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
        // check if we won or lost and return to lobby if we did
        // this will be changed to a proper win/lose scene later
        switch (_gameScene.getStatus())
        {
        case GameScene::Status::LOST:
            requestTransition([this]() {
            _audio.playMusic("lobby");
            _winLoseScene.setDidWin(false);
            _winLoseScene.captureStats();
            _winLoseScene.setActive(true);
            _gameScene.setActive(false);
            _currentScene = State::WINLOSE;
            _gameScene.reset();
            });
            break;
        case GameScene::Status::WON:
            requestTransition([this]() {
            _audio.playMusic("lobby");
            _winLoseScene.setDidWin(true);
            _winLoseScene.captureStats();
            _winLoseScene.setActive(true);
            _gameScene.setActive(false);
            _currentScene = State::WINLOSE;
            _gameScene.reset();
            });
            break;
        case GameScene::Status::PLAYING:
            break;
        case GameScene::Status::HOST_DISCONNECTED:
            requestTransition([this]() {
            CULog("Host disconnected in game — returning client to HostSetupScene...");
            _audio.playMusic("lobby");
            _gameScene.setActive(false);
            _gameScene.reset();
            _hostSetupScene.setActive(true);
            _hostSetupScene.showHostDisconnectedError();
            _currentScene = State::HOSTSETUP;
            });
            break;
        }
        break;
    }
    _input.resetAction();
}

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
void SceneLoader::requestTransition(std::function<void()> applySwitch)
{
    if (_transitionPhase != TransitionPhase::NONE) {
        return;
    }
    _pendingSwitch = applySwitch;
    _transitionPhase = TransitionPhase::FADE_OUT;
}

/**
* Advances the active fade transition. Will fade out visually if the current _transitionPhase is TransitionPhase::FADE_OUT and will fade in if  TransitionPhase::FADE_IN
*
* @param dt The time (in seconds) since the last frame
*/
void SceneLoader::updateTransition(float dt)
{
    if (_transitionPhase == TransitionPhase::NONE) {
        return;
    }

    // Cap the per-frame fade step at the equivalent of one 30 FPS frame.
    //
    // The scene swap runs synchronously inside _pendingSwitch() the moment
    // the screen is fully black. Some swaps (LOAD -> MENU, PREGAME -> GAME)
    // do hundreds of ms of work: initializing every scene, loading enemy
    // animations, etc. CUGL feeds real wall-clock
    // dt into update(), so the frame *after* a
    // heavy swap arrives with a dt that includes all that blocking work.
    //
    // Without a cap, that single inflated dt makes step >= 1.0 and the
    // fade-in collapses to one frame and so the user sees an instant cut
    // instead of a fade. Clamping the effective dt to 1/30s means the
    // fade advances at most by ~15% per frame no matter how long the
    // previous frame stalled, so the fade-in stays visible.
    //

    float step = std::min(dt, 1.0f / 30.0f) / FADE_DURATION;
    if (_transitionPhase == TransitionPhase::FADE_OUT) {
        _fadeAlpha += step;
        if (_fadeAlpha >= 1.0f) {
            _fadeAlpha = 1.0f;
            // Defer the scene swap by one frame so draw() can present a
            // fully-black frame first. Otherwise the synchronous swap blocks
            // before the black frame ever reaches the display, and the user
            // stares at the previous scene tinted to whatever alpha the last
            // drawn frame happened to land on (looks like a faded freeze).
            _transitionPhase = TransitionPhase::BLACK_HOLD;
        }
    } else if (_transitionPhase == TransitionPhase::BLACK_HOLD) {
        if (_pendingSwitch) {
            _pendingSwitch();
            _pendingSwitch = nullptr;
        }
        _transitionPhase = TransitionPhase::FADE_IN;
    } else {
        _fadeAlpha -= step;
        if (_fadeAlpha <= 0.0f) {
            _fadeAlpha = 0.0f;
            _transitionPhase = TransitionPhase::NONE;
        }
    }
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
void SceneLoader::draw()
{
    // This takes care of begin/end
    switch (_currentScene)
    {
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
    case State::WINLOSE:
        _winLoseScene.render();
        break;
    case State::CODEX:
        _codexScene.render();
        break;
    case State::PREGAMEENTRY:
        _preGameEntryScene.render();
        break;
    }
    if (_settingsScene.isActive()) {
        _settingsScene.render();
    }

    // Draw the black fade overlay on top of everything during a transition
    if (_transitionPhase != TransitionPhase::NONE) {
        _batch->setPerspective(Mat4::IDENTITY);
        _batch->begin();
        _batch->setTexture(nullptr);
        _batch->setColor(Color4(0, 0, 0, std::clamp(_fadeAlpha, 0.0f, 1.0f) * 255));
        _batch->fill(Rect(-1.0f, -1.0f, 2.0f, 2.0f));
        _batch->end();
        
        // Color is left as the fade tint after end(), so reset
        // it to the default white so subsequent scene renders are unaffected.
        _batch->setColor(Color4::WHITE);
    }
}

void SceneLoader::updateGameScene(float dt)
{
    _gameScene.update(dt, _input);
    // scene switching logic goes here
}

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
void SceneLoader::selectBossTheme(GameScene& gameScene) {
    if (gameScene.getGameState().getEnemy()) {
        std::string enemyName = gameScene.getGameState().getEnemy()->getId();
        if (enemyName == "gaia") {
            _audio.playMusic("gaia_theme");
        }
        else if (enemyName == "cyclops" || enemyName == "circe") {
            _audio.playMusic("cyclops_theme");
        }
        else if (enemyName == "cerberus") {
            _audio.playMusic("cerberus_theme");
        }
    } else {
        _audio.playMusic("cyclops_theme");
    }
}
