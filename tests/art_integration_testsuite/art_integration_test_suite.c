/**
 * @file art_integration_test_suite.c
 * @brief Integration test suite program for art card.
 * @version 0.1
 * @date 2022-01-10
 *
 * @copyright Copyright (c) 2022
 * Check wether art card handled by ptp_ocp driver works 
 * by interacting will all its devices
 */
#include <dirent.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "configurable_io_test.h"
#include "gnss_serial_test.h"
#include "log.h"
#include "mro_device_test.h"
#include "phase_error_tracking_test.h"
#include "ptp_device_test.h"
#include "utils.h"

#define READ 0
#define WRITE 1

#define SOCKET_PORT 2970

struct devices_path {
    char eeprom_path[1024];
    char mro_path[256];
    char ptp_path[256];
    char gnss_path[256];
};

static void print_help(void)
{
    printf("usage: art_integration_test_suite [-h] -p PATH -s SERIAL_NUMBER\n");
    printf("Parameters:\n");
    printf("- -p PATH: path of the file/EEPROM data should be written from\n");
    printf("- -s SERIAL_NUMBER: Serial number that should be written within data." \
        "Serial must start with an F followed by 8 numerical caracters\n");
    printf("- -h: prints help\n");
    return;
}

/* find device path in /dev from symlink in sysfs */
static void find_dev_path(const char *dirname, struct dirent *dir, char *dev_path)
{
    char dev_repository[1024];   /* should alwys be big enough */
    sprintf(dev_repository, "%s/%s", dirname, dir->d_name );
    char dev_name[100];
    char * token = strtok(realpath(dev_repository, NULL), "/");
    while(token != NULL) {
        strncpy(dev_name, token, sizeof(dev_name));
        token = strtok(NULL, "/");
    }
    sprintf(dev_path, "%s/%s", "/dev", dev_name );
}

/* Find file by name recursively in a directory */
static bool find_file(char * path , char * name, char * file_path)
{
    DIR * directory;
    struct dirent * dp;
    bool found = false;
    if(!(directory = opendir(path)))
        return false;

    while ((dp = readdir(directory)) != NULL) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
            continue;

        if (dp->d_type == DT_DIR) {
            char subpath[1024];
            sprintf(subpath, "%s/%s", path, dp->d_name);
            found = find_file(subpath, name, file_path);
            if (found)
                break;
        } else if (!strcmp(dp->d_name, name)) {
            log_info("\t- file %s is in %s/%s", name, path, dp->d_name);
            if (file_path != NULL)
                sprintf(file_path, "%s/%s", path, dp->d_name);
            found = true;
            break;
        }
    }
    closedir(directory);
    return found;
}

static bool test_ocp_directory(char * ocp_path, char * serial_number, struct devices_path *devices_path) {
    DIR * ocp_dir = opendir(ocp_path);
    bool gnss_receiver_passed = false;
    uint32_t mro50_coarse_value;
    bool config_io_passed = false;
    bool mro50_passed = false;
    bool found_eeprom = false;
    bool ptp_passed = false;

    if (ocp_dir == NULL) {
        log_error("Directory %s does not exists\n", ocp_path);
        return false;
    }

    struct dirent * entry = readdir(ocp_dir);
    while (entry != NULL) {
        /* I2C TEST: Find EEPROM File
         * EEPROM file will be written if test is successful
         */
        if (strncmp(entry->d_name, "i2c", 4) == 0) {
            log_info("I2C device detected");
            char pathname[1280];   /* should alwys be big enough */
            sprintf( pathname, "%s/%s", ocp_path, entry->d_name);
            found_eeprom = find_file(realpath(pathname, NULL), "eeprom", devices_path->eeprom_path);
            if (found_eeprom) {
                log_info("\t- Found EEPROM file\n");
            } else {
                log_warn("\t- Could not find EEPROM file\n");
            }

        /* MRO50 TEST: Perform R/W operations using ioctls
         * Also read factory coarse which needs to be written in EEPROM
         */
        } else if (strncmp(entry->d_name, "mro50", 6) == 0) {
            log_info("mro50 device detected");
            find_dev_path(ocp_path, entry, devices_path->mro_path);
            int mro50 = open(devices_path->mro_path, O_RDWR);
            if (mro50 > 0) {
                mro50_passed = test_mro50_device(mro50);
                if (mro50_passed) {
                    /* Read factory coarse of the mRO50 which needs to be stored in EEPROM */
                    if(mro50_read_coarse(mro50, &mro50_coarse_value) != 0) {
                        log_error("Could not read factory coarse value of mRO50");
                        mro50_passed = false;
                    }
                }
                close(mro50);
            } else {
                log_error("\t- Error opening mro50 device");
            }

        /* PTP CLOCK TEST: Set clock time */
        } else if (strncmp(entry->d_name, "ptp", 4) == 0) {
            log_info("ptp clock device detected");
            find_dev_path(ocp_path, entry, devices_path->ptp_path);
            int ptp_clock = open(devices_path->ptp_path, O_RDWR);
            if (ptp_clock > 0) {
                ptp_passed = test_ptp_device(ptp_clock);
                close(ptp_passed);
            } else {
                log_error("\t- Error opening ptp device");
            }

        /* SERIAL GNSS TEST:
         * Check serial can be opened
         * Reconfigure GNSS to default configuration
         * check it can receive a fix and UBX-MON-RF message
         */
        } else if (strncmp(entry->d_name, "ttyGNSS", 7) == 0) {
            log_info("ttyGPS detected");
            find_dev_path(ocp_path, entry, devices_path->gnss_path);
            gnss_receiver_passed = test_gnss_serial(devices_path->gnss_path);
        }

        entry = readdir(ocp_dir);
    }

    /* Test IO */
    config_io_passed = test_configurable_io(ocp_path);

    if (!(mro50_passed && ptp_passed && gnss_receiver_passed && found_eeprom && config_io_passed)) {
        log_error("At least one test failed");
        return false;
    } else {
        log_info("All tests passed\n");
        log_info("Writing EEPROM manufacturing data and factory disciplining parameters");
        char command[2048];
        sprintf(command, "art_eeprom_format -p %s -s %s -c %d", devices_path->eeprom_path, serial_number, mro50_coarse_value);
        if (system(command) != 0) {
            log_error("Could not write EEPROM data");
            return false;
        }
    }
    return true;
}

static void prepare_config_file_for_oscillatord(struct devices_path *devices_path, char * ocp_name, int socket_port_offset,struct config *config)
{
    char socket_port_number[10];
    /* Define socket port offset */
    sprintf(socket_port_number, "%d", SOCKET_PORT + socket_port_offset);

    memset(config, 0, sizeof(struct config));

    config_set(config, "disciplining", "true");
    config_set(config, "monitoring", "true");
    config_set(config, "socket-address", "0.0.0.0");
    config_set(config, "socket-port", socket_port_number);
    config_set(config, "oscillator", "mRO50");
    config_set(config, "ptp-clock", devices_path->ptp_path);
    config_set(config, "mro50-device", devices_path->mro_path);
    config_set(config, "gnss-device-tty", devices_path->gnss_path);
    config_set(config, "gnss-receiver-reconfigure", "false");
    config_set(config, "opposite-phase-error", "false");
    config_set(config, "debug", "1");
    config_set(config, "calibrate_first", "false");
    config_set(config, "phase_resolution_ns", "5");
    config_set(config, "ref_fluctuations_ns", "30");
    config_set(config, "phase_jump_threshold_ns", "300");
    config_set(config, "reactivity_min", "10");
    config_set(config, "reactivity_max", "30");
    config_set(config, "reactivity_power", "2");
    config_set(config, "fine_stop_tolerance", "200");
    config_set(config, "max_allowed_coarse", "30");
    config_set(config, "nb_calibration", "10");
    config_set(config, "oscillator_factory_settings", "true");
    return;
}

int main(int argc, char *argv[])
{
    struct devices_path devices_path;
    struct config config;
    uint32_t mro50_coarse_value;
    char *serial_number = NULL;
    char *sysfs_path = NULL;
    char ocp_name[100];
    char temp[1024];
    int ocp_number;
    int ret;
    int c;

    log_set_level(LOG_DEBUG);
    log_info("ART Program Test Suite");

    while ((c = getopt(argc, argv, "p:s:h")) != -1) {
        switch (c) {
        case 'p':
            sysfs_path = optarg;
            break;
        case 's':
            serial_number = optarg;
            break;
        case 'h':
            print_help();
            return 0;
            break;
        case '?':
            if (optopt == 'p')
                fprintf (stderr, "Option -%c requires path to ART card sysfs.\n", optopt);
            return -1;
            break;
        }
    }

    if (!sysfs_path) {
        printf("Please provide path to ART card sysfs\n");
        return -1;
    }

    log_info("Testing ART card which sysfs is %s", sysfs_path);
    if(test_ocp_directory(sysfs_path, serial_number, &devices_path)) {
        /* Extract ocpX from sysfs */
        sprintf(temp, "%s", sysfs_path);
        char * token = strtok(realpath(temp, NULL), "/");
        while(token != NULL) {
            strncpy(ocp_name, token, sizeof(ocp_name));
            token = strtok(NULL, "/");
        }
        log_debug("OCP name is %s", ocp_name);
        if (1 == sscanf(ocp_name, "%*[^0123456789]%d", &ocp_number)) {
            log_debug("ocp number is %d", ocp_number);
            /* Prepare config file to be used by oscillatord for tests */
            prepare_config_file_for_oscillatord(&devices_path, ocp_name, ocp_number, &config);

            /* Test card by checking phase error stays in limits defined in phase error tracking test */
            switch(test_phase_error_tracking(ocp_name, &config)) {
            case TEST_PHASE_ERROR_TRACKING_OK:
                /* Test passed without calibration, card is ready */
                break;
            case TEST_PHASE_ERROR_TRACKING_OK_WITH_CALIBRATION:
                /* Test passed but calibration has been needed
                 * We need to update factory coarse
                 */
                log_info("Test passed but factory coarse needs to be updated");
                int mro50 = open(devices_path.mro_path, O_RDWR);
                if (mro50 > 0) {
                    /* Read factory coarse of the mRO50 which needs to be stored in EEPROM */
                    ret = mro50_read_coarse(mro50, &mro50_coarse_value);
                    close(mro50);
                    if(ret != 0) {
                        log_error("Could not read factory coarse value of mRO50");
                        return -1;
                    }
                    log_info("Updating factory coarse in EEPROM due to calibration of the card");
                    char command[2048];
                    sprintf(command, "art_eeprom_format -p %s -s %s -c %d", devices_path.eeprom_path, serial_number, mro50_coarse_value);
                    if (system(command) != 0) {
                        log_error("Could not write EEPROM data");
                        return -1;
                    }
                } else {
                    log_error("\t- Error opening mro50 device");
                    return -1;
                }

                break;
            case TEST_PHASE_ERROR_TRACKING_KO:
                /* Test did not pass, card is must not be shipped */
                return -1;
                break;
            default:
                log_error("This test result is not supported !");
                return -1;
            }

        }

    } else {
        log_error("ART Card in %s did not pass all tests", sysfs_path);
        return -1;
    }

    return 0;
}
