//
//  HelloApp.cpp
//  Basic CUGL Demo
//
//  This is the implementation file for the custom application. This is the
//  definition of your root (and in this case only) class.
//
//  CUGL MIT License:
//      This software is provided 'as-is', without any express or implied
//      warranty.  In no event will the authors be held liable for any damages
//      arising from the use of this software.
//
//      Permission is granted to anyone to use this software for any purpose,
//      including commercial applications, and to alter it and redistribute it
//      freely, subject to the following restrictions:
//
//      1. The origin of this software must not be misrepresented; you must not
//      claim that you wrote the original software. If you use this software
//      in a product, an acknowledgment in the product documentation would be
//      appreciated but is not required.
//
//      2. Altered source versions must be plainly marked as such, and must not
//      be misrepresented as being the original software.
//
//      3. This notice may not be removed or altered from any source distribution.
//
//  Author: Walker White
//  Version: 12/22/25
//
// Include the class header, which includes all of the CUGL classes
#include "HelloApp.h"
#include <cugl/core/CUBase.h>
#include <cugl/core/util/CULogger.h>

// Add support for simple random number generation
#include <cstdlib>
#include <ctime>
#include <vector>

// This keeps us from having to write cugl:: all the time
using namespace cugl;
using namespace cugl::scene2;
using namespace cugl::graphics;


/** This is the main application and so we need this macro at the start */
CU_ROOTCLASS(HelloApp)

// The number of seconds before moving the logo to a new position
#define TIME_STEP    1

// The height is a suggestion, but the width is mandatory
#define GAME_WIDTH 1024
#define GAME_HEIGHT 576


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
HelloApp::HelloApp() : Application(), _countdown(-1) {
    // Pre-launch configuration. Nothing here can be reassigned later.
    setName("Hello World");
    setOrganization("GDIAC");
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
void HelloApp::onStartup() {
    // Create a scene graph with a locked screen height
    _scene = Scene2::allocWithHint(0,GAME_HEIGHT);

    // Create a sprite batch (and background color) to render the scene
    _batch = SpriteBatch::alloc();
    setClearColor(Color4(229,229,229,255));
    _scene->setSpriteBatch(_batch);

    // Create an asset manager to load all assets
    _assets = AssetManager::alloc();

    // You have to attach the individual loaders for each asset type
    _assets->attach<Texture>(TextureLoader::alloc()->getHook());
    _assets->attach<Font>(FontLoader::alloc()->getHook());

    // This reads the given JSON file and uses it to load all other assets
    _assets->loadDirectory("json/assets.json");

    // Activate mouse or touch screen input as appropriate
    // We have to do this BEFORE the scene, because the scene has a button
#if defined (CU_TOUCH_SCREEN)
    Input::activate<Touchscreen>();
#else
    Input::activate<Mouse>();
#endif

    // Build the scene from these assets
    buildScene();
    Application::onStartup();

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
void HelloApp::onShutdown() {
    Logger::close("debug");

    // Delete all smart pointers
    _logo = nullptr;
    _scene = nullptr;
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
void HelloApp::onResize() {
    // When we resize, we have to resize the scene
    // To see why, comment this out
    _scene->resizeToHint(0,GAME_HEIGHT);
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
 */
void HelloApp::update(float dt) {
    if (_countdown <= 0) {
        // Move the logo about the screen
        Size size = _scene->getSize();
        float x = (float)(std::rand() % (int)(size.width/2))+size.width/4;
        float y = (float)(std::rand() % (int)(size.height/2))+size.height/4;
        _logo->setPosition(x,y);
        _countdown = TIME_STEP;
    } else {
        _countdown -= dt;
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
void HelloApp::draw() {
    // This takes care of begin/end
    _scene->render();
}

/**
 * Internal helper to build the scene graph.
 *
 * Scene graphs are not required. You could manage all scenes just like
 * you do in 3152. However, they greatly simplify scene management, and
 * have become standard in most game engines.
 */
void HelloApp::buildScene() {
    Size size = _scene->getSize();
    
    // Create a logo from an image
    std::shared_ptr<Texture> texture  = _assets->get<Texture>("logo");
    _logo = scene2::PolygonNode::allocWithTexture(texture);
    _logo->setScale(0.2f); // Magic number to rescale asset

    // Put the logo in the middle of the screen
    _logo->setAnchor(Vec2::ANCHOR_CENTER);
    _logo->setPosition(size.width/2,size.height/2);
    
    // Create a button.  A button has an up image and a down image
    std::shared_ptr<Texture> up   = _assets->get<Texture>("close-normal");
    std::shared_ptr<Texture> down = _assets->get<Texture>("close-selected");
    auto button = scene2::Button::alloc(scene2::PolygonNode::allocWithTexture(up),
                                        scene2::PolygonNode::allocWithTexture(down));

    // Create a callback function for the button
    button->setName("close");
    button->addListener([this] (const std::string& name, bool down) {
        // Only quit when the button is released
        if (!down) {
            CULog("Goodbye!");
            this->quit();
        }
    });

    // Put this button in a scene node scaled to safe area
    auto safe = scene2::SceneNode::allocForSafeBounds();
    safe->addChild(button);
    
    // Add a layout manager, so it is adjusted on resize
    button->setAnchor(Vec2::ANCHOR_BOTTOM_RIGHT);
    auto layout = AnchoredLayout::alloc();
    layout->addAnchor("close", AnchoredLayout::Anchor::BOTTOM_RIGHT);
    safe->setLayout(layout);
    
    // Add the logo and button to the scene graph
    _scene->addChild(_logo);
    _scene->addChild(safe);
    safe->doLayout();
    
    // We can only activate a button AFTER it is added to a scene
    button->activate();

    // Start the logo countdown and C-style random number generator
    _countdown = TIME_STEP;
    std::srand((int)std::time(0));
}
