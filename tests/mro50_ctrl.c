/*
 * mRO50 oscillator control program.
 * Allows to read / write commands to the mRO50 oscillator
 */
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <oscillator-disciplining/oscillator-disciplining.h>

#include "log.h"
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

enum {
    COMMAND_READ,
    COMMAND_WRITE
};

enum {
    TYPE_FINE,
    TYPE_COARSE,
    TYPE_TEMP,
    TYPE_PARAM,
    TYPE_SERIAL_ACTIVATE,
    TYPE_TEMP_FIELD_A,
    TYPE_TEMP_FIELD_B
};

static void print_help(void)
{
    printf("usage: mro50_ctrl [-h] -d DEVICE -c COMMAND -t TYPE [WRITE_VALUE]\n");
    printf("- DEVICE: mrO50 device's path\n");
    printf("- COMMAND: 'read' or 'write'\n");
    printf("- TYPE: 'fine', 'coarse', 'temp' or 'parameters' \n");
    printf("- WRITE_VALUE: mandatory if command is write.\n");
    printf("- -h: prints help\n");
    return;
}

static int check_device(char * device)
{
    if (device == NULL) {
        log_error("Device path not provided!\n");
        return -1;
    }

    if (access(device, F_OK ) != -1)
        return 0;
    else {
        log_error("Device path does not exist\n");
        return -1;
    }
}


static int check_command(char * command)
{
    if (command == NULL) {
        log_error("Command not specified!\n");
        return -1;
    }

    if (strcmp(command, "read") == 0)
        return COMMAND_READ;
    else if (strcmp(command, "write") == 0)
        return COMMAND_WRITE;
    else {
        log_error("Unknown command %s\n", command);
        return -1;
    }
}


static int check_type(char * type)
{
    if (type == NULL) {
        log_error("Type not specified!\n");
        return -1;
    }

    if (strcmp(type, "fine") == 0)
        return TYPE_FINE;
    else if (strcmp(type, "coarse") == 0)
        return TYPE_COARSE;
    else if (strcmp(type, "temp") == 0)
        return TYPE_TEMP;
    else if (strcmp(type, "parameters") == 0)
        return TYPE_PARAM;
    else {
        log_error("Unknown type %s\n", type);
        return -1;
    }
}

int main(int argc, char ** argv)
{
    char * device;
    char * command;
    int command_int;
    char * type = NULL;
    int type_int;
    long int cmdl_value;
    uint32_t write_value;
    int index = 0;
    int c;
    int err;
    unsigned long ioctl_command = 1;

    log_set_level(LOG_INFO);

    while ((c = getopt(argc, argv, "d:c:t:h")) != -1)
    switch (c)
      {
      case 'd':
        device = optarg;
        break;
      case 'c':
        command = optarg;
        break;
      case 't':
        type = optarg;
        break;
    case 'h':
        print_help();
        return 0;
      case '?':
        if (optopt == 'd')
          fprintf (stderr, "Option -%c requires mRO50 device's path.\n", optopt);
        if (optopt == 'c')
          fprintf (stderr, "Option -%c requires an command type: either read or write.\n", optopt);
        if (optopt == 't')
          fprintf (stderr, "Option -%c requires an control values to command: either fine, coarse or serial_activate.\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr,
                   "Unknown option character `\\x%x'.\n",
                   optopt);
        return 1;
      default:
        abort ();
      }

    err = check_device(device);
    if (err != 0)
        return -1;

    err = check_command(command);
    if (err == -1)
        return err;
    command_int = err;

    err = check_type(type);
    if (err == -1)
        return err;
    type_int = err;

    int fd = open(device, O_RDWR);
    if (fd < 0) {
       log_error("Could not open mRo50 serial");
       return -1;
    }

    // Check for additional parameter if WRITE command is specified
    if (command_int == COMMAND_WRITE) {
        bool write_value_present = false;
        for (index = optind; index < argc; index++) {
            cmdl_value = atol(argv[index]);
            if (cmdl_value  < 0 ) {
                printf("Value to write must be positive");
                return -1;
            }
            write_value = (unsigned int)cmdl_value;
            write_value_present = true;
            break;
        }

        if (!write_value_present) {
            log_error("Write value not specified!");
            return -1;
        }
    }

    log_info("device = %s, command = %s, type = %s",
        device, command, type);

    if (command_int == COMMAND_READ) {
        uint32_t read_value = 0;
        if (type_int == TYPE_FINE)
            ioctl_command = MRO50_READ_FINE;
        else if (type_int == TYPE_COARSE)
            ioctl_command = MRO50_READ_COARSE;
        else if (type_int == TYPE_TEMP)
            ioctl_command = MRO50_READ_TEMP;
        else if (type_int == TYPE_PARAM) {
            ioctl_command = MRO50_READ_EEPROM_BLOB;
            struct disciplining_parameters params = {0};
            u8 buf[256] = {0};
            err = ioctl(fd, ioctl_command, buf);
            if (err != 0) {
                log_error("Error executing IOCTL");
                close(fd);
                return -1;
            }
            memcpy(&params, buf, sizeof(struct disciplining_parameters));
            print_disciplining_parameters(&params, LOG_INFO);
            close(fd);
            return 0;
        } else
            return -1;

        err = ioctl(fd, ioctl_command, &read_value);
        if (err != 0) {
            log_error("Error reading %s value", type);
            return -1;
        }
        if (type_int == TYPE_TEMP) {
            double temperature = compute_temp(read_value);
            log_info("Temperature is %f °C", temperature);
        } else
            log_info("%s value read is %d", type, read_value);
    } else if (command_int == COMMAND_WRITE) {
        if (type_int == TYPE_FINE) {
            ioctl_command = MRO50_ADJUST_FINE;
            if (write_value < FINE_RANGE_MIN
                || write_value > FINE_RANGE_MAX) {
                    log_error("value is out of range for fine control !");
                    return -1;
                }
        } else if (type_int == TYPE_COARSE) {
            ioctl_command = MRO50_ADJUST_COARSE;
            if (write_value > COARSE_RANGE_MAX) {
                    log_error("value is out of range for coarse control !");
                    return -1;
            }
        } else if (type_int == TYPE_TEMP) {
            log_warn("Cannot write temperature ");
            return 0;
        } else if (type_int == TYPE_PARAM) {
            ioctl_command = MRO50_WRITE_EEPROM_BLOB;
            struct disciplining_parameters params = {
                .ctrl_nodes_length = 3,
                .ctrl_load_nodes = {0.25,0.5,0.75},
                .ctrl_drift_coeffs = {1.2,0.0,-1.2},
                .coarse_equilibrium = 4186417,
                .ctrl_nodes_length_factory = 3,
                .ctrl_load_nodes_factory = {0.25,0.5,0.75},
                .ctrl_drift_coeffs_factory = {1.2,0.0,-1.2},
                .coarse_equilibrium_factory = -1,
                .calibration_valid = false,
                .calibration_date = 0
            };
            log_info("Disciplining parameters written:");
            print_disciplining_parameters(&params, LOG_INFO);
            u8 buf[256] = {0};
            memcpy(buf, &params, 256);
            err = ioctl(fd, ioctl_command, buf);
            close(fd);
            if (err != 0) {
                log_error("Error occured writing disciplining parameters: %d", err);
                return -1;
            }
            return 0;
        } else
            return -1;

        err = ioctl(fd, ioctl_command, &write_value);
        if (err != 0) {
            log_error("Error Writing %s value", type);
            return -1;
        }

        log_info("Wrote %s value", type);
    }

    close(fd);

    return 0;
}
