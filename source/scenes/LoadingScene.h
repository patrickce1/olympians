#ifndef __APP_LOADING_SCENE_H__
#define __APP_LOADING_SCENE_H__

#include <cugl/cugl.h>
#include <cugl/core/assets/CUAssetManager.h>
#include <cugl/scene2/CUScene2.h>
#include <cugl/scene2/CUProgressBar.h>
#include <cugl/scene2/CUButton.h>
    
/**
 * This class is a simple loading screen for asychronous asset loading.
 *
 * This class will either create its own {@link AssetManager}, or it can be
 * assigned on. In the latter case, it needs a json file defining the assets
 * for the initial loading screen. In the former, the asset manager should
 * come preloaded with these assets. To properly display to the screen,
 * these assets must include a {@link scene2::SceneNode} named "load". This
 * node must have at least four children:
 *
 *     - "load.before": The scene to display while loading is in progress
 *     - "load.after": The scene to display when the loading is complete
 *     - "load.bar": A {@link ProgressBar} for showing the loading progress
 *     - "load.play" A play {@link Button} for the user to start the game
 *
 * When the play button is pressed, this scene is deactivated, indicating to
 * the application that it is time to switch scenes.
 *
 * In addition to these assets, the loading scene will take the take of an
 * asset directory. This is a JSON file defining the assets that should be
 * loaded asynchronously by this loading scene. Accessing the asset manager
 * with {@link #getAssetManager} gives access to these assets.
 */
class AppLoadingScene : public cugl::scene2::Scene2 {
public:

    /**
     * Represents the different phases of the animated loading sequence.
     *
     * The loading scene progresses through these phases in order to create
     * a cinematic transition from startup into the main menu.
     */
    enum class LoadPhase {
        /** The team logo fades smoothly into view */
        LOGO_FADE_IN,
        /** The logo remains fully visible for a short duration */
        LOGO_HOLD,
        /** The logo fades back out to transition into the loading scene */
        LOGO_FADE_OUT,
        /** The loading scene background fades into visibility */
        LOADING_SCENE_FADE_IN,
        /** The loading bar and loading text fade into visibility */
        BAR_FADE_IN,
        /** Assets are actively loading and progress is updated */
        LOADING,
        /** Loading has completed and the scene is ready to transition */
        FINISHED
    };
    
    
protected:
    /** The asset manager for loading. */
    std::shared_ptr<cugl::AssetManager> _assets;
    
    /** The asset directory reference */
    std::string _directory;
    
    /** The scene during loading */
    std::shared_ptr<cugl::scene2::SceneNode>  _loadingScene;
    
    /** The animated progress bar */
    std::shared_ptr<cugl::scene2::ProgressBar>  _bar;
    
    /** The black overlay for transitioning in the loading scene */
    std::shared_ptr<cugl::scene2::SceneNode> _blackOverlay;
    
    /** The studio logo */
    std::shared_ptr<cugl::scene2::SceneNode> _logo;
    
    /** The label above the loading bar */
    std::shared_ptr<cugl::scene2::Label> _loadingText;
    
    /** The progress displayed on the screen */
    float _progress = 0.0f;
    
    /** The artificial progress*/
    float _displayProgress = 0.0f;
    
    /** Whether or not the player has pressed play to continue */
    bool  _completed = false;
    
    /** Whether or not the asset loader has started loading */
    bool  _started = false;
    
    /** How quickly the dots in 'Loading..." appear */
    float _dotTimer = 0.0f;
    
    /** The number of dots after 'Loading' currently */
    int _dotCount = 0;
    
    /** The current phase of the animated loading sequence */
    LoadPhase _phase;
    
    /** Tracks elapsed time within the current loading phase */
    float _phaseTimer = 0.0f;
    
    /** Current alpha value used for fading the logo in and out */
    float _logoAlpha = 0.0f;
    
    /** Current alpha value used for fading in the loading scene */
    float _sceneAlpha = 0.0f;
    
    /** Current alpha value used for fading in the loading bar and text */
    float _barAlpha = 0.0f;

public:
#pragma mark -
#pragma mark Constructors
    /**
     * Creates a new loading scene with the default values.
     *
     * This constructor does not allocate any objects or start the scene.
     * This allows us to use the object without a heap pointer.
     */
    AppLoadingScene() : Scene2(), _progress(0.0f), _completed(false), _started(false) {}
    
    /**
     * Deletes this scene, disposing all resources.
     */
    ~AppLoadingScene() { dispose(); }
    
    /**
     * Disposes all of the resources used by this sceene.
     *
     * A disposed node can be safely reinitialized. The scene graph owned by
     * this scene will be released, as well as the asset manager. They will be
     * deleted if no other object owns them.
     */
    void dispose();
    
    /**
     * Initializes a loading scene with the given scene and directory.
     *
     * This class will create its own {@link AssetManager}, which can be
     * accessed via {@link #getAssetManager}. This asset manager will only
     * attach loaders for {@link Font}, {@link Texture}, {@link scene2::SceneNode}
     * and {@link WidgetValue}.
     *
     * The string scene should be a path to a JSON file that defines the scene
     * graph for this loading scene. This file will be loaded synchronously, so
     * it should be lightweight. The scene must include a {@link scene2::SceneNode}
     * named "load". This node must have at least three children:
     *
     *     - "load.loadingScene": The scene to display while loading is in progress
     *     - "load.loadingScene.bar": A {@link ProgressBar} for showing the loading progress
     *     - "load.logo" A logo defining the game creators
     *
     * The string directory is the asset directory to be loaded asynchronously
     * by this scene. Loading will commence after a call to {@link #start}. The
     * progress on this directory can be monitored via {@link #getProgress}.
     *
     * @param scene     A JSON file with the scene graph for this scene
     * @param directory The asset directory to load asynchronously
     *
     * @return true if the scene is initialized properly, false otherwise.
     */
    bool init(const std::string scene, const std::string directory);
    
    /**
     * Initializes a loading scene with the given asset manager and directory.
     *
     * The asset manager must already contain the scene graph used by this
     * scene. The scene must include a {@link scene2::SceneNode} named
     * "load". This node must have at least This node must have at least three children:
     *
     *     - "load.loadingScene": The scene to display while loading is in progress
     *     - "load.loadingScene.bar": A {@link ProgressBar} for showing the loading progress
     *     - "load.logo" A logo defining the game creators
     *     
     * The string directory is the asset directory to be loaded asynchronously
     * by this scene. The progress on this directory can be monitored via
     * {@link #getProgress}.
     *
     * @param manager   A previously initialized asset manager
     * @param directory The asset directory to load asynchronously
     *
     * @return true if the scene is initialized properly, false otherwise.
     */
    bool init(const std::shared_ptr<cugl::AssetManager>& manager,
              const std::string directory);
    
    /**
     * Returns a newly allocated loading scene with the given scene and directory.
     *
     * This class will create its own {@link AssetManager}, which can be
     * accessed via {@link #getAssetManager}. This asset manager will only
     * attach loaders for {@link graphics::Font}, {@link graphics::Texture},
     * {@link scene2::SceneNode} and {@link WidgetValue}.
     *
     * The string scene should be a path to a JSON file that defines the scene
     * graph for this loading scene. This file will be loaded synchronously, so
     * it should be lightweight. The scene must include a {@link scene2::SceneNode}
     * named "load". This node must have at least three children:
     *
     *     - "load.loadingScene": The scene to display while loading is in progress
     *     - "load.loadingScene.bar": A {@link ProgressBar} for showing the loading progress
     *     - "load.logo" A logo defining the game creators
     *
     * The string directory is the asset directory to be loaded asynchronously
     * by this scene. The progress on this directory can be monitored via
     * {@link #getProgress}.
     *
     * @param scene     A JSON file with the scene graph for this scene
     * @param directory The asset directory to load asynchronously
     *
     * @return a newly allocated loading scene with the given scene and directory.
     */
    static std::shared_ptr<AppLoadingScene> alloc(const std::string scene,
                                               const std::string directory) {
        std::shared_ptr<AppLoadingScene> result = std::make_shared<AppLoadingScene>();
        return (result->init(scene,directory) ? result : nullptr);
    }
    
    /**
     * Returns a newly allocated loading scene with the given asset manager and directory.
     *
     * The asset manager must already contain the scene graph used by this
     * scene. The scene must include a {@link scene2::SceneNode} named
     * "load". This node must have at least three children:
     *
     *     - "load.loadingScene": The scene to display while loading is in progress
     *     - "load.loadingScene.bar": A {@link ProgressBar} for showing the loading progress
     *     - "load.logo" A logo defining the game creators
     *
     * The string directory is the asset directory to be loaded asynchronously
     * by this scene. The progress on this directory can be monitored via
     * {@link #getProgress}.
     *
     * @param manager   A previously initialized asset manager
     * @param directory The asset directory to load asynchronously
     *
     * @return a newly allocated loading scene with the given asset manager and directory.
     */
    static std::shared_ptr<AppLoadingScene> alloc(const std::shared_ptr<cugl::AssetManager>& manager,
                                               const std::string directory) {
        std::shared_ptr<AppLoadingScene> result = std::make_shared<AppLoadingScene>();
        return (result->init(manager,directory) ? result : nullptr);
    }
    
    
#pragma mark -
#pragma mark Progress Monitoring
    /**
     * Returns the asset manager for this loading scene
     *
     * @returns the asset manager for this loading scene
     */
    std::shared_ptr<cugl::AssetManager> getAssetManager() const { return _assets; }
    
    /**
     * Starts the loading progress for this scene
     *
     * This method has no affect if loading is already in progress.
     */
    void start();
    
    /**
     * Updates the loading scene progress.
     *
     * This method queries the asset manager to update the progress bar amount.
     *
     * @param timestep  The amount of time (in seconds) since the last frame
     */
    void update(float timestep);

    /**
     * Returns the current progress of this this loading scene.
     *
     * The value is in the range [0,1] where 0 means no progress and 1 means
     * that loading has completed.
     *
     * @return the current progress of this this loading scene.
     */
    float getProgress( ) const { return _progress; }
    
    /**
     * Returns true if loading is complete, and the player has pressed play
     *
     * @return true if loading is complete, and the player has pressed play
     */
    bool isComplete( ) const { return _completed; }
    
    /** Resizes the scene as needed. */
    void resize();
};

#endif /* __APP_LOADING_SCENE_H__ */
