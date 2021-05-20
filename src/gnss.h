#ifndef OSCILLATORD_GNSS_H
#define OSCILLATORD_GNSS_H

#include <gps.h>

#include "config.h"
#include <ubloxcfg/ff_rx.h>
#include <pthread.h>
struct gnss_data {
	time_t time;
	int fix;
	bool valid;
};

struct gnss {
	bool session_open;
	RX_t *rx;
	struct gnss_data data;
	pthread_t thread;
	pthread_mutex_t mutex_data;
	bool stop;
};

int gnss_init(const struct config *config, struct gnss *gnss);
struct gnss_data gnss_get_data(struct gnss *gnss);

#endif
