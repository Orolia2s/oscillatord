#include <gps.h>
#include <errno.h>

#include "log.h"
#include "config.h"
#include "gnss.h"


#define TIMEOUT_MSEC 1


static bool gnss_data_valid(const struct gnss *gnss)
{
	if (!gnss->data.set) {
		debug("gps data not set\n");
		return false;
	}

	if (gnss->data.fix.status < STATUS_FIX) {
		debug("No gps fix\n");
		return false;
	}

	return true;
}


int gnss_init(const struct config *config, struct gnss *gnss)
{
	int ret;
	const char *gnss_device_tty;
	const char *gpsd_addr;
	const char *gpsd_port;

	gnss_device_tty = config_get(config, "gnss-device-tty");
	if (gnss_device_tty == NULL) {
		err("device-tty not defined in config %s", config->path);
		return -1;
	}

	info("GNSS device tty %s\n", gnss_device_tty);

	gpsd_addr = config_get(config, "gpsd-addr");
	if (gpsd_addr == NULL) {
		err("gpsd-addr not defined in config %s", config->path);
		return -1;
	}

	gpsd_port = config_get(config, "gpsd-port");
	if (gpsd_port == NULL) {
		err("gpsd-port not defined in config %s", config->path);
		return -1;
	}

	ret = gps_open(gpsd_addr, gpsd_port, &gnss->data);
	if (ret != 0) {
		err("gps_open %s: ", gps_errstr(ret));
		return -1;
	}

	gnss->session_open = true;
	debug("Starting gpsd session\n");

	if (gps_stream(&gnss->data, WATCH_DEVICE,
	    (void *)gnss_device_tty) != 0) {
		perr("gps_stream", errno);
		return -1;
	}

	return 0;
}

enum gnss_state gnss_get_data(struct gnss *gnss)
{
	if (!gps_waiting(&gnss->data, TIMEOUT_MSEC)) {
		debug("Waiting on gnss data\n");
		return GNSS_WAITING;
	}

	if (gps_read(&gnss->data, NULL, 0) == -1) {
		if (errno == 0)
			err("gps_read: the socket to the daemon has closed or"
					" the shared-memory segment was"
					" unavailable\n");
		else
			perr("gps_read", errno);
		return GNSS_ERROR;
	}

	if (!gnss_data_valid(gnss))
		return GNSS_INVALID;

	return GNSS_VALID;
}

void gnss_cleanup(struct gnss *gnss)
{
	if (!gnss->session_open)
		return;

	debug("Closing gpsd session\n");

	if (gps_close(&gnss->data) == -1)
		perr("gps_close", errno);

	gnss->session_open = false;
}
