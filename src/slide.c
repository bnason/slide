#define _POSIX_C_SOURCE 200112L

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#include "slide.h"
#include "utils/die.h"
#include "devices/input/input.h"
#include "devices/input/keyboard/keyboard.h"
#include "devices/output/output.h"
#include "features/autostart/autostart.h"

#include "config/output.h"

// The state of our application
struct sc_state state = {
	.xdgSurfaceListener = {.notify = server_new_xdg_surface },
	.new_layer_shell_surface = {.notify = createlayersurface},
};

int main(void)
{
	// Wayland requires XDG_RUNTIME_DIR for creating its communications socket
	if (!getenv("XDG_RUNTIME_DIR"))
		die("XDG_RUNTIME_DIR must be set");

	wlr_log_init(WLR_DEBUG, NULL);

	// The Wayland display is managed by libwayland. It handles accepting
	// clients from the Unix socket, manging Wayland globals, and so on.
	state.display = wl_display_create();

	// The backend is a wlroots feature which abstracts the underlying input and
	// output hardware. The autocreate option will choose the most suitable
	// backend based on the current environment, such as opening an X11 window
	// if an X11 server is running. The NULL argument here optionally allows you
	// to pass in a custom renderer if wlr_renderer doesn't meet your needs. The
	// backend uses the renderer, for example, to fall back to software cursors
	// if the backend does not support hardware cursors (some older GPUs
	// don't).
	state.backend = wlr_backend_autocreate(state.display, NULL);
	if (!state.backend) die("Couldn't create backend");

	// Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
	// can also specify a renderer using the WLR_RENDERER env var.
	// The renderer is responsible for defining the various pixel formats it
	// supports for shared memory, this configures that for clients.
	state.renderer = wlr_renderer_autocreate(state.backend);
	if (!state.renderer) die("Couldn't create renderer");

	// TODO: what does this do?
	wlr_renderer_init_wl_display(state.renderer, state.display);

	// Autocreates an allocator for us.
	// The allocator is the bridge between the renderer and the backend. It
	// handles the buffer creation, allowing wlroots to render onto the
	// screen
	state.allocator = wlr_allocator_autocreate(state.backend, state.renderer);
	if (!state.allocator) die("Couldn't create allocator");

	// This creates some hands-off wlroots interfaces. The compositor is
	// necessary for clients to allocate surfaces, the subcompositor allows to
	// assign the role of subsurfaces to surfaces and the data device manager
	// handles the clipboard. Each of these wlroots interfaces has room for you
	// to dig your fingers in and play with their behavior if you want. Note that
	// the clients cannot set the selection directly without compositor approval,
	// see the handling of the request_set_selection event below.
	wlr_compositor_create(state.display, 5, state.renderer);
	wlr_subcompositor_create(state.display);
	wlr_data_device_manager_create(state.display);

	output_init();

	// Create a scene graph. This is a wlroots abstraction that handles all
	// rendering and damage tracking. All the compositor author needs to do
	// is add things that should be rendered to the scene graph at the proper
	// positions and then call wlr_scene_output_commit() to render a frame if
	// necessary.
	state.scene = wlr_scene_create();
	state.scene_layout = wlr_scene_attach_output_layout(state.scene, outputState.layout);

	// TODO: document
	state.layer_shell = wlr_layer_shell_v1_create(state.display, 3);
	wl_signal_add(&state.layer_shell->events.new_surface, &state.new_layer_shell_surface);

	/* Set up our client lists and the xdg-shell. The xdg-shell is a
	 * Wayland protocol which is used for application windows. For more
	 * detail on shells, refer to the article:
	 *
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html
	 */
	wl_list_init(&state.clients);
	wl_list_init(&state.fstack);

	// Set up xdg-shell version 3. The xdg-shell is a Wayland protocol which is
	// used for application windows. For more detail on shells, refer to my
	// article:
	//
	// https://drewdevault.com/2018/07/29/Wayland-shells.html
	wl_list_init(&state.views);
	state.xdgShell = wlr_xdg_shell_create(state.display, 3);
	wl_signal_add(&state.xdgShell->events.new_surface, &state.xdgSurfaceListener);

	input_init();

	// Add a Unix socket to the Wayland display.
	const char *socket = wl_display_add_socket_auto(state.display);
	if (!socket) {
		wlr_backend_destroy(state.backend);
		return 1;
	}

	// Start the backend. This will enumerate outputs and inputs, become the DRM
	// master, etc
	if (!wlr_backend_start(state.backend))
	{
		wlr_backend_destroy(state.backend);
		wl_display_destroy(state.display);
		die("Failed to start backend");
	}

	// Set the WAYLAND_DISPLAY environment variable to our socket and run the
	// startup command if requested.
	setenv("WAYLAND_DISPLAY", socket, true);

	// Initialize Features
	autostart_init();

	// Run the Wayland event loop. This does not return until you exit the
	// compositor. Starting the backend rigged up all of the necessary event
	// loop configuration to listen to libinput events, DRM events, generate
	// frame events at the refresh rate, and so on.
	wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);
	wl_display_run(state.display);

	// Once wl_display_run returns, we destroy all clients then shut down the
	// server.
	autostart_cleanup();
	// input_destroy();
	// output_destroy();

	// TODO: these should be included in the destroy functions
	wl_display_destroy_clients(state.display);
	// wlr_scene_node_destroy(&server.scene->tree.node);
	// wlr_xcursor_manager_destroy(server.cursor_mgr);
	// wlr_output_layout_destroy(server.output_layout);
	wl_display_destroy(state.display);

	return 0;
}


void createlayersurface(struct wl_listener *listener, void *data)
{
	struct wlr_layer_surface_v1 *wlr_layer_surface = data;
	struct sc_layer_surface *layersurface;
	struct wlr_layer_surface_v1_state old_state;
	struct wlr_scene_tree *l = outputState.layers[layermap[wlr_layer_surface->pending.layer]];

	if (!wlr_layer_surface->output)
		wlr_layer_surface->output = outputState.monitorSelected ? outputState.monitorSelected->output : NULL;

	if (!wlr_layer_surface->output) {
		wlr_layer_surface_v1_destroy(wlr_layer_surface);
		return;
	}

	layersurface = calloc(1, sizeof(struct sc_layer_surface));
	layersurface->type = LayerShell;
	LISTEN(&wlr_layer_surface->surface->events.commit, &layersurface->surface_commit, commitlayersurfacenotify);
	LISTEN(&wlr_layer_surface->events.destroy, &layersurface->destroy, destroylayersurfacenotify);
	// LISTEN(&wlr_layer_surface->events.map, &layersurface->map, maplayersurfacenotify);
	// LISTEN(&wlr_layer_surface->events.unmap, &layersurface->unmap, unmaplayersurfacenotify);

	layersurface->layer_surface = wlr_layer_surface;
	layersurface->mon = wlr_layer_surface->output->data;
	wlr_layer_surface->data = layersurface;

	layersurface->scene_layer = wlr_scene_layer_surface_v1_create(l, wlr_layer_surface);
	layersurface->scene = layersurface->scene_layer->tree;
	layersurface->popups = wlr_layer_surface->surface->data = wlr_scene_tree_create(l);

	layersurface->scene->node.data = layersurface;

	wl_list_insert(&layersurface->mon->layers[wlr_layer_surface->pending.layer], &layersurface->link);

	// Temporarily set the layer's current state to pending
	// so that we can easily arrange it
	old_state = wlr_layer_surface->current;
	wlr_layer_surface->current = wlr_layer_surface->pending;
	layersurface->mapped = 1;
	arrangelayers(layersurface->mon);
	wlr_layer_surface->current = old_state;
}


void commitlayersurfacenotify(struct wl_listener *listener, void *data)
{
	struct sc_layer_surface *layersurface = wl_container_of(listener, layersurface, surface_commit);
	struct wlr_layer_surface_v1 *wlr_layer_surface = layersurface->layer_surface;
	struct wlr_output *wlr_output = wlr_layer_surface->output;
	struct wlr_scene_tree *layer = outputState.layers[layermap[wlr_layer_surface->current.layer]];

	/* For some reason this layersurface have no monitor, this can be because
	 * its monitor has just been destroyed */
	if (!wlr_output || !(layersurface->mon = wlr_output->data))
		return;

	if (layer != layersurface->scene->node.parent) {
		wlr_scene_node_reparent(&layersurface->scene->node, layer);
		wlr_scene_node_reparent(&layersurface->popups->node, layer);
		wl_list_remove(&layersurface->link);
		wl_list_insert(&layersurface->mon->layers[wlr_layer_surface->current.layer], &layersurface->link);
	}

	if (wlr_layer_surface->current.layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP)
		wlr_scene_node_reparent(&layersurface->popups->node, outputState.layers[LyrTop]);

	if (wlr_layer_surface->current.committed == 0 && layersurface->mapped == wlr_layer_surface->surface->mapped)
		return;

	layersurface->mapped = wlr_layer_surface->surface->mapped;

	arrangelayers(layersurface->mon);
}


void arrange(struct sc_output *m)
{
	struct sc_client *c;
	wl_list_for_each(c, &state.clients, link)
		if (c->mon == m)
			wlr_scene_node_set_enabled(&c->scene->node, VISIBLEON(c, m));

	wlr_scene_node_set_enabled(&m->fullscreen_bg->node, (c = focustop(m)) && c->isfullscreen);

	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, LENGTH(m->ltsymbol));

	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);

	motionnotify(0);
	// checkidleinhibitor(NULL);
}


void arrangelayer(struct sc_output *m, struct wl_list *list, struct wlr_box *usable_area, int exclusive)
{
	struct sc_layer_surface *layersurface;
	struct wlr_box full_area = m->m;

	wl_list_for_each(layersurface, list, link) {
		struct wlr_layer_surface_v1 *wlr_layer_surface = layersurface->layer_surface;
		struct wlr_layer_surface_v1_state *state = &wlr_layer_surface->current;

		if (exclusive != (state->exclusive_zone > 0))
			continue;

		wlr_scene_layer_surface_v1_configure(layersurface->scene_layer, &full_area, usable_area);
		wlr_scene_node_set_position(&layersurface->popups->node, layersurface->scene->node.x, layersurface->scene->node.y);
		layersurface->geom.x = layersurface->scene->node.x;
		layersurface->geom.y = layersurface->scene->node.y;
	}
}


void arrangelayers(struct sc_output *m)
{
	int i;
	struct wlr_box usable_area = m->m;
	uint32_t layers_above_shell[] = {
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
	};

	struct sc_layer_surface *layersurface;
	if (!m->output->enabled)
		return;

	/* Arrange exclusive surfaces from top->bottom */
	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 1);

	if (memcmp(&usable_area, &m->w, sizeof(struct wlr_box))) {
		m->w = usable_area;
		arrange(m);
	}

	/* Arrange non-exlusive surfaces from top->bottom */
	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 0);

	/* Find topmost keyboard interactive layer, if such a layer exists */
	for (i = 0; i < LENGTH(layers_above_shell); i++) {
		wl_list_for_each_reverse(layersurface, &m->layers[layers_above_shell[i]], link) {
			if (!locked && layersurface->layer_surface->current.keyboard_interactive && layersurface->mapped) {
				/* Deactivate the focused client. */
				focusclient(NULL, 0);
				exclusive_focus = layersurface;
				// client_notify_enter(layersurface->layer_surface->surface, wlr_seat_get_keyboard(inputState.seat));
				return;
			}
		}
	}
}

// We probably should change the name of this, it sounds like
// will focus the topmost client of this mon, when actually will
// only return that client
struct sc_client *focustop(struct sc_output *m)
{
	struct sc_client *c;
	wl_list_for_each(c, &state.fstack, flink)
		if (VISIBLEON(c, m))
			return c;
	return NULL;
}


void focusclient(struct sc_client *c, int lift)
{
	// struct wlr_surface *old = inputState.seat->keyboard_state.focused_surface;
	// int i, unused_lx, unused_ly, old_client_type;
	// struct sc_client *old_c = NULL;
	// struct sc_layer_surface *old_l = NULL;

	// if (locked)
	// 	return;

	// /* Raise client in stacking order if requested */
	// if (c && lift)
	// 	wlr_scene_node_raise_to_top(&c->scene->node);

	// if (c && client_surface(c) == old)
	// 	return;

	// if ((old_client_type = toplevel_from_wlr_surface(old, &old_c, &old_l)) == XDGShell) {
	// 	struct wlr_xdg_popup *popup, *tmp;
	// 	wl_list_for_each_safe(popup, tmp, &old_c->surface.xdg->popups, link)
	// 		wlr_xdg_popup_destroy(popup);
	// }

	// /* Put the new client atop the focus stack and select its monitor */
	// if (c && !client_is_unmanaged(c)) {
	// 	wl_list_remove(&c->flink);
	// 	wl_list_insert(&state.fstack, &c->flink);
	// 	outputState.monitorSelected = c->mon;
	// 	c->isurgent = 0;
	// 	client_restack_surface(c);

	// 	/* Don't change border color if there is an exclusive focus or we are
	// 	 * handling a drag operation */
	// 	if (!exclusive_focus && !inputState.seat->drag)
	// 		for (i = 0; i < 4; i++)
	// 			wlr_scene_rect_set_color(c->border[i], focuscolor);
	// }

	// /* Deactivate old client if focus is changing */
	// if (old && (!c || client_surface(c) != old)) {
	// 	/* If an overlay is focused, don't focus or activate the client,
	// 	 * but only update its position in fstack to render its border with focuscolor
	// 	 * and focus it after the overlay is closed. */
	// 	if (old_client_type == LayerShell && wlr_scene_node_coords(
	// 				&old_l->scene->node, &unused_lx, &unused_ly)
	// 			&& old_l->layer_surface->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
	// 		return;
	// 	} else if (old_c && old_c == exclusive_focus && client_wants_focus(old_c)) {
	// 		return;
	// 	/* Don't deactivate old client if the new one wants focus, as this causes issues with winecfg
	// 	 * and probably other clients */
	// 	} else if (old_c && !client_is_unmanaged(old_c) && (!c || !client_wants_focus(c))) {
	// 		for (i = 0; i < 4; i++)
	// 			wlr_scene_rect_set_color(old_c->border[i], bordercolor);

	// 		client_activate_surface(old, 0);
	// 	}
	// }
	// printstatus();

	// if (!c) {
	// 	/* With no client, all we have left is to clear focus */
	// 	wlr_seat_keyboard_notify_clear_focus(inputState.seat);
	// 	return;
	// }

	// /* Change cursor surface */
	// motionnotify(0);

	// /* Have a client, so focus its top-level wlr_surface */
	// client_notify_enter(client_surface(c), wlr_seat_get_keyboard(inputState.seat));

	// /* Activate the new client */
	// client_activate_surface(client_surface(c), 1);
}


void motionnotify(uint32_t time)
{
	// double sx = 0, sy = 0;
	// struct sc_client *c = NULL, *w = NULL;
	// struct sc_layer_surface *l = NULL;
	// int type;
	// struct wlr_surface *surface = NULL;

	// /* time is 0 in internal calls meant to restore pointer focus. */
	// if (time) {
	// 	IDLE_NOTIFY_ACTIVITY;

	// 	/* Update selmon (even while dragging a window) */
	// 	if (sloppyfocus)
	// 		selmon = xytomon(cursor->x, cursor->y);
	// }

	// /* Update drag icon's position */
	// wlr_scene_node_set_position(&drag_icon->node, cursor->x, cursor->y);

	// /* If we are currently grabbing the mouse, handle and return */
	// if (cursor_mode == CurMove) {
	// 	/* Move the grabbed client to the new position. */
	// 	resize(grabc, (struct wlr_box) {
	// 		.x = cursor->x - grabcx,
	// 		.y = cursor->y - grabcy,
	// 		.width = grabc->geom.width,
	// 		.height = grabc->geom.height
	// 	}, 1, 1);
	// 	return;
	// } else if (cursor_mode == CurResize) {
	// 	resize(grabc, (struct wlr_box){
	// 		.x = grabc->geom.x,
	// 		.y = grabc->geom.y,
	// 		.width = cursor->x - grabc->geom.x,
	// 		.height = cursor->y - grabc->geom.y
	// 	}, 1, 1);
	// 	return;
	// }

	// /* Find the client under the pointer and send the event along. */
	// xytonode(cursor->x, cursor->y, &surface, &c, NULL, &sx, &sy);

	// if (cursor_mode == CurPressed && !seat->drag) {
	// 	if ((type = toplevel_from_wlr_surface(
	// 			 seat->pointer_state.focused_surface, &w, &l)) >= 0) {
	// 		c = w;
	// 		surface = seat->pointer_state.focused_surface;
	// 		sx = cursor->x - (type == LayerShell ? l->geom.x : w->geom.x);
	// 		sy = cursor->y - (type == LayerShell ? l->geom.y : w->geom.y);
	// 	}
	// }

	// /* If there's no client surface under the cursor, set the cursor image to a
	//  * default. This is what makes the cursor image appear when you move it
	//  * off of a client or over its border. */
	// if (!surface && !seat->drag && (!cursor_image || strcmp(cursor_image, "left_ptr")))
	// 	wlr_xcursor_manager_set_cursor_image(cursor_mgr, (cursor_image = "left_ptr"), cursor);

	// pointerfocus(c, surface, sx, sy, time);
}


void destroylayersurfacenotify(struct wl_listener *listener, void *data)
{
	struct sc_layer_surface *layersurface = wl_container_of(listener, layersurface, destroy);

	wl_list_remove(&layersurface->link);
	wl_list_remove(&layersurface->destroy.link);
	wl_list_remove(&layersurface->map.link);
	wl_list_remove(&layersurface->unmap.link);
	wl_list_remove(&layersurface->surface_commit.link);
	wlr_scene_node_destroy(&layersurface->scene->node);
	free(layersurface);
}

// Called when the surface is mapped, or ready to display on-screen.
void xdg_toplevel_map(struct wl_listener *listener, void *data) {
	struct sc_view *view = wl_container_of(listener, view, map);

	if (view == NULL)
		wlr_log(WLR_DEBUG, "xdg_toplevel_map view is null");

	wl_list_insert(&state.views, &view->link);

	// focus_view(view, view->xdg_toplevel->base->surface);
}

// Called when the surface is unmapped, and should no longer be shown.
void xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
	struct sc_view *view = wl_container_of(listener, view, unmap);

	// /* Reset the cursor mode if the grabbed view was unmapped. */
	// if (view == state.grabbed_view) {
	// 	reset_cursor_mode(state);
	// }

	wl_list_remove(&view->link);
}

// Called when the surface is destroyed and should never be shown again.
void xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
	struct sc_view *view = wl_container_of(listener, view, destroy);

	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->destroy.link);
	// wl_list_remove(&view->request_move.link);
	// wl_list_remove(&view->request_resize.link);
	// wl_list_remove(&view->request_maximize.link);
	// wl_list_remove(&view->request_fullscreen.link);

	free(view);
}


// This event is raised when wlr_xdg_shell receives a new xdg surface from a
// client, either a toplevel (application window) or popup.
void server_new_xdg_surface(struct wl_listener *listener, void *data) {
	struct wlr_xdg_surface *xdg_surface = data;

	// We must add xdg popups to the scene graph so they get rendered. The
	// wlroots scene graph provides a helper for this, but to use it we must
	// provide the proper parent scene node of the xdg popup. To enable this,
	// we always set the user data field of xdg_surfaces to the corresponding
	// scene node.
	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
		struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(xdg_surface->popup->parent);
		// assert(parent != NULL);

		struct wlr_scene_tree *parent_tree = parent->data;
		xdg_surface->data = wlr_scene_xdg_surface_create(parent_tree, xdg_surface);

		return;
	}
	// assert(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);

	// Allocate a sc_view for this surface
	struct sc_view *view = calloc(1, sizeof(struct sc_view));
	*view = (struct sc_view){
		.xdg_toplevel = xdg_surface->toplevel,
		.scene_tree = wlr_scene_xdg_surface_create(&outputState.scene->tree, xdg_surface->toplevel->base),
		.map = {.notify = xdg_toplevel_map },
		.unmap = {.notify = xdg_toplevel_unmap },
		.destroy = {.notify = xdg_toplevel_destroy },
		// .request_move = { .notify = xdg_toplevel_request_move },
		// .request_resize = { .notify = xdg_toplevel_request_resize },
		// .request_maximize = { .notify = xdg_toplevel_request_maximize },
		// .request_fullscreen = { .notify = xdg_toplevel_request_fullscreen },
	};
	view->scene_tree->node.data = view;
	xdg_surface->data = view->scene_tree;

	// Listen to the various events it can emit
	wl_signal_add(&xdg_surface->surface->events.map, &view->map);
	wl_signal_add(&xdg_surface->surface->events.unmap, &view->unmap);
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

	// // cotd
	// struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
	// wl_signal_add(&toplevel->events.request_move, &view->request_move);
	// wl_signal_add(&toplevel->events.request_resize, &view->request_resize);
	// wl_signal_add(&toplevel->events.request_maximize, &view->request_maximize);
	// wl_signal_add(&toplevel->events.request_fullscreen, &view->request_fullscreen);
}

void quit(const Arg *arg)
{
	wl_display_terminate(state.display);
}

void spawn(const Arg *arg)
{
	if (fork() == 0) {
		dup2(STDERR_FILENO, STDOUT_FILENO);
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		die("slide: execvp %s failed:", ((char **)arg->v)[0]);
	}
}
