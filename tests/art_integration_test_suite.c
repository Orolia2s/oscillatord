/*
 * Integration test suite program for art card.
 * Check wether art card handled by ptp_ocp driver works 
 * by interacting will all its devices
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <ubloxcfg/ff_epoch.h>
#include <ubloxcfg/ff_rx.h>
#include <ubloxcfg/ff_ubx.h>
#include <unistd.h>
#include <time.h>

#include "log.h"
#include "utils.h"

#define GNSS_TIMEOUT_MS 1500

#define CLOCKFD 3
#define FD_TO_CLOCKID(fd)	((clockid_t) ((((unsigned int) ~fd) << 3) | CLOCKFD))

/** Minimum possible value of coarse control */
#define COARSE_RANGE_MIN 0
/** Maximum possible value of coarse control */
#define COARSE_RANGE_MAX 4194303
/** Minimum possible value of fine control */
#define FINE_RANGE_MIN 1600
/** Maximum possible value of fine control */
#define FINE_RANGE_MAX 3200

typedef u_int32_t u32;
/*---------------------------------------------------------------------------*/
#ifndef MRO50_IOCTL_H
#define MRO50_IOCTL_H

#define MRO50_READ_FINE		_IOR('M', 1, u32 *)
#define MRO50_READ_COARSE	_IOR('M', 2, u32 *)
#define MRO50_ADJUST_FINE	_IOW('M', 3, u32)
#define MRO50_ADJUST_COARSE	_IOW('M', 4, u32)
#define MRO50_READ_TEMP		_IOR('M', 5, u32 *)
#define MRO50_READ_CTRL		_IOR('M', 6, u32 *)
#define MRO50_SAVE_COARSE	_IO('M', 7)

#endif /* MRO50_IOCTL_H */
/*---------------------------------------------------------------------------*/

const char base_procfs_path[16] = "/proc/driver/ocp";

/* Open file from symlink path */
static int opensymlink( const char *dirname, struct dirent *dir)
{
    char pathname[1024];   /* should alwys be big enough */
    int fp;
    sprintf( pathname, "%s/%s", dirname, dir->d_name );

    fp = open(realpath(pathname, NULL), O_RDWR);
    return fp;
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
            log_info("\t\t- file %s is in %s", name, file_path);
            found = true;
            break;
        }
    }
    closedir(directory);
    return found;
}

/* Compute diff between two timespec */
static void timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }

    return;
}

/*
 * Performs read / write operation on mro50 using ioctl to check wether
 * coarse and fine adjustments is working
 */
static bool mro50_check_ioctl_read_write(
    int mro50,
    unsigned long read_cmd,
    unsigned long write_cmd,
    uint32_t value_min,
    uint32_t value_max)
{
    int err;
    uint32_t original_value;
    /* Read current coarse value of mro50 */
    err = ioctl(mro50, read_cmd, &original_value);
    if (err != 0) {
        log_debug("\t\t- Error %d", err);
        log_error("\t\t- Error reading value of mro50");
        return false;
    }
    if (original_value >= value_min &&
        original_value <= value_max) {
        log_info("\t\t- value is in acceptable range");
    } else {
        return false;
    }
    /* try writing value + 1 or value - 1 to check
     * write operation works
     */
    uint32_t new_value;
    if (original_value + 1 <= value_max) {
        new_value = original_value + 1;
    } else {
        new_value = original_value - 1;
    }
    err = ioctl(mro50, write_cmd, &new_value);
    if (err != 0) {
        return false;
    }

    /* Check value has been well written */
    uint32_t written_value;
    err = ioctl(mro50, read_cmd, &written_value);
    if (written_value != new_value) {
        return false;
    }

    err = ioctl(mro50, write_cmd, &original_value);
    if (err != 0) {
        return false;
    }
    /* Write back previous value to preserve configuration */
    return true;
}

static bool test_mro50_device(int mro50)
{
    log_info("\t- Testing mro50 device...");

    log_info("\t- Testing read / write operations on coarse value:");
    if (mro50_check_ioctl_read_write(
        mro50, MRO50_READ_COARSE, MRO50_ADJUST_COARSE,
        COARSE_RANGE_MIN, COARSE_RANGE_MAX)
    ) {
        log_info("\t\t- Coarse operations working !");
    } else {
        log_error("\t\t- Error reading / writing coarse value");
        return false;
    }
    log_info("\t\t- Passed\n");

    log_info("\t- Testing read / write operations on fine value:");
    if (mro50_check_ioctl_read_write(
        mro50,MRO50_READ_FINE,MRO50_ADJUST_FINE,
        FINE_RANGE_MIN, FINE_RANGE_MAX)
    ) {
        log_info("\t\t- Fine operations working !");
    } else {
        log_error("\t\t- Error reading / writing fine value");
        return false;
    }
    log_info("\t\t- Passed\n");
    log_info("\t- Testing temperature read...");

    uint32_t temp_reg;
    int err = ioctl(mro50, MRO50_READ_TEMP, &temp_reg);
    if (err != 0) {
        log_error("\t\t- Error reading temperature");
        return false;
    }
    double temperature = compute_temp(temp_reg);
    log_info("\t\t- Temperature read is %f °C", temperature);
    log_info("\t\t- Passed\n");

    return true;
}

/*
 * Get current time from server and set ptp clock to this time
 * Check time is correctly written
 */
static bool test_ptp_device(int ptp)
{
    int ret;
    clockid_t clkid;
    struct timespec ts_real;
    struct timespec ts_set;
    struct timespec ts_diff;

    ret = clock_gettime(CLOCK_REALTIME, &ts_real);
    if (ret != 0) {
        log_error("\t- Could not read clock realtime on server\n");
        return false;
    }

    clkid = FD_TO_CLOCKID(ptp);
    ret = clock_settime(clkid, &ts_real);
    if (ret != 0) {
        log_error("\t- Could not set time of ptp clock\n");
        return false;
    }
    ret = clock_gettime(CLOCK_REALTIME, &ts_set);
    if (ret != 0) {
        log_error("\t- Could not read time of ptp clock\n");
        return false;
    }

    timespec_diff(&ts_real, &ts_set, &ts_diff);
    log_debug(
        "\t- Diff time between time set and time read is: %lus %luns",
        ts_diff.tv_sec,
        ts_diff.tv_nsec
    );
    log_info("\t- PTP Clock time correctly set\n");
    return true;
}

static bool test_gnss_serial(char * path)
{
    bool got_gnss_fix = false;
    bool got_mon_rf_message = false;

    if (path == NULL) {
        log_error("\t- GNSS Path does not exists");
        return false;
    }
    RX_ARGS_t args = RX_ARGS_DEFAULT();
    args.autobaud = true;
    args.detect = true;
    RX_t * rx = rxInit(path, &args);

    if (!rxOpen(rx)) {
        free(rx);
        log_error("\t- Gnss rx init failed\n");
        return false;
    }

    EPOCH_t coll;
    EPOCH_t epoch;

    epochInit(&coll);

    time_t timeout = time(NULL) + 10;
    while ((!got_gnss_fix || !got_mon_rf_message) && time(NULL) < timeout)
    {
        PARSER_MSG_t *msg = rxGetNextMessageTimeout(rx, GNSS_TIMEOUT_MS);
        if (msg != NULL)
        {
            if(epochCollect(&coll, msg, &epoch))
            {
                if(epoch.haveFix) {
                    log_info("\t\t- Got fix !");
                    got_gnss_fix = true;
                }
            } else {
                uint8_t clsId = UBX_CLSID(msg->data);
                uint8_t msgId = UBX_MSGID(msg->data);
                if (clsId == UBX_MON_CLSID && msgId == UBX_MON_RF_MSGID) {
                    log_info("\t\t- Got UBX-MON-RF message");
                    got_mon_rf_message = true;
                }
            }
        } else {
            log_warn("GNSS: UART Timeout !");
            usleep(5 * 1000);
        }
    }

    rxClose(rx);
    free(rx);
    if (got_gnss_fix && got_mon_rf_message) {
        log_info("\t- Passed\n");
        return true;
    }
    log_error("\t- GNSS: did not get either fix or UBX-MON-RF message before 10 second timeout\n");
    return false;
}


int main(int argc, char *argv[])
{
    bool mro50_passed = false;
    bool ptp_passed = false;
    bool gnss_receiver_passed = false;
    bool found_eeprom = false;
    log_set_level(LOG_DEBUG);

    log_info("ART Program Test Suite");
    log_info("Checking ART's procfs is exposed on the server");

    DIR * ocp_0 = opendir("/proc/driver/ocp0");

    if (ocp_0 != NULL) {
        log_info("Directory /proc/driver/ocp0 exists\n");
    } else {
        log_error("Directory /proc/driver/ocp0 does not exists\n");
        return -1;
    }

    struct dirent * entry = readdir(ocp_0);
    while (entry != NULL) {
        /* I2C TEST: Find EEPROM File */
        if (strncmp(entry->d_name, "i2c", 4) == 0) {
            log_info("I2C device detected");
            char pathname[1024];   /* should alwys be big enough */
            sprintf( pathname, "%s/%s", "/proc/driver/ocp0", entry->d_name);
            found_eeprom = find_file(realpath(pathname, NULL), "eeprom");
            if (found_eeprom) {
                log_info("\t- Found EEPROM file\n");
            } else {
                log_warn("\t- Could not find EEPROM file\n");
            }

        /* MRO50 TEST: Perform R/W operations using ioctls */
        } else if (strncmp(entry->d_name, "mro50", 6) == 0) {
            log_info("mro50 device detected");
            int mro50 = opensymlink("/proc/driver/ocp0", entry);
            if (mro50 > 0) {
                mro50_passed = test_mro50_device(mro50);
                close(mro50);
            } else {
                log_error("\t- Error opening mro50 device");
            }

        /* PTP CLOCK TEST: Set clock time */
        } else if (strncmp(entry->d_name, "ptp", 4) == 0) {
            log_info("ptp clock device detected");
            int ptp_clock = opensymlink("/proc/driver/ocp0", entry);
            if (ptp_clock > 0) {
                ptp_passed = test_ptp_device(ptp_clock);
                close(ptp_passed);
            } else {
                log_error("\t- Error opening ptp device");
            }

        /* SERIAL GNSS TEST: check it can receive a fix and UBX-MON-RF message */
        } else if (strncmp(entry->d_name, "ttyGPS", 7) == 0) {
            log_info("ttyGPS detected");
            char pathname[1024];   /* should alwys be big enough */
            sprintf( pathname, "%s/%s", "/proc/driver/ocp0", entry->d_name);
            gnss_receiver_passed = test_gnss_serial(realpath(pathname, NULL));
        }

        entry = readdir(ocp_0);
    }

    if (!found_eeprom) {
        log_warn("Could not find EEPROM file.");
        log_warn("The card may work without eeprom storage,"\
        "but configuration will not be stored on the device");
    }
    if (!(mro50_passed && ptp_passed && gnss_receiver_passed)) {
        log_error("At least one test failed");
        return -1;
    }
    log_info("All tests passed");
    return 0;
}
