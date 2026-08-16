#include "ButtonHelpers.h"

using namespace cugl;

/**
 * Wires a tap-style action onto a CUGL Button.
 *
 * The button stays visually pressed for the entire duration of the press —
 * even if the finger drifts out of bounds — and un-presses on release. The
 * callback fires only if the release position is still inside the button's
 * bounding box. If the finger is released outside the bounds, the button
 * un-presses normally but the callback does not fire.
 *
 * The button must still be activated separately (`button->activate()` is
 * usually called by the scene's setActive(true)).
 *
 * @param button  The button to wire the tap action onto.
 * @param onTap   The callback to invoke when a valid tap completes.
 */
void ButtonHelpers::addTapListener(
    const std::shared_ptr<scene2::Button>& button, std::function<void()> onTap) {
    if (!button || !onTap) return;

    // pressed once a press begins inside the button. Cleared by any release.
    // Shared so the button listener and the mouse/touch release listener work together.
    auto pressed = std::make_shared<bool>(false);

    button->addListener([pressed](const std::string&, bool down) {
        if (down) *pressed = true;
    });

    // Gets the raw release position so we can decide
    // whether the action should fire, and explicitly un-presses the button
    // so the visual state recovers even if CUGL Button's own release
    // listener stopped mid-press (e.g. CodexScene deactivates its
    // item buttons when a swipe-scroll begins).
    if (Mouse* mouse = Input::get<Mouse>()) {
        Uint32 key = mouse->acquireKey();
        mouse->addReleaseListener(key,
            [button, pressed, onTap](const MouseEvent& event, Uint8, bool) {
                if (!*pressed) return;
                *pressed = false;
                button->setDown(false);
                if (button->inContentBounds(event.position)) {
                    onTap();
                }
            });
    } else if (Touchscreen* touch = Input::get<Touchscreen>()) {
        Uint32 key = touch->acquireKey();
        touch->addEndListener(key,
            [button, pressed, onTap](const TouchEvent& event, bool) {
                if (!*pressed) return;
                *pressed = false;
                button->setDown(false);
                if (button->inContentBounds(event.position)) {
                    onTap();
                }
            });
    }
}
