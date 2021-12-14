/*
 * Integration test suite program for art card.
 * Check wether art card handled by ptp_ocp driver works 
 * by interacting will all its devices
 */
#include <dirent.h>
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

/* Open file from symlink path */
static int opensymlink( const char *dirname, struct dirent *dir)
{
    char pathname[1024];   /* should alwys be big enough */
    int fp;
    sprintf( pathname, "%s/%s", dirname, dir->d_name );
    log_info("Realpath is %s", realpath(pathname, NULL));
    fp = open(realpath(pathname, NULL), O_RDWR);
    return fp;
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
    bool found;
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

static bool test_ocp_directory(char * ocp_path, char * dir_name) {
    bool mro50_passed = false;
    bool ptp_passed = false;
    bool gnss_receiver_passed = false;
    bool found_eeprom = false;
    DIR * ocp_dir = opendir(ocp_path);

    if (ocp_dir != NULL) {
        log_info("Directory %s exists\n", ocp_path);
    } else {
        log_error("Directory %s does not exists\n", ocp_path);
        return false;
    }

    struct dirent * entry = readdir(ocp_dir);
    while (entry != NULL) {
        /* I2C TEST: Find EEPROM File */
        if (strncmp(entry->d_name, "i2c", 4) == 0) {
            log_info("I2C device detected");
            char pathname[1024];   /* should alwys be big enough */
            sprintf( pathname, "%s/%s", ocp_path, entry->d_name);
            found_eeprom = find_file(realpath(pathname, NULL), "eeprom");
            if (found_eeprom) {
                log_info("\t- Found EEPROM file\n");
            } else {
                log_warn("\t- Could not find EEPROM file\n");
            }

        /* MRO50 TEST: Perform R/W operations using ioctls */
        } else if (strncmp(entry->d_name, "mro50", 6) == 0) {
            log_info("mro50 device detected");
            int mro50 = opensymlink(ocp_path, entry);
            if (mro50 > 0) {
                mro50_passed = test_mro50_device(mro50);
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

        /* SERIAL GNSS TEST: check it can receive a fix and UBX-MON-RF message */
        } else if (strncmp(entry->d_name, "ttyGNSS", 7) == 0) {
            log_info("ttyGPS detected");
            char dev_path[1024];   /* should alwys be big enough */
            find_dev_path(ocp_path, entry, dev_path);
            gnss_receiver_passed = test_gnss_serial(dev_path);
        }

        entry = readdir(ocp_dir);
    }

    // Small hack to test mRO50 while not being in sysfs
    char mRO_pathname[1024] = "/dev/mro50.";
    mRO_pathname[11] = dir_name[3];
    mRO_pathname[12] = '\0';
    int mro50= open(mRO_pathname, O_RDWR);
    if (mro50 > 0) {
        mro50_passed = test_mro50_device(mro50);
        close(mro50);
    } else {
        log_error("\t- Error opening mro50 device");
    }
    if (!found_eeprom) {
        log_warn("Could not find EEPROM file.");
        log_warn("The card may work without eeprom storage,"\
        "but configuration will not be stored on the device");
    }

    if (!(mro50_passed && ptp_passed && gnss_receiver_passed)) {
        log_error("At least one test failed");
        return false;
    }
    log_info("All tests passed");
    return true;
}

int main(int argc, char *argv[])
{
    log_set_level(LOG_DEBUG);

    log_info("ART Program Test Suite");
    log_info("Checking if there are ART's procfs on the server");

    DIR * driver_fs = opendir("/sys/class/timecard");
    if (driver_fs == NULL) {
        log_error("Cannot access /sys/class/timecard");
        return -1;
    }

    struct dirent * entry;
    while ((entry = readdir(driver_fs)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        if (!strncmp(entry->d_name, "ocp", 3)) {
            char ocp_path[1024];
            snprintf(ocp_path, sizeof(ocp_path), "%s/%s", "/sys/class/timecard", entry->d_name);
            log_info("Found directory %s", ocp_path);

            if(test_ocp_directory(ocp_path, entry->d_name)) {
                test_phase_error_tracking();
            }
            else
                log_error("ART Card in %s did not pass all tests", ocp_path);
        }
    }
    closedir(driver_fs);
    return 0;
}
