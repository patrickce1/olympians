#ifndef __BUTTON_HELPERS_H__
#define __BUTTON_HELPERS_H__

#include <cugl/cugl.h>
#include <functional>

namespace ButtonHelpers {

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
void addTapListener(
        const std::shared_ptr<cugl::scene2::Button>& button,
        std::function<void()> onTap);

}

#endif /* __BUTTON_HELPERS_H__ */
