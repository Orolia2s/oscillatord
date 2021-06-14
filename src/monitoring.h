#ifndef MONITORING_H
#define MONITORING_H

#include <pthread.h>
#include "config.h"

enum monitoring_request {
	REQUEST_NONE,
	REQUEST_CALIBRATION,
};

struct monitoring {
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	enum monitoring_request request;
	int status;
	int phase_error;
	int sockfd;
	bool stop;
};

struct monitoring* monitoring_init(const struct config *config);
void monitoring_stop(struct monitoring *monitoring);
#endif // MONITORING_H
