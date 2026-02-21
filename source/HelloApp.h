//
//  HelloApp.h
//  Basic CUGL Demo
//
//  This is the header for the custom application. This is the declaration of 
//  your root (and in this case only) class.
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
#ifndef __HELLO_APP_H__
#define __HELLO_APP_H__
#include <cugl/cugl.h>

/**
 * Class for a simple Hello World style application
 *
 * The application simply moves the CUGL logo across the screen.  It also
 * provides a button to quit the application.
 */
class HelloApp : public cugl::Application {
protected:
    /** The loaders to (synchronously) load in assets */
    std::shared_ptr<cugl::AssetManager> _assets;

    /** A scene graph, used to display our 2D scenes */
    std::shared_ptr<cugl::scene2::Scene2> _scene;
    /** A 3152 style SpriteBatch to render the scene */
    std::shared_ptr<cugl::graphics::SpriteBatch>  _batch;
    /** A reference to the logo, so that we can move it around */
    std::shared_ptr<cugl::scene2::SceneNode>  _logo;

    /** A countdown used to move the logo */
    float _countdown;
    
    /** A logger for debugging */
    std::shared_ptr<cugl::Logger> _logger;

    /**
     * Internal helper to build the scene graph.
     *
     * Scene graphs are not required. You could manage all scenes just like
     * you do in 3152. However, they greatly simplify scene management, and
     * have become standard in most game engines.
     */
    void buildScene();

public:
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
    HelloApp();

    /**
     * Disposes this application, releasing all resources.
     *
     * This destructor is called by SDL when the application quits. It simply
     * calls the dispose() method in Application.  There is nothing special to
     * do here.
     */
    ~HelloApp() { }

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

};

#endif /* __HELLO_APP_H__ */
