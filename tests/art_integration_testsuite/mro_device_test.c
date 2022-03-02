#include <errno.h>
#include <sys/ioctl.h>

#include "log.h"
#include "mro_device_test.h"
#include "mRO50_ioctl.h"
#include "utils.h"

/** Minimum possible value of coarse control */
#define COARSE_RANGE_MIN 0
/** Maximum possible value of coarse control */
#define COARSE_RANGE_MAX 4194303
/** Minimum possible value of fine control */
#define FINE_RANGE_MIN 1600
/** Maximum possible value of fine control */
#define FINE_RANGE_MAX 3200

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
    /* Read current coarse/fine value of mro50 */
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
    if (read_cmd == MRO50_READ_COARSE) {
        log_info("Coarse value read on mRO50 is %u", original_value);
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

    /* Write back previous value to preserve configuration */
    err = ioctl(mro50, write_cmd, &original_value);
    if (err != 0) {
        return false;
    }
    return true;
}

int mro50_read_coarse(int mro50, uint32_t *coarse)
{
    if (coarse == NULL)
        return -EFAULT;
    return ioctl(mro50, MRO50_READ_COARSE, coarse);
}

bool test_mro50_device(int mro50)
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
