#ifndef _input_h_included_
#define _input_h_included_

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_seat.h>

struct sc_input_state
{
	struct wl_listener newListener;
	struct wl_list keyboards;
	struct wlr_seat *seat;
};

extern struct sc_input_state inputState;

void input_init();
void new_input_notify(struct wl_listener* listener, void* data);

#endif
