/**
 * @file art_temperature_table_manager.c
 * @brief Read and Write temperature table from/to ART card's EEPROM
 * @version 0.1
 * @date 2022-06-13
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <oscillator-disciplining/oscillator-disciplining.h>

#include "eeprom.h"
#include "log.h"

enum Mode {
    ART_TEMPERATURE_TABLE_NONE,
    ART_TEMPERATURE_TABLE_READ,
    ART_TEMPERATURE_TABLE_WRITE
};

static int float_array_parser(const char* value, float **result) {
    char *endptr;
    char *ptr;
    const char *delim = ",";
    float buffer[2];
    int parsed = 0;
    float value_float;

    errno = 0;

    ptr = strtok((char *) value, delim);
    while (ptr != NULL)
    {
        if (parsed >= 2) {
            return -ERANGE;
        }
        value_float = strtold(ptr, &endptr);
        if (value_float == HUGE_VAL ||
            (value_float == 0 && errno == ERANGE))
            return -ERANGE;
        buffer[parsed] = value_float;
        parsed++;
        ptr = strtok(NULL, delim);
    }

    float *values = malloc(parsed * sizeof(float));
    if (values == NULL) {
        return -ENOMEM;
    }
    for (int i = 0; i < parsed; i++) {
        values[i] = buffer[i];
    }

    *result = values;

    return parsed;
}

int main(int argc, char *argv[])
{
    struct disciplining_parameters dsc_parameters;
    enum Mode mode = ART_TEMPERATURE_TABLE_NONE;
    char *input_file = NULL;
    char *output_file = NULL;
    char *mro50_path;
    int option;
    FILE *fp;
    log_set_level(LOG_INFO);

    while ((option = getopt(argc, argv, "m:w:ho:r")) != -1) {
        switch (option)
        {
        case 'm':
            mro50_path = optarg;
            break;
        case 'r':
            mode = ART_TEMPERATURE_TABLE_READ;
            break;
        case 'w':
            mode = ART_TEMPERATURE_TABLE_WRITE;
            input_file = optarg;
            break;
        case 'o':
            output_file = optarg;
            break;
        case '?':
            if (optopt == 'm')
                log_error("Option -%c requires path to eeprom.\n", optopt);
            if (optopt == 'w')
                log_error("Option -%c path to input temperature table file.\n", optopt);
            if (optopt == 'o')
                log_error("Option -%c path to output temperature table file.\n", optopt);
            return -1;
            break;
        case 'h':
        default:
            log_info("art_temperature_table_manager -m mro50_path [-w input_table.txt | -r -o output_table.txt] -h");
            log_info("\t-m mro50_path: Path to the mRO50 device file");
            log_info("\t-w input_table.txt: Path to input temperature table to write to EEPROM");
            log_info("\t-r: Read temperature table from EEPROM");
            log_info("\t-o: output_file_path: write calibration parameters read in file");
            log_info("\t-h: print help");
            break;
        }
    }

    if (mro50_path == NULL) {
        log_error("No mro50 path provided!");
        return -1;
    }

    switch (mode) {
        case ART_TEMPERATURE_TABLE_READ:
            read_disciplining_parameters_from_mro50(mro50_path, &dsc_parameters);
            print_temperature_table(dsc_parameters.mean_fine_over_temperature, LOG_INFO);
            if (output_file != NULL) {
                log_info("writing value in %s", output_file);
                fp = fopen(output_file, "w");
                if (!fp) {
                    log_error("cannot write to file at %s", output_file);
                    return -1;
                }
                for (int i = 0; i < MEAN_TEMPERATURE_ARRAY_MAX  ; i++) {
                    char line[256];
                    sprintf(line, "%.2f,%.2f\n",  MIN_TEMPERATURE + (float) i / STEPS_BY_DEGREE, (float) dsc_parameters.mean_fine_over_temperature[i] / 10.0);
                    fputs(line, fp);
                }
                fclose(fp);
            }
            break;
        case ART_TEMPERATURE_TABLE_WRITE:
            read_disciplining_parameters_from_mro50(mro50_path, &dsc_parameters);
            fp = fopen(input_file, "r");
            char *line = NULL;
            size_t len = 0;
            ssize_t read = 0;
            if (!fp) {
                log_error("cannot open file at %s", input_file);
                return -1;
            }
            while((read = getline(&line, &len, fp)) != -1) {
                log_debug("%s", line);
                char tmp[256];
                float *temp_value_tuple = NULL;
                strcpy(tmp, line);
                int num_floats = float_array_parser(tmp, &temp_value_tuple);
                if (num_floats == 2) {
                    int temperature_index;
                     if (temp_value_tuple[0] < MIN_TEMPERATURE || temp_value_tuple[0] >= MAX_TEMPERATURE) {
                        log_error("Temperature %.2f is out of range", temp_value_tuple[0]);
                        return -1;
                    }
                    temperature_index = (int) floor(STEPS_BY_DEGREE * (temp_value_tuple[0] - MIN_TEMPERATURE));
                    log_info("writing %.2f to range [%.2f, %.2f[",
                        (float) round(temp_value_tuple[1] * 10) / 10,
                        (temperature_index + STEPS_BY_DEGREE * MIN_TEMPERATURE) / STEPS_BY_DEGREE,
                        (temperature_index + 1 + STEPS_BY_DEGREE * MIN_TEMPERATURE) / STEPS_BY_DEGREE
                    );
                    dsc_parameters.mean_fine_over_temperature[temperature_index] = round(temp_value_tuple[1] * 10);
                }
                free(temp_value_tuple);
            }
            free(line);
            int ret = write_disciplining_parameters_to_mro50(mro50_path, &dsc_parameters);
             if (ret != 0) {
                log_error("Error writing temperature table to mRo50");
                return -1;
            }
        default:
            break;
    }
    return 0;
}