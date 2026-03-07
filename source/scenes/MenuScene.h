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
        JOIN_GAME
    };

protected:
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;
    /** The root scene node for this scene graph. */
    std::shared_ptr<cugl::scene2::SceneNode> _scene;

    /** Menu buttons. */
    std::shared_ptr<cugl::scene2::Button> _playButton;
    std::shared_ptr<cugl::scene2::Button> _joinButton;
    std::shared_ptr<cugl::scene2::Button> _settingsButton;
//    std::shared_ptr<cugl::scene2::Button> _itemsButton;

    /** The next action requested by menu input. */
    Action _nextAction = Action::NONE;

public:
    /**
     * Creates an uninitialized menu scene.
     *
     * This constructor only sets default values. Actual scene content is loaded
     * by {@link init}.
     */
    MenuScene() : cugl::scene2::Scene2() {}

    /**
     * Disposes this scene and releases owned resources.
     */
    ~MenuScene() { dispose(); }

    /**
     * Disposes all resources allocated by this scene.
     *
     * This clears the scene graph references, button handles, and any queued
     * menu action.
     */
    void dispose() override;

    /**
     * Initializes this scene from loaded assets.
     *
     * Expected assets include a Scene2 node named `menuScene` containing child
     * buttons `play`, `settings`, and `items` under a `menu` node.
     *
     * @param assets    The loaded asset manager
     *
     * @return true if initialization succeeds; false otherwise.
     */
    bool init(const std::shared_ptr<cugl::AssetManager>& assets);

    /**
     * Sets whether this scene is currently active.
     *
     * When active, menu buttons receive input. When inactive, they are
     * explicitly deactivated.
     *
     * @param value whether this scene should be active
     */
    void setActive(bool value) override;

    /**
     * Updates this scene.
     *
     * This scene uses event listeners for button input, so update currently
     * performs no per-frame logic beyond activity checks.
     *
     * @param dt    The elapsed time since the previous frame, in seconds
     */
    void update(float dt) override;

    /**
     * Returns and clears the pending menu action.
     *
     * This allows `SceneLoader` to consume a one-shot transition request from
     * this scene each frame.
     *
     * @return the queued action, or Action::NONE if no action is pending
     */
    Action consumeAction();
};

#endif /* __MENU_SCENE_H__ */
