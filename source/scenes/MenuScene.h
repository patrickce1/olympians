#ifndef __MENU_SCENE_H__
#define __MENU_SCENE_H__

#include <cugl/cugl.h>

/**
 * Main menu scene shown after loading completes.
 */
class MenuScene : public cugl::scene2::Scene2 {
public:
    /** Menu actions consumed by SceneLoader for transitions. */
    enum class Action {
        NONE,
        START_GAME,
        OPEN_SETTINGS,
        OPEN_ITEMS
    };

protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;
    /** The root scene node for this scene graph. */
    std::shared_ptr<cugl::scene2::SceneNode> _scene;

    /** Menu buttons. */
    std::shared_ptr<cugl::scene2::Button> _playButton;
    std::shared_ptr<cugl::scene2::Button> _settingsButton;
    std::shared_ptr<cugl::scene2::Button> _itemsButton;

    /** The next action requested by menu input. */
    Action _nextAction = Action::NONE;

public:
    MenuScene() : cugl::scene2::Scene2() {}
    ~MenuScene() { dispose(); }

    void dispose() override;
    bool init(const std::shared_ptr<cugl::AssetManager>& assets);
    void setActive(bool value) override;
    void update(float dt) override;

    /** Returns and clears the queued action from this frame. */
    Action consumeAction();
};

#endif /* __MENU_SCENE_H__ */
