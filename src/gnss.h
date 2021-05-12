#ifndef OSCILLATORD_GNSS_H
#define OSCILLATORD_GNSS_H

#include <gps.h>

#include "config.h"
#include <ubloxcfg/ff_rx.h>

enum gnss_state {
	GNSS_VALID,
	GNSS_INVALID,
	GNSS_WAITING,
	GNSS_ERROR,
};

struct gnss {
	bool session_open;
	RX_t *rx;
	struct gps_data_t data;
	time_t time;
};

int gnss_init(const struct config *config, struct gnss *gnss);
enum gnss_state gnss_get_data(struct gnss *gnss);
void gnss_cleanup(struct gnss *gnss);

#endif
