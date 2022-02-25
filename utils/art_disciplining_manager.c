/**
 * @file art_disciplining_manager.c
 * @brief Read and Write dsiciplining parameters to ART card's EEPROM
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

#include <oscillator-disciplining/oscillator-disciplining.h>

#include "config.h"
#include "eeprom.h"
#include "log.h"

enum Mode {
    ART_EEPROM_MANAGER_NONE,
    ART_EEPROM_MANAGER_READ,
    ART_EEPROM_MANAGER_WRITE,
    ART_EEPROM_MANAGER_INIT,
};

static int write_disciplining_parameters_to_eeprom(const char * path, struct disciplining_parameters *calibration)
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

static void read_disciplining_parameters_from_eeprom(const char *path, struct disciplining_parameters *dsc_parameters)
{
    FILE *fp = fopen(path,"rb");
    if (fp != NULL) {
        int ret = fread(dsc_parameters, sizeof(struct disciplining_parameters), 1, fp);
        log_debug("ret is %d, sizeof struct is %ld", ret, sizeof(struct disciplining_parameters));
        if (ret != 1) {
            log_error("Error reading calibration parameters from %s", path);
            return;
        }
        print_disciplining_parameters(dsc_parameters, LOG_INFO);
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

static int read_disciplining_parameters_from_config_file(const char *path, struct disciplining_parameters *result)
{
    double *ctrl_drift_coeffs;
    double *ctrl_load_nodes;
    struct config config;
    int ret;

    ret = config_init(&config, path);
    if (ret != 0)
        error(EXIT_FAILURE, -ret, "config_init(%s)", path);

    memcpy(result, &factory_parameters, sizeof(struct disciplining_parameters));

    result->calibration_valid = config_get_bool_default(&config, "calibration_valid", false);
    result->coarse_equilibrium = config_get_unsigned_number(&config, "coarse_equilibrium");
    result->ctrl_nodes_length = config_get_unsigned_number(&config, "ctrl_nodes_length");
    result->calibration_date = config_get_unsigned_number(&config, "calibration_date");

    ctrl_load_nodes = get_double_array_from_config(&config, "ctrl_load_nodes", result->ctrl_nodes_length);
    if (ctrl_load_nodes == NULL) {
        log_error("Could not get ctrl_load_nodes from config file at %s", path);
        return -1;
    }

    ctrl_drift_coeffs = get_double_array_from_config(&config, "ctrl_drift_coeffs", result->ctrl_nodes_length);
    if (ctrl_drift_coeffs == NULL) {
        log_error("Could not get ctrl_drift_coeffs from config file at %s", path);
        free(ctrl_load_nodes);
        return -1;
    }

    for (uint i = 0; i < result->ctrl_nodes_length; i++) {
        result->ctrl_load_nodes[i] = ctrl_load_nodes[i];
        result->ctrl_drift_coeffs[i] = ctrl_drift_coeffs[i];
    }

    log_info("Disciplining parameters that will be written in %s", path);
    print_disciplining_parameters(result, LOG_INFO);

    free(ctrl_drift_coeffs);
    free(ctrl_load_nodes);
    config_cleanup(&config);

    return 0;
}

static int write_disciplining_parameters_to_config_file(const char *path, struct disciplining_parameters *dsc_parameters)
{
    struct config config;
    char buffer[2048];
    char float_buffer[256];

    memset(&config, 0, sizeof(config));
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%d", dsc_parameters->coarse_equilibrium);
    config_set(&config, "coarse_equilibrium", buffer);


    sprintf(buffer, "%u", dsc_parameters->ctrl_nodes_length);
    config_set(&config, "ctrl_nodes_length", buffer);

    memset(buffer, 0, sizeof(buffer));
    for (int i = 0; i < dsc_parameters->ctrl_nodes_length; i++) {
        sprintf(float_buffer, "%f", dsc_parameters->ctrl_load_nodes[i]);
        strncat(buffer, float_buffer, strlen(float_buffer));
        if (i != dsc_parameters->ctrl_nodes_length - 1)
            strcat(buffer, ",");
    }
    config_set(&config, "ctrl_load_nodes", buffer);

    memset(buffer, 0, sizeof(buffer));
    for (int i = 0; i < dsc_parameters->ctrl_nodes_length; i++) {
        sprintf(float_buffer, "%f", dsc_parameters->ctrl_drift_coeffs[i]);
        strncat(buffer, float_buffer, strlen(float_buffer));
        if (i != dsc_parameters->ctrl_nodes_length - 1)
            strcat(buffer, ",");
    }
    config_set(&config, "ctrl_drift_coeffs", buffer);

    sprintf(buffer, "%s", dsc_parameters->calibration_valid ? "true" : "false");
    config_set(&config, "calibration_valid", buffer);

    config_set(&config, "calibration_date", buffer);
    sprintf(buffer, "%ld\n", dsc_parameters->calibration_date);

    config_dump(&config, buffer, 2048);

    FILE *fd = fopen(path, "w+");
    int return_val = fputs(buffer,fd);
    fclose(fd);
    return return_val == 1 ? 0 : -1;
}

int main(int argc, char *argv[])
{
    enum Mode mode = ART_EEPROM_MANAGER_NONE;
    char *input_calibration_path = NULL;
    char *output_file = NULL;
    char *path = NULL;
    int option;
    int ret = 0;
    bool write_factory = false;

    log_set_level(LOG_INFO);

    while ((option = getopt(argc, argv, ":p:w:fho:r")) != -1) {
        switch (option) {
        case 'r':
            mode = ART_EEPROM_MANAGER_READ;
            break;
        case 'w':
            mode = ART_EEPROM_MANAGER_WRITE;
            input_calibration_path = optarg;
            break;
        case 'o':
            output_file = optarg;
            break;
        case 'f':
            write_factory = true;
            break;
        case 'p':
            path = optarg;
            break;
        case ':':
            log_error("Option needs a value ");
            break;
        case 'h':
        default:
            log_info("art_disciplining_manager -p eeprom_path  [-w calibration.conf -r -f -o output_file_path -h]");
            log_info("\t-p eeprom_path: Path to the eeprom file");
            log_info("\t-w calibration.conf: Path to the calibration paramters file to write in the eeprom");
            log_info("\t-r: Read calibration parameters from the eeprom");
            log_info("\t-f: force write operation to write factory parameters");
            log_info("\t-o: output_file_path: write calibration parameters read in file");
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
    case ART_EEPROM_MANAGER_READ:
        log_info("Reading data from %s", path);
        struct disciplining_parameters dsc_parameters;
        read_disciplining_parameters_from_eeprom(path, &dsc_parameters);
        if (output_file) {
            log_info("Writing disciplining parameters read to %s", output_file);
            ret = write_disciplining_parameters_to_config_file(output_file, &dsc_parameters);
            if (ret != 0) {
                log_error("Error writing in config file");
            }
        }
        break;
    case ART_EEPROM_MANAGER_WRITE:
        if (write_factory) {
            log_info("Writing default calibration to %s", path);
            write_disciplining_parameters_to_eeprom(path, (struct disciplining_parameters *) &factory_parameters);
        } else {
            log_info("Writing calibration from %s to %s",input_calibration_path, path);
            struct disciplining_parameters input_calibration;
            ret = read_disciplining_parameters_from_config_file(input_calibration_path, &input_calibration);
            if (ret != 0) {
                log_info("Error reading input calibration from %s", input_calibration_path);
                return -1;
            }
            ret = write_disciplining_parameters_to_eeprom(path, &input_calibration);
            if (ret != 0) {
                log_error("Error writing calibration parameters to %s", path);
            }
        }
        break;
    default:
        log_error("No Mode (Read or Write) provided");
        return -1;
    }

    log_info("Success");
    return ret;
}
