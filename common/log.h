/**
 * Copyright (c) 2020 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

#include <oscillator-disciplining/oscillator-disciplining.h>

#include "ppsthread.h"

#define LOG_VERSION "0.1.0"

typedef struct {
  va_list ap;
  const char *fmt;
  const char *file;
  struct tm *time;
  void *udata;
  int line;
  int level;
} log_Event;

typedef void (*log_LogFn)(log_Event *ev);
typedef void (*log_LockFn)(bool lock, void *udata);

enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

#define log_trace(...) log_log(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  log_log(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_log(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

const char* log_level_string(int level);
void log_set_lock(log_LockFn fn, void *udata);
void log_set_level(int level);
void log_set_quiet(bool enable);
int log_add_callback(log_LogFn fn, void *udata, int level);
int log_add_fp(FILE *fp, int level);

void log_log(int level, const char *file, int line, const char *fmt, ...);
void ppsthread_log(volatile struct pps_thread_t *pps_thread, int level, const char *fmt, ...);

static inline void print_disciplining_parameters(struct disciplining_parameters *calibration, int level)
{
    log_log(level, __FILE__, __LINE__, "Calibration parameters:");
    log_log(level, __FILE__, __LINE__, "ctrl_nodes_length = %d", calibration->ctrl_nodes_length);
    log_log(level, __FILE__, __LINE__, "ctrl_load_nodes[] =");
    if (calibration->ctrl_nodes_length > 0 && calibration->ctrl_nodes_length <= CALIBRATION_POINTS_MAX)
        for (int i = 0; i < calibration->ctrl_nodes_length; i++)
            log_log(level, __FILE__, __LINE__, " %f",calibration->ctrl_load_nodes[i]);

    log_log(level, __FILE__, __LINE__, "ctrl_drift_coeffs[] =");
    if (calibration->ctrl_nodes_length > 0 && calibration->ctrl_nodes_length <= CALIBRATION_POINTS_MAX)
        for (int i = 0; i < calibration->ctrl_nodes_length; i++)
            log_log(level, __FILE__, __LINE__, " %f", calibration->ctrl_drift_coeffs[i]);
    char buff[20];
    struct tm * timeinfo;
    timeinfo = localtime(&calibration->calibration_date);
    strftime(buff, sizeof(buff), "%b %d %Y", timeinfo);
    log_log(level, __FILE__, __LINE__, "Date of calibration: %s", buff);

    log_log(level, __FILE__, __LINE__, "coarse_equilibrium = %d", calibration->coarse_equilibrium);
    log_log(level, __FILE__, __LINE__, "calibration_valid = %d", calibration->calibration_valid);

    log_log(level, __FILE__, __LINE__, "ctrl_nodes_length_factory = %d", calibration->ctrl_nodes_length_factory);
    log_log(level, __FILE__, __LINE__, "ctrl_load_nodes_factory[] =");
    if (calibration->ctrl_nodes_length_factory > 0 && calibration->ctrl_nodes_length_factory <= CALIBRATION_POINTS_MAX)
        for (int i = 0; i < calibration->ctrl_nodes_length_factory; i++)
    log_log(level, __FILE__, __LINE__, " %f", calibration->ctrl_load_nodes_factory[i]);

    log_log(level, __FILE__, __LINE__, "ctrl_drift_coeffs_factory[] =");
    if (calibration->ctrl_nodes_length_factory > 0 && calibration->ctrl_nodes_length_factory <= CALIBRATION_POINTS_MAX)
        for (int i = 0; i < calibration->ctrl_nodes_length_factory; i++)
            log_log(level, __FILE__, __LINE__, " %f", calibration->ctrl_drift_coeffs_factory[i]);

    log_log(level, __FILE__, __LINE__, "coarse_equilibrium_factory = %d", calibration->coarse_equilibrium_factory);
    log_log(level, __FILE__, __LINE__, "estimated_equilibrium_ES = %d", calibration->estimated_equilibrium_ES);

    /* Print temperature table */
    log_log(level, __FILE__, __LINE__, "Temperature compensation table:");
    for (int i = 0; i < MEAN_TEMPERATURE_ARRAY_MAX; i++) {
        if (calibration->mean_fine_over_temperature[i] != 0) {
            log_log(level, __FILE__, __LINE__, "Read mean value of %.2f in temperature range [%.2f, %.2f[",
                (float) calibration->mean_fine_over_temperature[i] / 10.0,
                (i + STEPS_BY_DEGREE * MIN_TEMPERATURE) / STEPS_BY_DEGREE,
                (i + 1 + STEPS_BY_DEGREE * MIN_TEMPERATURE) / STEPS_BY_DEGREE);
        }
    }
}

#endif
