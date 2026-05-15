#include <cugl/graphics/loaders/CUTextureLoader.h>
#include <cugl/graphics/loaders/CUFontLoader.h>
#include <cugl/scene2/CUScene2Loader.h>
#include <cugl/core/assets/CUWidgetLoader.h>
#include <cugl/core/CUApplication.h>
#include "LoadingScene.h"

using namespace cugl;
using namespace cugl::scene2;
using namespace cugl::graphics;

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
 * named "load". This node must have at least four children:
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
bool AppLoadingScene::init(const std::string scene,
                           const std::string directory) {

    _assets = AssetManager::alloc();

    if (_assets == nullptr || !_assets->loadDirectory(scene)) {
        return false;
    }

    _assets->attach<Font>(FontLoader::alloc()->getHook());
    _assets->attach<Texture>(TextureLoader::alloc()->getHook());
    _assets->attach<WidgetValue>(WidgetLoader::alloc()->getHook());
    _assets->attach<scene2::SceneNode>(Scene2Loader::alloc()->getHook());

    return init(_assets,directory);
}

bool AppLoadingScene::init(
    const std::shared_ptr<cugl::AssetManager>& assets,
    const std::string directory) {

    auto layer = assets->get<scene2::SceneNode>("load");

    if (layer == nullptr) {
        CUAssertLog(false,"Missing \"load\" scene");
        return false;
    }
    _blackOverlay = assets->get<scene2::SceneNode>("load.blackOverlay");

    _logo = assets->get<scene2::SceneNode>("load.logo");

    _loadingScene = assets->get<scene2::SceneNode>("load.loadingScene");
    
    _loadingText = std::dynamic_pointer_cast<scene2::Label>(
            assets->get<scene2::SceneNode>("load.loadingScene.label"));

    _bar = std::dynamic_pointer_cast<scene2::ProgressBar>(
            assets->get<scene2::SceneNode>("load.loadingScene.bar.fill"));

    if (_bar == nullptr) {
        CUAssertLog(false,"Missing loading bar");
        return false;
    }


    if (layer->getJSON()->has("size")) {

        if (!Scene2::initWithHint(layer->getContentSize())) {
            return false;
        }

    } else if (!Scene2::init()) {
        return false;
    }

    layer->setContentSize(_size);
    layer->doLayout();

    addChild(layer);

    _loadingScene->setVisible(false);
    _bar->setVisible(false);

    // Initial alpha setup
    Color4 opacity;

    opacity = _logo->getColor();
    opacity.a = 0;
    _logo->setColor(opacity);

    opacity = _loadingScene->getColor();
    opacity.a = 0;
    _loadingScene->setColor(opacity);

    opacity = _bar->getColor();
    opacity.a = 0;
    _bar->setColor(opacity);
    
    opacity = _loadingText->getForeground();
    opacity.a = 0;
    _loadingText->setColor(opacity);

    _assets = assets;
    _directory = directory;

    _phase = LoadPhase::LOGO_FADE_IN;

    return true;
}

/**
 * Disposes of all (non-static) resources allocated to this mode.
 */
void AppLoadingScene::dispose() {
    _assets = nullptr;

    _blackOverlay = nullptr;
    _logo = nullptr;
    _loadingScene = nullptr;

    _bar = nullptr;

    _progress = 0.0f;
    _displayProgress = 0.0f;

    _logoAlpha = 0.0f;
    _sceneAlpha = 0.0f;
    _barAlpha = 0.0f;

    _phaseTimer = 0.0f;

    _started = false;
    _completed = false;
}

/**
 * Starts the loading progress for this scene
 *
 * This method has no affect if loading is already in progress.
 */
void AppLoadingScene::start() {
    if (_started) return;
    _started = true;
}

/**
 * Updates the loading scene progress.
 *
 * This method queries the asset manager to update the progress bar amount.
 *
 * @param timestep  The amount of time (in seconds) since the last frame
 */
void AppLoadingScene::update(float dt) {

    if (!_started) {
        return;
    }

    _phaseTimer += dt;

    switch (_phase) {

        // Fade logo in
        case LoadPhase::LOGO_FADE_IN: {

            _logoAlpha += dt;

            if (_logoAlpha > 1.0f) {
                _logoAlpha = 1.0f;
            }

            Color4 opacity = _logo->getColor();
            opacity.a = (Uint8)(_logoAlpha * 255);

            _logo->setColor(opacity);

            // Small pulse effect
            float scale =
                1.0f + 0.02f * sin(_phaseTimer * 2.0f);

            _logo->setScale(scale);

            if (_logoAlpha >= 1.0f) {
                _phase = LoadPhase::LOGO_HOLD;
                _phaseTimer = 0.0f;
            }
            break;
        }
        case LoadPhase::LOGO_HOLD: {

            if (_phaseTimer >= 1.0f) {
                _phase = LoadPhase::LOGO_FADE_OUT;
            }

            break;
        }
        case LoadPhase::LOGO_FADE_OUT: {

            _logoAlpha -= dt;

            if (_logoAlpha < 0.0f) {
                _logoAlpha = 0.0f;
            }

            Color4 color = _logo->getColor();
            color.a = (Uint8)(_logoAlpha * 255);

            _logo->setColor(color);

            if (_logoAlpha <= 0.0f) {
                _loadingScene->setVisible(true);
                _assets->loadDirectoryAsync(_directory,nullptr);

                _phase = LoadPhase::LOADING_SCENE_FADE_IN;
                _phaseTimer = 0.0f;
            }

            break;
        }
        case LoadPhase::LOADING_SCENE_FADE_IN: {

            _sceneAlpha += dt;

            if (_sceneAlpha > 1.0f) {
                _sceneAlpha = 1.0f;
            }

            Color4 color = _loadingScene->getColor();
            color.a = (Uint8)(_sceneAlpha * 255);

            _loadingScene->setColor(color);

            if (_sceneAlpha >= 1.0f) {
                _phase = LoadPhase::BAR_FADE_IN;
                _phaseTimer = 0.0f;
            }
            break;
        }
        case LoadPhase::BAR_FADE_IN: {

            _bar->setVisible(true);
            _loadingText->setVisible(true);

            _barAlpha += dt * 2.0f;

            if (_barAlpha > 1.0f) {
                _barAlpha = 1.0f;
            }

            Color4 color = _bar->getColor();
            color.a = (Uint8)(_barAlpha * 255);

            _bar->setColor(color);
            _loadingText->setColor(color);

            if (_barAlpha >= 1.0f) {
                _phase = LoadPhase::LOADING;
            }

            break;
        }
        case LoadPhase::LOADING: {
            _dotTimer += dt;

            if (_dotTimer >= 0.2f) {
                _dotTimer = 0.0f;
                _dotCount = (_dotCount + 1) % 4;
                std::string text = "Loading";
                
                for (int i = 0; i < _dotCount; i++) {
                    text += ".";
                }
                _loadingText->setText(text);
            }
            
            // Actual loading
            _progress = _assets->progress();
            
            _displayProgress +=
                (_progress - _displayProgress) * 0.1f;

            _bar->setProgress(_displayProgress);

            if (_progress >= 1.0f &&
                _displayProgress >= 0.99f) {

                _bar->setProgress(1.0f);

                _phase = LoadPhase::FINISHED;
                _phaseTimer = 0.0f;
            }

            break;
        }

        case LoadPhase::FINISHED:
            _dotTimer += dt;

            if (_dotTimer >= 0.2f) {
                _dotTimer = 0.0f;
                _dotCount = (_dotCount + 1) % 4;
                std::string text = "Loading";
                
                for (int i = 0; i < _dotCount; i++) {
                    text += ".";
                }
                _loadingText->setText(text);
            }
            
            if (_phaseTimer >= 1.0f) {
                    _completed = true;
                }
            break;
    }
}
