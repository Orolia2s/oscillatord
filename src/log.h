#ifndef LOG_H_
#define LOG_H_
#include <stdio.h>
#include <string.h>

#define ERR "<3>"
#define WARN "<4>"
#define INFO "<6>"
#define DEBUG "<7>"

#define err(...) log(ERR __VA_ARGS__)
#define warn(...) log(WARN __VA_ARGS__)
#define info(...) log(INFO __VA_ARGS__)
#ifdef FPTD_DEBUG
#define debug(...) log(DEBUG __VA_ARGS__)
#else /* FPTD_DEBUG */
#define debug(...) do { } while (0)
#endif /* FPTD_DEBUG */

#define perr(f, e) err("%s: %s", f, strerror(abs(e)))

#define log(...) fprintf(stderr, __VA_ARGS__)

#endif /* LOG_H_ */
