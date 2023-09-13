#ifndef _slide_h_included_
#define _slide_h_included_

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>

#include "devices/output/output.h"

#define END(A)                  ((A) + LENGTH(A))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define LISTEN(E, L, H)         wl_signal_add((E), ((L)->notify = (H), (L)))
#define VISIBLEON(C, M)         ((M) && (C)->mon == (M) && ((C)->tags & (M)->tagset[(M)->seltags]))
#define IDLE_NOTIFY_ACTIVITY    wlr_idle_notify_activity(state.idle, inputState.seat), wlr_idle_notifier_v1_notify_activity(state.idle_notifier, inputState.seat)

static int locked;
static void *exclusive_focus;

struct sc_view {
	struct wl_list link;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
};

struct sc_state
{
	struct wl_display *display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;

	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;

	struct wlr_layer_shell_v1 *layer_shell;
	struct wl_listener new_layer_shell_surface;

	struct wlr_xdg_shell *xdgShell;
	struct wl_listener xdgSurfaceListener;

	struct wl_list views;
	struct sc_view *grabbed_view;

	struct wl_list clients; /* tiling order */
	struct wl_list fstack;  /* focus order */

	struct wlr_idle *idle;
	struct wlr_idle_notifier_v1 *idle_notifier;
};

extern struct sc_state state;

typedef union {
	int i;
	uint32_t ui;
	float f;
	const void *v;
} Arg;


void motionnotify(uint32_t time);
struct sc_client *focustop(struct sc_output *m);
void focusclient(struct sc_client *c, int lift);
void arrange(struct sc_output *m);

void commitlayersurfacenotify(struct wl_listener *listener, void *data);
void destroylayersurfacenotify(struct wl_listener *listener, void *data);
void arrangelayer(struct sc_output *m, struct wl_list *list, struct wlr_box *usable_area, int exclusive);
void arrangelayers(struct sc_output *m);
void server_new_xdg_surface(struct wl_listener *listener, void *data);
void createlayersurface(struct wl_listener *listener, void *data);

void quit(const Arg *arg);
void spawn(const Arg *arg);

#endif
