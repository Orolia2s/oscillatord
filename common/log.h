#ifndef LOG_H_
#define LOG_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <errno.h>

#define ERR "<3>"
#define WARN "<4>"
#define INFO "<6>"
#define DEBUG "<7>"

extern bool log_debug_enabled;

/* all those logging functions preserve the value of errno */
#define err(...) log(ERR __VA_ARGS__)
#define warn(...) log(WARN __VA_ARGS__)
#define info(...) log(INFO __VA_ARGS__)
#define debug(...) do { \
	if (log_debug_enabled) \
		log(DEBUG __VA_ARGS__); \
} while (0);

#define perr(f, e) err("%s: %s", f, strerror(abs(e)))

#define log(...) do { \
	int __old_errno = errno; \
	fprintf(stderr, __VA_ARGS__); \
	errno = __old_errno; \
} while (0);
void log_enable_debug(bool enable_debug);

#endif /* LOG_H_ */
