#ifndef __MENU_SCENE_H__
#define __MENU_SCENE_H__

#include <cugl/cugl.h>
#include "../AudioController.h"
#include "../SavedDataManager.h"

/**
 * Main menu scene shown after loading completes.
 */
class MenuScene : public cugl::scene2::Scene2 {
public:
    /** Scene status for SceneLoader for transitions. */
    enum class Status {
        NONE,
        START_GAME,
        OPEN_SETTINGS,
        NAME_ONBOARDING,
        PENDING_ONBOARDING,
        PENDING_SAVE
    };

protected:
    /**
     * Internal state of the first-launch onboarding overlay.
     */
    enum class OverlayState {
        HIDDEN,             // No overlay is visible; normal menu interaction.
        NAME_PROMPT,        // The name-entry popup is visible and accepting input.
        CONFIRM,            // The confirmation popup is visible and counting down.
    };
    
    /** The asset manager for this scene. */
    std::shared_ptr<cugl::AssetManager> _assets;

    /** The audio controller shared across all scenes */
    AudioController* _audio = nullptr;
    
    /** The root scene node for this scene graph. */
    std::shared_ptr<cugl::scene2::SceneNode> _scene;

    /** Play button. */
    std::shared_ptr<cugl::scene2::Button> _playButton;
    
    /** Settings button*/
    std::shared_ptr<cugl::scene2::Button> _settingsButton;
    
    /** Root node of the name-entry modal shown on first launch. */
    std::shared_ptr<cugl::scene2::SceneNode> _namePopup;

    /** Text field inside the name-entry popup. */
    std::shared_ptr<cugl::scene2::TextField> _nameField;

    /** Save button inside the name-entry popup. */
    std::shared_ptr<cugl::scene2::Button> _nameSaveButton;

    /** Root node of the confirmation modal shown after name is saved. */
    std::shared_ptr<cugl::scene2::SceneNode> _confirmPopup;

    /** The next action requested by menu input. */
    Status _status = Status::NONE;
    
    /** Current state of the first-launch onboarding overlay. */
    OverlayState _overlayState = OverlayState::HIDDEN;

    /** Elapsed time (seconds) since the confirm popup became visible. */
    float _confirmTimer = 0.0f;

    /** Duration (seconds) before the confirm popup is auto-dismissed. */
    static constexpr float CONFIRM_DISPLAY_TIME = 2.0f;

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
     * @param audio    The audio controller used for various sounds.
     *
     * @return true if initialization succeeds; false otherwise.
     */
    bool init(const std::shared_ptr<cugl::AssetManager>& assets, AudioController* audio);

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
     * Returns the current menu status.
     *
     * SceneLoader calls this each frame to check whether a scene transition
     * has been requested.
     *
     * @return the current Status value.
     */
    Status getStatus() const { return _status; }

    /**
     * Resets the menu status back to NONE.
     *
     * SceneLoader calls this after consuming a transition so the status
     * does not fire again on the next frame.
     */
    void resetStatus() { _status = Status::NONE; }
};

#endif /* __MENU_SCENE_H__ */
