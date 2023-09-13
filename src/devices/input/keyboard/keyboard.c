#define _POSIX_C_SOURCE 200112L

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/util/log.h>

#include "keyboard.h"
#include "../../../slide.h"
#include "../../../config/keyboard.h"
#include "../../../utils/die.h"

void keyboard_init(struct wlr_input_device *device)
{
	wlr_log(WLR_DEBUG, "keyboard_init");

	struct keyboard *keyboard = calloc(1, sizeof(struct keyboard));
	*keyboard = (struct keyboard) {
		.wlr_keyboard = wlr_keyboard_from_input_device(device),
		.destroy = {.notify = keyboard_destroy_notify },
		.key = {.notify = keyboard_key_notify },
	};

	wl_signal_add(&device->events.destroy, &keyboard->destroy);
	wl_signal_add(&keyboard->wlr_keyboard->events.key, &keyboard->key);

	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!context) die("Failed to create XKB context");

	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!keymap) die("Failed to create XKB keymap");

	wlr_keyboard_set_keymap(keyboard->wlr_keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
}

void keyboard_key_notify(struct wl_listener *listener, void *data)
{
	wlr_log(WLR_DEBUG, "keyboard_key_notify");

	struct keyboard *keyboard = wl_container_of(listener, keyboard, key);
	struct sc_state *state = state;
	struct wlr_keyboard_key_event *event = data;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;

	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;

	int nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode, &syms);

	uint32_t mods = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);

	for (int i = 0; i < nsyms; i++)
		keybinding(mods, syms[i]);

	// for (int i = 0; i < nsyms; i++)
	// {
	// 	xkb_keysym_t sym = syms[i];
	// 	if (sym == XKB_KEY_Escape)
	// 	{
	// 		wl_display_terminate(state->display);
	// 	}
	// }
}

void keyboard_destroy_notify(struct wl_listener *listener, void *data)
{
	wlr_log(WLR_DEBUG, "keyboard_destroy_notify");

	struct keyboard *keyboard = wl_container_of(listener, keyboard, destroy);

	wl_list_remove(&keyboard->destroy.link);
	wl_list_remove(&keyboard->key.link);

	free(keyboard);
}

int keybinding(uint32_t mods, xkb_keysym_t sym)
{
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its own
	 * processing.
	 */
	int handled = 0;
	const Key *k;
	for (k = keys; k < END(keys); k++) {
		if (CLEANMASK(mods) == CLEANMASK(k->mod) &&
			sym == k->keysym && k->func) {
			k->func(&k->arg);
			handled = 1;
		}
	}
	return handled;
}
