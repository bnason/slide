#ifndef _input_keyboard_h_included_
#define _input_keyboard_h_included_

#include "../../../slide.h"

#define CLEANMASK(mask) (mask & ~WLR_MODIFIER_CAPS)
#define CHVT(n) { WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT,XKB_KEY_XF86Switch_VT_##n, chvt, {.ui = (n)} }

struct keyboard
{
	struct wlr_keyboard *wlr_keyboard;
	struct wl_listener key;
	struct wl_listener destroy;
};

// typedef union Arg Arg;
typedef struct {
	uint32_t mod;
	xkb_keysym_t keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

void keyboard_init(struct wlr_input_device *device);
void keyboard_key_notify(struct wl_listener *listener, void *data);
void keyboard_destroy_notify(struct wl_listener *listener, void *data);
int keybinding(uint32_t mods, xkb_keysym_t sym);

#endif
