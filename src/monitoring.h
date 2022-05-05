/**
 * @file monitoring.h
 * @brief Header for monitoring part of the program
 * @date 2022-01-10
 *
 * @copyright Copyright (c) 2022
 *
 * Program expose a socket other processes can connect to and
 * request data as well as requesting a calibartion
 */
#ifndef MONITORING_H
#define MONITORING_H

#include <pthread.h>
#include <oscillator-disciplining/oscillator-disciplining.h>
#include "config.h"
#include "oscillator.h"

enum monitoring_request {
	REQUEST_NONE,
	REQUEST_CALIBRATION,
	REQUEST_GNSS_START,
	REQUEST_GNSS_STOP,
};

/**
 * @brief General structure for monitoring thread
 */
struct monitoring {
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	enum monitoring_request request;
	struct od_monitoring disciplining;
	struct oscillator_ctrl ctrl_values;
	const char *oscillator_model;
	int64_t phase_error;
	int fix;
	int satellites_count;
	int leap_seconds;
	int lsChange;
	int8_t antenna_power;
	int8_t antenna_status;
	double temperature;
	int sockfd;
	bool fixOk;
	bool stop;
	bool disciplining_mode;
	bool tracking_only;
	bool phase_error_supported;
};

struct monitoring* monitoring_init(const struct config *config);
void monitoring_stop(struct monitoring *monitoring);
#endif // MONITORING_H
