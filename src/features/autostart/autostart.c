#define _POSIX_C_SOURCE 200112L

#include "autostart.h"
#include "../../slide.h"
#include "../../utils/die.h"

#include "../../config/autostart.h"

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <wlr/util/log.h>

struct autostart_state autostartState = {
	.len = 0,
};

void autostart_init()
{
	wlr_log(WLR_DEBUG, "autostart_init");

	const char *const *p;
	size_t i = 0;

	// count entries
	for (p = autostart; *p; autostartState.len++, p++)
		while (*++p);

	autostartState.pids = calloc(autostartState.len, sizeof(pid_t));
	for (p = autostart; *p; i++, p++) {
		if ((autostartState.pids[i] = fork()) == 0) {
			wlr_log(WLR_DEBUG, "starting: %s", p[i]);
			setsid();
			execvp(*p, (char *const *)p);
			die("slide: execvp %s:", *p);
		}

		// skip arguments
		while (*++p);
	}
}

void autostart_cleanup()
{
	size_t i;

	// kill child processes
	for (i = 0; i < autostartState.len; i++) {
		if (0 < autostartState.pids[i]) {
			kill(autostartState.pids[i], SIGTERM);
			waitpid(autostartState.pids[i], NULL, 0);
		}
	}
}
