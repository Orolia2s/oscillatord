#include <dirent.h>
#include <getopt.h>
#include <linux/limits.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "log.h"
#include "utils.h"

int main(int argc, char *argv[])
{
	char ocp_path[256] = {0};
    bool ocp_path_valid;
    bool found_eeprom = false;

	/* Set log level */
	log_set_level(1);

    snprintf(ocp_path, sizeof(ocp_path) - 1, "%s", argv[1]);

	log_info("\t-ocp path is: \"%s\", checking...",ocp_path);
	if (access(ocp_path, F_OK) != -1)
	{
		ocp_path_valid = true;
        log_info("\t\tocp dir path exists !");
    }
	else
	{
		ocp_path_valid = false;
        log_info("\t\tocp path doesn't exists !");
    }

    DIR * ocp_dir = opendir(ocp_path);

    if (ocp_dir == NULL)
    {
        log_error("Directory %s does not exists", ocp_path);
    }
    else
    {
        char eeprom_path[256];
        char mro_path[256];
        char ptp_path[256];
        char pps_path[256];
        char gnss_path[256];
        char mac_path[256];
        char disciplining_config_path[256];
        char temperature_table_path[256];

        DIR * ocp_dir;

        if (ocp_path == NULL)
        {
            log_error("No sysfs-path provided in oscillatord config file !");
        }
        else if (ocp_path_valid)
        {
            log_info("\t-sysfs path %s", ocp_path);

            ocp_dir = opendir(ocp_path);
            struct dirent * entry = readdir(ocp_dir);
            while (entry != NULL)
            {
                if (strncmp(entry->d_name, "i2c", 4) == 0)
                {
                    log_info("I2C device detected");
                    char pathname[PATH_MAX];   /* should always be big enough */
                    sprintf( pathname, "%s/%s", ocp_path, entry->d_name);
                    found_eeprom = find_file(realpath(pathname, NULL), "eeprom", eeprom_path);
                    if (found_eeprom)
                    {
                        log_info("\t- Found EEPROM file: %s", eeprom_path);
                    }
                    else
                    {
                        log_warn("\t- Could not find EEPROM file");
                    }
                }
                if (strcmp(entry->d_name, "mro50") == 0)
                {
                    find_dev_path(ocp_path, entry, mro_path);
                    log_info("\t-mro50 device detected: %s", mro_path);
                } else if (strcmp(entry->d_name, "ptp") == 0)
                {
                    find_dev_path(ocp_path, entry, ptp_path);
                    log_info("\t-ptp clock device detected: %s", ptp_path);
                } else if (strcmp(entry->d_name, "pps") == 0)
                {
                    find_dev_path(ocp_path, entry, pps_path);
                    log_info("\t-pps device detected: %s", pps_path);
                } else if (strcmp(entry->d_name, "ttyGNSS") == 0)
                {
                    find_dev_path(ocp_path, entry, gnss_path);
                    log_info("\t-ttyGPS detected: %s", gnss_path);
                }
                else if (strcmp(entry->d_name, "ttyMAC") == 0)
                {
                    find_dev_path(ocp_path, entry, mac_path);
                    log_info("\t-ttyMAC detected: %s", mac_path);
                }
                else if (strcmp(entry->d_name, "disciplining_config") == 0)
                {
                    find_file((char *) ocp_path, "disciplining_config", disciplining_config_path);
                    log_info("\t-disciplining_config detected: %s", disciplining_config_path);
                }
                else if (strcmp(entry->d_name, "temperature_table") == 0)
                {
                    find_file((char *) ocp_path, "temperature_table", temperature_table_path);
                    log_info("\t-temperature_table detected: %s", temperature_table_path);
                }

                entry = readdir(ocp_dir);
            }
        }
        else
        {
            log_info("ocp dir Test Aborted");
        }
    }
}
