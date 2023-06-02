#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "utils.h"
#include "gnss.h"
#include "gnss-config.h"
#include "log.h"
#include "f9_defvalsets.h"

#include <time.h>

static struct gps_context_t context;
struct devices_path devices_path = { 0 };



static int get_devices_path_from_sysfs(struct config *config, struct devices_path *devices_path)
{
	const char *sysfs_path;
	DIR * ocp_dir;

	sysfs_path = config_get(config, "sysfs-path");
	if (sysfs_path == NULL) {
		log_error("No sysfs-path provided in oscillatord config file !");
		return -EINVAL;
	}
	log_info("Scanning sysfs path %s", sysfs_path);

	ocp_dir = opendir(sysfs_path);
	struct dirent * entry = readdir(ocp_dir);
	while (entry != NULL) {
		if (strcmp(entry->d_name, "mro50") == 0) {
			find_dev_path(sysfs_path, entry, devices_path->mro_path);
			log_debug("mro50 device detected: %s", devices_path->mro_path);
		} else if (strcmp(entry->d_name, "ptp") == 0) {
			find_dev_path(sysfs_path, entry, devices_path->ptp_path);
			log_debug("ptp clock device detected: %s", devices_path->ptp_path);
		} else if (strcmp(entry->d_name, "pps") == 0) {
			find_dev_path(sysfs_path, entry, devices_path->pps_path);
			log_debug("pps device detected: %s", devices_path->pps_path);
		} else if (strcmp(entry->d_name, "ttyGNSS") == 0) {
			find_dev_path(sysfs_path, entry, "dev/ttyTEST");
			log_debug("ttyGPS detected: %s", "dev/ttyTEST");
		} else if (strcmp(entry->d_name, "ttyMAC") == 0) {
			find_dev_path(sysfs_path, entry, devices_path->mac_path);
			log_debug("ttyMAC detected: %s", devices_path->mac_path);
		} else if (strcmp(entry->d_name, "disciplining_config") == 0) {
			find_file((char *) sysfs_path, "disciplining_config", devices_path->disciplining_config_path);
			log_debug("disciplining_config detected: %s", devices_path->disciplining_config_path);
		} else if (strcmp(entry->d_name, "temperature_table") == 0) {
			find_file((char *) sysfs_path, "temperature_table", devices_path->temperature_table_path);
			log_debug("temperature_table detected: %s", devices_path->temperature_table_path);
		}

		entry = readdir(ocp_dir);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct gps_device_t session = {};
	volatile struct pps_thread_t * pps_thread = NULL;
	__attribute__((cleanup(fd_cleanup))) int fd_clock = -1;
	struct gnss *gnss;
	struct config config;
	const char *path;
	int ret;

    bool survey = false;
    bool valid = false;
    int32_t qErr = -1;

	log_set_level(2);

	if (argc != 2)
		error(EXIT_FAILURE, 0, "usage: %s config_file_path", argv[0]);
	path = argv[1];

	/* Read Config file */
	ret = config_init(&config, path);
	if (ret != 0) {
		error(EXIT_FAILURE, -ret, "config_init(%s)", path);
		return -EINVAL;
	}

	/* Get devices' path from sysfs directory */
	ret = get_devices_path_from_sysfs(&config, &devices_path);
	if (ret != 0) {
		error(EXIT_FAILURE, -ret, "get_devices_path_from_sysfs");
		return -EINVAL;
	}

	/* Open PTP clock file descriptor */
	fd_clock = open(devices_path.ptp_path, O_RDWR);
	if (fd_clock == -1) {
		log_error("Could not open ptp clock device while disciplining_mode is activated !");
		error(EXIT_FAILURE, errno, "open(%s)", devices_path.ptp_path);
		return -EINVAL;
	}

	/* Init GPS session and context */
	session.context = &context;
	(void)memset(&context, '\0', sizeof(struct gps_context_t));
	context.leap_notify = LEAP_NOWARNING;
	session.sourcetype = source_pps;
	pps_thread = &(session.pps_thread);
	pps_thread->context = &session;

	/* Start GNSS Thread */
	gnss = gnss_init(&config, devices_path.gnss_path, &session, fd_clock);
	if (gnss == NULL) {
		error(EXIT_FAILURE, errno, "Failed to listen to the receiver");
		return -EINVAL;
	}

	time_t start;
	double cpu_time_used;
	while(loop)
    {	
		usleep(1000000);
	}
}