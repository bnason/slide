#ifndef _output_h_included_
#define _output_h_included_

#include <time.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_scene.h>

enum { XDGShell, LayerShell, X11Managed, X11Unmanaged }; /* client types */
enum { LyrBg, LyrBottom, LyrTile, LyrFloat, LyrFS, LyrTop, LyrOverlay, LyrBlock, NUM_LAYERS }; /* scene layers */
static const int layermap[] = { LyrBg, LyrBottom, LyrTop, LyrOverlay };

struct sc_layer_surface
{
	/* Must keep these three elements in this order */
	unsigned int type; /* LayerShell */
	struct wlr_box geom;
	struct sc_output *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_tree *popups;
	struct wlr_scene_layer_surface_v1 *scene_layer;
	struct wl_list link;
	int mapped;
	struct wlr_layer_surface_v1 *layer_surface;

	struct wl_listener destroy;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;
};

struct sc_layout {
	const char *symbol;
	void (*arrange)(struct sc_output *);
};

struct sc_output
{
	struct wl_list link;
	struct wlr_output *output;
	struct wl_listener frame;
	struct wl_listener destroy;
	struct wl_listener request_state;

	struct timespec last_frame;
	float color[4];
	int dec;

	struct wl_list layers[4]; /* LayerSurface::link */
	struct wlr_box m; /* monitor area, layout-relative */
	struct wlr_box w; /* window area, layout-relative */
	struct wlr_scene_rect *fullscreen_bg; /* See createmon() for info */
	char ltsymbol[16];
	struct sc_layout *lt[2];
	unsigned int sellt;
	unsigned int seltags;
	uint32_t tagset[2];
};

struct sc_output_state
{
	struct wl_list monitors;
	struct sc_output *monitorSelected;
	struct wl_listener newListener;
	struct wlr_output_layout *layout;
	struct wlr_scene *scene;
	struct wlr_scene_output_layout *sceneLayout;
	struct wlr_scene_tree *layers[NUM_LAYERS];
};

struct sc_client {
	/* Must keep these three elements in this order */
	unsigned int type; /* XDGShell or X11* */
	struct wlr_box geom; /* layout-relative, includes border */
	struct sc_output *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_rect *border[4]; /* top, bottom, left, right */
	struct wlr_scene_tree *scene_surface;
	struct wl_list link;
	struct wl_list flink;
	union {
		struct wlr_xdg_surface *xdg;
		struct wlr_xwayland_surface *xwayland;
	} surface;
	struct wl_listener commit;
	struct wl_listener map;
	struct wl_listener maximize;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener set_title;
	struct wl_listener fullscreen;
	struct wlr_box prev; /* layout-relative, includes border */
#ifdef XWAYLAND
	struct wl_listener activate;
	struct wl_listener configure;
	struct wl_listener set_hints;
#endif
	unsigned int bw;
	uint32_t tags;
	int isfloating, isurgent, isfullscreen;
	uint32_t resize; /* configure serial of a pending resize */
};

extern struct sc_output_state outputState;

void output_init();
void output_frame_notify(struct wl_listener *listener, void *data);
void output_remove_notify(struct wl_listener *listener, void *data);
void new_output_notify(struct wl_listener *listener, void *data);
void output_request_state(struct wl_listener *listener, void *data);

#endif
