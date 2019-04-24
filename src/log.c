#include <stdbool.h>

#include "log.h"

bool log_debug_enabled = false;

void log_enable_debug(bool enable_debug)
{
	log_debug_enabled = enable_debug;
	if (log_debug_enabled)
		debug("debug log level enabled\n");
}
