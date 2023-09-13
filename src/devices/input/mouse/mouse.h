#ifndef _input_mouse_h_included_
#define _input_mouse_h_included_

#include <wayland-server-core.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>

struct sc_mouse_state
{
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *xcursor_manager;

	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;

	double cur_x, cur_y;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;
};

void mouse_init();
void mouse_new(struct wlr_input_device *device);

void server_cursor_frame(struct wl_listener *listener, void *data);
void seat_request_cursor(struct wl_listener *listener, void *data);
void seat_request_set_selection(struct wl_listener *listener, void *data);
void handle_cursor_motion(struct wl_listener *listener, void *data);
void handle_cursor_motion_absolute(struct wl_listener *listener, void *data);
void handle_cursor_button(struct wl_listener *listener, void *data);
void handle_cursor_axis(struct wl_listener *listener, void *data);

#endif
