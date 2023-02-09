/**
 * @file oscillatord.c
 * @brief Main file of the program
 * @date 2022-01-10
 *
 * @copyright Copyright (c) 2022
 *
 * Oscillatord aims at disciplining an oscillator to an external reference.
 * It is responsible for fetching oscillator and reference data and pass them
 * to a disciplining algorithm, and apply the decision of the algorithm regarding the oscillator.
 */
#include <sys/select.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <math.h>

#include <error.h>


#include <linux/ptp_clock.h>

#include "log.h"
#include "oscillator.h"
#include "oscillator_factory.h"
#include "utils.h"

#define UPDATE_DISCIPLINING_PARAMETERS_SEC 3600

struct oscillator *oscillator = NULL;

static int get_devices_path_from_sysfs(const char* sysfs_path, struct devices_path *devices_path) 
{

	DIR * ocp_dir;

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
			find_dev_path(sysfs_path, entry, devices_path->gnss_path);
			log_debug("ttyGPS detected: %s", devices_path->gnss_path);
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
	struct config config;
    struct devices_path devices_path = { 0 };
	struct oscillator_ctrl ctrl_values;
	const char *path;
	struct oscillator_attributes osc_attr = { 0 };
    int fd_clock;
    int log_level;

    int ret;

    char file_path[256] = "";
    char ocp_path[256] = "";

    snprintf(file_path, sizeof(file_path), "/etc/oscillatord_%s.conf", argv[1]);
    snprintf(ocp_path, sizeof(ocp_path), "/sys/class/timecard/%s", argv[1]);
printf("%s %s\n",file_path, ocp_path);

	/* Read Config file */

	ret = config_init(&config, file_path);
	if (ret != 0) {
		error(EXIT_FAILURE, -ret, "config_init(%s)", path);
		return -EINVAL;
	}

    get_devices_path_from_sysfs(ocp_path, &devices_path);

	/* Set log level */
	log_set_level(0);
	log_info("Starting Oscillatord MRO TEST");

	/* Create oscillator object */
	oscillator = oscillator_factory_new(&config, &devices_path);
	if (oscillator == NULL) {
		error(EXIT_FAILURE, errno, "oscillator_factory_new");
		return -EINVAL;
	}
	log_info("oscillator model %s", oscillator->class->name);


    ret = oscillator_parse_attributes(oscillator, &osc_attr);
    if (ret == -ENOSYS)
    {
        osc_attr.temperature = 0.0;
        osc_attr.locked = false;
    } else if (ret < 0)
    {
        log_warn("Coud not get temperature of oscillator");
    }

    ret = oscillator_get_ctrl(oscillator, &ctrl_values);
    if (ret != 0) {
        log_warn("Could not get control values of oscillator");
    }

    // ret = oscillator_apply_output(oscillator, &output);


    ret = oscillator_parse_attributes(oscillator, &osc_attr);
    if (ret == -ENOSYS)
    {
        osc_attr.temperature = 0.0;
        osc_attr.locked = false;
    }
    ret = oscillator_get_ctrl(oscillator, &ctrl_values);
    if (ret != 0)
    {
        log_warn("Could not get control values of oscillator");

	}

	if (oscillator != NULL)
    {
		oscillator_factory_destroy(&oscillator);
	}

	return EXIT_SUCCESS;
}
