#ifndef _input_autostart_h_included_
#define _input_autostart_h_included_

#include "../../slide.h"

struct autostart_state
{
	pid_t *pids;
	size_t len;
};

extern struct autostart_state autostartState;

void autostart_init();
void autostart_cleanup();

#endif
