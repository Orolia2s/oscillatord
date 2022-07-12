/**
 * @file monitoring.h
 * @brief Header for monitoring part of the program
 * @date 2022-01-10
 *
 * @copyright Copyright (c) 2022
 *
 * Program expose a socket other processes can connect to and
 * request data as well as requesting a calibration
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
	REQUEST_READ_EEPROM,
	REQUEST_SAVE_EEPROM,
	REQUEST_FAKE_HOLDOVER_START,
	REQUEST_FAKE_HOLDOVER_STOP,
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
	struct oscillator_attributes osc_attributes;
	const char *oscillator_model;
	struct devices_path devices_path;
	int64_t phase_error;
	int fix;
	int satellites_count;
	float survey_in_position_error;
	int leap_seconds;
	int lsChange;
	int8_t antenna_power;
	int8_t antenna_status;
	int sockfd;
	bool fixOk;
	bool stop;
	bool disciplining_mode;
	bool phase_error_supported;
};

struct monitoring* monitoring_init(const struct config *config, struct devices_path *devices_path);
void monitoring_stop(struct monitoring *monitoring);
#endif // MONITORING_H
