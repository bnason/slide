#define _POSIX_C_SOURCE 200112L

#include "input.h"
#include "../../slide.h"
#include "keyboard/keyboard.h"
#include "mouse/mouse.h"

#include <stdlib.h>
#include <wlr/util/log.h>

struct sc_input_state inputState = {
	.newListener = {.notify = new_input_notify },
};

void input_init()
{
	// Configures a seat, which is a single "seat" at which a user sits and
	// operates the computer. This conceptually includes up to one keyboard,
	// pointer, touch, and drawing tablet device. We also rig up a listener to
	// let us know when new input devices are available on the backend.
	inputState.seat = wlr_seat_create(state.display, "seat0");
	wl_list_init(&inputState.keyboards);
	wl_signal_add(&state.backend->events.new_input, &inputState.newListener);

	mouse_init();
}

void new_input_notify(struct wl_listener *listener, void *data)
{
	struct wlr_input_device *device = data;

	switch (device->type)
	{
		case WLR_INPUT_DEVICE_KEYBOARD:
			keyboard_init(device);
			break;
		case WLR_INPUT_DEVICE_POINTER:
		case WLR_INPUT_DEVICE_TOUCH:
		case WLR_INPUT_DEVICE_TABLET_TOOL:
			mouse_new(device);
			break;
		default:
			break;
	}
}
