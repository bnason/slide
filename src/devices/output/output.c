#define _POSIX_C_SOURCE 200112L

#include "output.h"
#include "../../slide.h"
#include "../input/keyboard/keyboard.h"

#include <stdlib.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/log.h>

// struct sc_output_state outputState = { 0 };
struct sc_output_state outputState = {
	.newListener = {.notify = new_output_notify},
};

void output_init()
{
	wlr_log(WLR_DEBUG, "output_init");

	// Creates an output layout, which a wlroots utility for working with an
	// arrangement of screens in a physical layout.
	outputState.layout = wlr_output_layout_create();

	// Configure a listener to be notified when new outputs are available on the
	// backend.
	wl_list_init(&outputState.monitors);
	wl_signal_add(&state.backend->events.new_output, &outputState.newListener);

	// Create a scene graph. This is a wlroots abstraction that handles all
	// rendering and damage tracking. All the compositor author needs to do
	// is add things that should be rendered to the scene graph at the proper
	// positions and then call wlr_scene_output_commit() to render a frame if
	// necessary.
	outputState.scene = wlr_scene_create();
	outputState.sceneLayout = wlr_scene_attach_output_layout(outputState.scene, outputState.layout);

	wlr_log(WLR_DEBUG, "output_init done");
}

void output_frame_notify(struct wl_listener *listener, void *data)
{
	wlr_log(WLR_DEBUG, "output_frame_notify");

	struct sc_output *output = wl_container_of(listener, output, frame);
	struct wlr_scene *scene = state.scene;
	struct wlr_output *wlr_output = output->output;

	struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(scene, output->output);

	// Render the scene if needed and commit the output
	wlr_scene_output_commit(scene_output, NULL);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

void output_remove_notify(struct wl_listener *listener, void *data)
{
	wlr_log(WLR_DEBUG, "output_remove_notify");

	struct sc_output *output = wl_container_of(listener, output, destroy);

	wlr_log(WLR_DEBUG, "Output removed");

	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->request_state.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->link);

	free(output);

	if (wl_list_empty(&outputState.monitors)) {
		wlr_log(WLR_DEBUG, "No more monitors, destroying display");
		wl_display_terminate(state.display);
	}
}

#define allocobj(p, ...) ({typeof(*p)*_p=malloc(sizeof*_p);if(_p)*_p=(typeof(*_p)){__VA_ARGS__};_p;})

// This event is raised by the backend when a new output (aka a display or
// monitor) becomes available.
void new_output_notify(struct wl_listener *listener, void *data)
{
	wlr_log(WLR_DEBUG, "new_output_notify");

	struct wlr_output *wlrOutput = data;

	// Configures the output created by the backend to use our allocator
	// and our renderer. Must be done once, before commiting the output
	wlr_output_init_render(wlrOutput, state.allocator, state.renderer);

	// The output may be disabled, switch it on.
	struct wlr_output_state wlrOutputState;
	wlr_output_state_init(&wlrOutputState);
	wlr_output_state_set_enabled(&wlrOutputState, true);

	// Some backends don't have modes. DRM+KMS does, and we need to set a mode
	// before we can use the output. The mode is a tuple of (width, height,
	// refresh rate), and each monitor supports only a specific set of modes. We
	// just pick the monitor's preferred mode, a more sophisticated compositor
	// would let the user configure it.
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlrOutput);
	if (mode != NULL)
		wlr_output_state_set_mode(&wlrOutputState, mode);

	// Atomically applies the new output state.
	wlr_output_commit_state(wlrOutput, &wlrOutputState);
	wlr_output_state_finish(&wlrOutputState);

	// Allocates and configures our state for this output
	struct sc_output *output = calloc(1, sizeof(struct sc_output));
	*output = (struct sc_output){
		.color = { 1.0, 0.0, 0.0, 1.0 },
		.dec = 0,
		.last_frame = { 0 },
		.output = wlrOutput,
		.frame = {.notify = output_frame_notify },
		.destroy = {.notify = output_remove_notify },
		.request_state = {.notify = output_request_state},
	};

	clock_gettime(CLOCK_MONOTONIC, &output->last_frame);

	// Sets up a listener for the frame event.
	wl_signal_add(&wlrOutput->events.frame, &output->frame);

	// Sets up a listener for the state request event.
	wl_signal_add(&wlrOutput->events.request_state, &output->request_state);

	// Sets up a listener for the destroy event.
	wl_signal_add(&wlrOutput->events.destroy, &output->destroy);

	wl_list_insert(&outputState.monitors, &output->link);

	// Adds this to the output layout. The add_auto function arranges outputs
	// from left-to-right in the order they appear. A more sophisticated
	// compositor would let the user configure the arrangement of outputs in the
	// layout.
	//
	// The output layout utility automatically adds a wl_output global to the
	// display, which Wayland clients can see to find out information about the
	// output (such as DPI, scale factor, manufacturer, etc).
	struct wlr_output_layout_output *l_output = wlr_output_layout_add_auto(outputState.layout, wlrOutput);
	struct wlr_scene_output *scene_output = wlr_scene_output_create(state.scene, wlrOutput);
	wlr_scene_output_layout_add_output(state.scene_layout, l_output, scene_output);
}

void output_request_state(struct wl_listener *listener, void *data)
{
	// This function is called when the backend requests a new state for
	// the output. For example, Wayland and X11 backends request a new mode
	// when the output window is resized.
	struct sc_output *output = wl_container_of(listener, output, request_state);
	const struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(output->output, event->state);
}
