/**
 * @file art_eeprom_manager.c
 * @brief Read and Write calibration data to ART card's EEPROM
 * @version 0.1
 * @date 2022-01-24
 *
 */
#include <getopt.h>
#include <errno.h>
#include <error.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "log.h"

#define CALIBRATION_POINTS_MAX 10

/**
 * @struct calibration_parameters
 * @brief Calibration parameters as saved in the EEPROM
 */
struct calibration_parameters {
    int8_t ctrl_nodes_length;
    float ctrl_load_nodes[CALIBRATION_POINTS_MAX];
    float ctrl_drift_coeffs[CALIBRATION_POINTS_MAX];
    int32_t coarse_equilibrium;
    int8_t ctrl_nodes_length_factory;
    float ctrl_load_nodes_factory[3];
    float ctrl_drift_coeffs_factory[3];
    int32_t coarse_equilibrium_factory;
    bool calibration_valid;
    int8_t pad_0[4];
};

enum Mode {
    NONE,
    READ,
    WRITE,
    INIT,
};

/**
 * @struct calibration_parameters default_factory
 * @brief Default calibration parameters (no calibration done, factory settings)
 */
static const struct calibration_parameters default_factory = {
    .ctrl_nodes_length = 3,
    .ctrl_load_nodes = {0.25,0.5,0.75},
    .ctrl_drift_coeffs = {0.0, 0.0, 0.0},
    .coarse_equilibrium = -1,
    .ctrl_nodes_length_factory = 3,
    .ctrl_load_nodes_factory = {0.25,0.5,0.75},
    .ctrl_drift_coeffs_factory = {1.2,0.0,-1.2},
    .coarse_equilibrium_factory = -1,
    .calibration_valid = false
};


static void print_calibration(struct calibration_parameters *calibration) {
    log_info("ctrl_nodes_length = %d", calibration->ctrl_nodes_length);
    if (calibration->ctrl_nodes_length >= 0 && calibration->ctrl_nodes_length <= CALIBRATION_POINTS_MAX) {
        for (int i = 0; i < calibration->ctrl_nodes_length; i++) {
            log_info("ctrl_load_nodes[%d] = %f", i, calibration->ctrl_load_nodes[i]);
            log_info("ctrl_drift_coeffs[%d] = %f", i, calibration->ctrl_drift_coeffs[i]);
        }
    }
    log_info("coarse_equilibrium = %d", calibration->coarse_equilibrium);
    log_info("calibration_valid = %s", calibration->calibration_valid ? "true" : "false");

    log_info("ctrl_nodes_length_factory = %d", calibration->ctrl_nodes_length_factory);
    if (calibration->ctrl_nodes_length_factory >= 0 && calibration->ctrl_nodes_length_factory <= CALIBRATION_POINTS_MAX) {
        for (int i = 0; i < calibration->ctrl_nodes_length_factory; i++) {
            log_info("ctrl_load_nodes_factory[%d] = %f", i, calibration->ctrl_load_nodes_factory[i]);
            log_info("ctrl_drift_coeffs_factory[%d] = %f", i, calibration->ctrl_drift_coeffs_factory[i]);
        }
    }
    log_info("coarse_equilibrium_factory = %d", calibration->coarse_equilibrium_factory);
}

static int write_calibration_parameters(const char * path, struct calibration_parameters *calibration)
{
    FILE *fp = fopen(path,"wb");
    if(fp != NULL) {
        fwrite(calibration, 1, sizeof(*calibration), fp);
        fclose(fp);
    } else {
        log_error("Could not open file at %s", path);
        return -1;
    }
    return 0;
}

static void read_calibration_parameters(const char *path)
{
    FILE *fp = fopen(path,"rb");
    if (fp != NULL) {
        struct calibration_parameters calibration;
        int ret = fread(&calibration, sizeof(calibration), 1, fp);
        log_debug("ret is %d, sizeof struct is %ld", ret, sizeof(struct calibration_parameters));
        if (ret != 1) {
            log_error("Error reading calibration parameters from %s", path);
            return;
        }
        print_calibration(&calibration);
        fclose(fp);
    } else {
        log_error("Could not open file at %s", path);
    }
    return;
}

static int double_array_parser(const char* value, double **result) {
    char *endptr;
    char *ptr;
    const char *delim = ",";
    double buffer[CALIBRATION_POINTS_MAX];
    int parsed = 0;
    double value_double;

    errno = 0;

    ptr = strtok((char *) value, delim);
    while (ptr != NULL)
    {
        if (parsed >= CALIBRATION_POINTS_MAX) {
            return -ERANGE;
        }
        value_double = strtold(ptr, &endptr);
        if (value_double == HUGE_VAL ||
            (value_double == 0 && errno == ERANGE))
            return -ERANGE;
        buffer[parsed] = value_double;
        parsed++;
        ptr = strtok(NULL, delim);
    }

    double *values = malloc(parsed * sizeof(double));
    if (values == NULL) {
        return -ENOMEM;
    }
    for (int i = 0; i < parsed; i++) {
        values[i] = buffer[i];
    }

    *result = values;

    return parsed;
}

static double * get_double_array_from_config(struct config *config, const char *key, int expected_length)
{
    int ret;
    const char *value;
    char *value_cpy = NULL;
    double *result = NULL;

    value = config_get(config, key);
    if (value == NULL) {
        log_error("Error reading %s", key);
        return NULL;
    }
    value_cpy = strdup(value);
    ret = double_array_parser(value_cpy, &result);
    free(value_cpy);
    if (ret != expected_length) {
        log_error("Error: Expected length of %d for %s. Got %d", expected_length, key, ret);
        free(result);
        return NULL;
    }

    return result;
}

static int read_input_calibration(const char *path, struct calibration_parameters *result)
{
    uint coarse_equilibrium;
    uint ctrl_nodes_length;
    int ret;

    double *ctrl_drift_coeffs;
    double *ctrl_load_nodes;

    struct config config;

    ret = config_init(&config, path);
    if (ret != 0)
        error(EXIT_FAILURE, -ret, "config_init(%s)", path);

    ctrl_nodes_length = config_get_unsigned_number(&config, "ctrl_nodes_length");
    coarse_equilibrium = config_get_unsigned_number(&config, "coarse_equilibrium");

    ctrl_load_nodes = get_double_array_from_config(&config, "ctrl_load_nodes", ctrl_nodes_length);
    if (ctrl_load_nodes == NULL) {
        log_error("Could not get ctrl_load_nodes from config file at %s", path);
        return -1;
    }

    ctrl_drift_coeffs = get_double_array_from_config(&config, "ctrl_drift_coeffs", ctrl_nodes_length);
    if (ctrl_drift_coeffs == NULL) {
        log_error("Could not get ctrl_drift_coeffs from config file at %s", path);
        free(ctrl_load_nodes);
        return -1;
    }

    memcpy(result, &default_factory, sizeof(struct calibration_parameters));

    result->calibration_valid = true;
    result->coarse_equilibrium = coarse_equilibrium;
    result->ctrl_nodes_length = ctrl_nodes_length;
    for (uint i = 0; i < ctrl_nodes_length; i++) {
        result->ctrl_load_nodes[i] = ctrl_load_nodes[i];
        result->ctrl_drift_coeffs[i] = ctrl_drift_coeffs[i];
    }

    free(ctrl_drift_coeffs);
    free(ctrl_load_nodes);
    config_cleanup(&config);

    return 0;
}

int main(int argc, char *argv[])
{
    int option;
    int ret = 0;
    enum Mode mode = NONE;
    char *path = NULL;
    char *input_calibration_path = NULL;
    log_set_level(LOG_INFO);

    while ((option = getopt(argc, argv, ":p:w:ihr")) != -1) {
        switch (option) {
        case 'r':
            mode = READ;
            break;
        case 'w':
            mode = WRITE;
            input_calibration_path = optarg;
            break;
        case 'i':
            mode = INIT;
            break;
        case 'p':
            path = optarg;
            break;
        case ':':
            log_error("Option needs a value ");
            break;
        case 'h':
        default:
            log_info("art_eeprom_manager -p eeprom_path  [-w calibration.conf -r -i -h]");
            log_info("\t-p eeprom_path: Path to the eeprom file");
            log_info("\t-w calibration.conf: Path to the calibration paramters file to write in the eeprom");
            log_info("\t-r: Read calibration parameters from the eeprom");
            log_info("\t-i: Write default parameters in the eeprom");
            log_info("\t-h: print help");
        }
    }

    if (path == NULL) {
        log_error("No eeprom path provided!");
        return -1;
    }

    log_debug("Path: %s", path);
    log_debug("Mode: %d", mode);

    switch (mode) {
    case READ:
        log_info("Reading data from %s", path);
        read_calibration_parameters(path);
        break;
    case WRITE:
        log_info("Writing calibration from %s to %s",input_calibration_path, path);
        struct calibration_parameters input_calibration;
        ret = read_input_calibration(input_calibration_path, &input_calibration);
        if (ret != 0) {
            log_info("Error reading input calibration from %s", input_calibration_path);
            return -1;
        }
        ret = write_calibration_parameters(path, &input_calibration);
        if (ret != 0) {
            log_error("Error writing calibration parameters to %s", path);
        }
        break;
    case INIT:
        log_info("Writing default values to eeprom at %s", path);
        write_calibration_parameters(path, (struct calibration_parameters *) &default_factory);
        break;
    default:
        log_error("No Mode (Read, Write, Init) provided");
        return -1;
    }

    log_info("Success");
    return ret;
}
