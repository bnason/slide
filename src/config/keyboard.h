#ifndef _config_keyboard_h_included_
#define _config_keyboard_h_included_

#include "../devices/input/keyboard/keyboard.h"

#define MODKEY WLR_MODIFIER_LOGO

// Commands
static const char *termcmd[] = { "alacritty", NULL };
static const char *menucmd[] = {
	"/home/brandonnason/Repositories/github.com/lbonn/rofi/build/rofi",
	"-modi", "drun",
	"-show", "drun",
	"--display-drun", "\"Run\"",
	"-show-icons",
	"-theme", "/home/brandonnason/.config/awesome/configuration/mirage/rofi.rasi"
};

// Key shortcuts
const Key keys[] = {
	{ MODKEY | WLR_MODIFIER_SHIFT, XKB_KEY_Q, quit, {0} },
	{ MODKEY , XKB_KEY_d, spawn, {.v = menucmd} },
	{ MODKEY , XKB_KEY_Return, spawn, {.v = termcmd} },
};

#endif
