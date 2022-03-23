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
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "gnss_serial_test.h"
#include "log.h"
#include "mro_device_test.h"
#include "phase_error_tracking_test.h"
#include "ptp_device_test.h"
#include "utils.h"

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

/* Find file by name recursively in a directory*/
static bool find_file(char * path , char * name)
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
            snprintf(subpath, sizeof(subpath), "%s/%s", path, dp->d_name);
            found = find_file(subpath, name);
            if (found)
                break;
        } else if (!strcmp(dp->d_name, name)) {
            char file_path[1024];
            snprintf(file_path, sizeof(file_path), "%s/%s", path, dp->d_name);
            log_info("\t- file %s is in %s", name, file_path);
            found = true;
            break;
        }
    }
    closedir(directory);
    return found;
}

static bool test_ocp_directory(char * ocp_path) {
    DIR * ocp_dir = opendir(ocp_path);
    bool gnss_receiver_passed = false;
    uint32_t mro50_coarse_value;
    bool mro50_passed = false;
    bool found_eeprom = false;
    bool ptp_passed = false;

    if (ocp_dir != NULL) {
        log_info("Directory %s exists\n", ocp_path);
    } else {
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
            found_eeprom = find_file(realpath(pathname, NULL), "eeprom");
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
            char dev_path[1024];
            find_dev_path(ocp_path, entry, dev_path);
            printf("dev_path is %s\n", dev_path);
            int mro50 = open(dev_path, O_RDWR);
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
            char dev_path[1024];   /* should alwys be big enough */
            find_dev_path(ocp_path, entry, dev_path);
            int ptp_clock = open(dev_path, O_RDWR);
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
            char dev_path[1024];   /* should alwys be big enough */
            find_dev_path(ocp_path, entry, dev_path);
            gnss_receiver_passed = test_gnss_serial(dev_path);
        }

        entry = readdir(ocp_dir);
    }

    if (!(mro50_passed && ptp_passed && gnss_receiver_passed && found_eeprom)) {
        log_error("At least one test failed");
        return false;
    } else {
        log_info("All tests passed");
        // TODO: Add format of eeprom
    }
    return true;
}

int main(int argc, char *argv[])
{
    char *serial_number = NULL;
    char *sysfs_path = NULL;
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
    if(test_ocp_directory(sysfs_path)) {
        test_phase_error_tracking();
    } else {
        log_error("ART Card in %s did not pass all tests", sysfs_path);
        return -1;
    }
    return 0;
}
