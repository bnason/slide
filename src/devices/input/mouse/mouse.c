#define _POSIX_C_SOURCE 200112L

#include "mouse.h"
#include "../../../slide.h"
#include "../input.h"
#include "../../../utils/die.h"
#include "../../output/output.h"

#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/util/log.h>

struct sc_mouse_state mouseState = {
	.request_cursor = {.notify = seat_request_cursor },
	.request_set_selection = {.notify = seat_request_set_selection },
	.cursor_motion = { .notify = handle_cursor_motion },
	.cursor_motion_absolute = { .notify = handle_cursor_motion_absolute },
	.cursor_button = { .notify = handle_cursor_button },
	.cursor_axis = { .notify = handle_cursor_axis },
	.cursor_frame = { .notify = server_cursor_frame },
	.cur_x = 0,
	.cur_y = 0,
};

void mouse_init()
{
	wlr_log(WLR_DEBUG, "mouse_init start");

	// Creates a cursor, which is a wlroots utility for tracking the cursor
	// image shown on screen.
	mouseState.cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(mouseState.cursor, outputState.layout);

	// Creates an xcursor manager, another wlroots utility which loads up
	// Xcursor themes to source cursor images from and makes sure that cursor
	// images are available at all scale factors on the screen (necessary for
	// HiDPI support).
	mouseState.xcursor_manager = wlr_xcursor_manager_create(NULL, 24);
	if (!mouseState.xcursor_manager) die("Failed to load default cursor");

	// wlr_cursor *only* displays an image on screen. It does not move around
	// when the pointer moves. However, we can attach input devices to it, and
	// it will generate aggregate events for all of them. In these events, we
	// can choose how we want to process them, forwarding them to clients and
	// moving the cursor around. More detail on this process is described in my
	// input handling blog post:
	//
	// https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html
	//
	// And more comments are sprinkled throughout the notify functions above.
	wl_signal_add(&mouseState.cursor->events.motion, &mouseState.cursor_motion);
	wl_signal_add(&mouseState.cursor->events.motion_absolute, &mouseState.cursor_motion_absolute);
	wl_signal_add(&mouseState.cursor->events.button, &mouseState.cursor_button);
	wl_signal_add(&mouseState.cursor->events.axis, &mouseState.cursor_axis);
	wl_signal_add(&mouseState.cursor->events.frame, &mouseState.cursor_frame);
	wl_signal_add(&inputState.seat->events.request_set_cursor, &mouseState.request_cursor);
	wl_signal_add(&inputState.seat->events.request_set_selection, &mouseState.request_set_selection);

	wlr_cursor_set_xcursor(mouseState.cursor, mouseState.xcursor_manager, "default");

	wlr_log(WLR_DEBUG, "mouse_init done");
}

void server_cursor_frame(struct wl_listener *listener, void *data)
{
	// This event is forwarded by the cursor when a pointer emits an frame
	// event. Frame events are sent after regular pointer events to group
	// multiple events together. For instance, two axis events may happen at the
	// same time, in which case a frame event won't be sent in between.

	// Notify the client with pointer focus of the frame event.
	wlr_seat_pointer_notify_frame(inputState.seat);
}

void seat_request_cursor(struct wl_listener *listener, void *data)
{
	// This event is raised by the seat when a client provides a cursor image
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client = inputState.seat->pointer_state.focused_client;

	// This can be sent by any client, so we check to make sure this one is
	// actually has pointer focus first.
	if (focused_client == event->seat_client) {
		// Once we've vetted the client, we can tell the cursor to use the
		// provided surface as the cursor image. It will set the hardware cursor
		// on the output that it's currently on and continue to do so as the
		// cursor moves between outputs.
		wlr_cursor_set_surface(mouseState.cursor, event->surface, event->hotspot_x, event->hotspot_y);
	}
}

void seat_request_set_selection(struct wl_listener *listener, void *data)
{
	// This event is raised by the seat when a client wants to set the selection,
	// usually when the user copies something. wlroots allows compositors to
	// ignore such requests if they so choose, but in tinywl we always honor
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(inputState.seat, event->source, event->serial);
}

void mouse_new(struct wlr_input_device *device)
{
	wlr_log(WLR_DEBUG, "mouse_new");
	wlr_cursor_attach_input_device(mouseState.cursor, device);
}

void handle_cursor_motion(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_motion_event *event = data;
	wlr_cursor_move(mouseState.cursor, &event->pointer->base, event->delta_x, event->delta_y);
}

void handle_cursor_motion_absolute(struct wl_listener *listener, void *data)
{
	// wlr_log(WLR_DEBUG, "handle_cursor_motion_absolute");

	struct wlr_pointer_motion_absolute_event *event = data;

	mouseState.cur_x = event->x;
	mouseState.cur_y = event->y;

	// wlr_log(WLR_DEBUG, "handle_cursor_motion_absolute %f %f", mouseState.cur_x, mouseState.cur_y);
	wlr_cursor_warp_absolute(mouseState.cursor, &event->pointer->base, mouseState.cur_x, mouseState.cur_y);
}

void handle_cursor_button(struct wl_listener *listener, void *data)
{
	wlr_log(WLR_DEBUG, "handle_cursor_button");
	// struct wlr_pointer_button_event *event = data;

	// float(*color)[4];
	// if (event->state == WLR_BUTTON_RELEASED) {
	// 	color = &mouseState.default_color;
	// 	memcpy(&mouseState.clear_color, color, sizeof(*color));
	// }
	// else {
	// 	float red[4] = { 0.25f, 0.25f, 0.25f, 1 };
	// 	red[event->button % 3] = 1;
	// 	color = &red;
	// 	memcpy(&mouseState.clear_color, color, sizeof(*color));
	// }
}

void handle_cursor_axis(struct wl_listener *listener, void *data)
{
	wlr_log(WLR_DEBUG, "handle_cursor_axis");
	// struct wlr_pointer_axis_event *event = data;

	// for (size_t i = 0; i < 3; ++i) {
	// 	mouseState.default_color[i] += event->delta > 0 ? -0.05f : 0.05f;
	// 	if (mouseState.default_color[i] > 1.0f) {
	// 		mouseState.default_color[i] = 1.0f;
	// 	}
	// 	if (mouseState.default_color[i] < 0.0f) {
	// 		mouseState.default_color[i] = 0.0f;
	// 	}
	// }

	// memcpy(&mouseState.clear_color, &mouseState.default_color, sizeof(mouseState.clear_color));
}
