/*
 * mRO50 oscillator control program.
 * Allows to read / write commands to the mRO50 oscillator
 */
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

#include "utils.h"

typedef u_int32_t u32;
typedef u_int8_t u8;

/** Minimum possible value of coarse control */
#define COARSE_RANGE_MIN 0
/** Maximum possible value of coarse control */
#define COARSE_RANGE_MAX 4194303
/** Minimum possible value of fine control */
#define FINE_RANGE_MIN 1600
/** Maximum possible value of fine control */
#define FINE_RANGE_MAX 3200

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

#define MRO50_READ_EEPROM_BLOB _IOR('M', 8, u8*)
#define MRO50_WRITE_EEPROM_BLOB _IOW('M', 8, u8*)

#endif /* MRO50_IOCTL_H */
/*---------------------------------------------------------------------------*/
#define CALIBRATION_POINTS_MAX 10

struct disciplining_parameters {
	/**
	 * Array containing the control node, in percentage
	 * value of the control range.
	 * Array contains ctrl_nodes_length valid values.
	 */
	float ctrl_load_nodes[CALIBRATION_POINTS_MAX];
	/**
	 * Array of drift coefficients for each control node.
	 * Array contains ctrl_nodes_length valid values.
	 */
	float ctrl_drift_coeffs[CALIBRATION_POINTS_MAX];
	/** Equilibrium Coarse value define during calibration */
	/**
	 * Array containing the control node, in percentage
	 * value of the control range.
	 * Array contains ctrl_nodes_length_factory valid values.
	 */
	float ctrl_load_nodes_factory[3];
	/**
	 * Array of drift coefficients for each control node.
	 * Array contains ctrl_nodes_length_factory valid values.
	 */
	float ctrl_drift_coeffs_factory[3];
	/** Equilibrium Coarse value for factory_settings */
	int32_t coarse_equilibrium_factory;
	int32_t coarse_equilibrium;
	/** Date at which calibration has been made */
	time_t calibration_date;
	/** Factory Settings that can be used with any mRO50 */
	/** Number of control nodes in ctrl_load_nodes_factory */
	uint8_t ctrl_nodes_length_factory;
	/** Number of control nodes in ctrl_load_nodes */
	uint8_t ctrl_nodes_length;
	/** Indicate wether calibration parameters are valid */
	bool calibration_valid;
};

enum {
    COMMAND_READ,
    COMMAND_WRITE
};

enum {
    TYPE_FINE,
    TYPE_COARSE,
    TYPE_TEMP,
    TYPE_PARAM
};

static void print_help(void)
{
    printf("usage: mro50_ctrl [-h] -d DEVICE -c COMMAND -t TYPE [WRITE_VALUE]\n");
    printf("- DEVICE: mrO50 device's path\n");
    printf("- COMMAND: 'read' or 'write'\n");
    printf("- TYPE: 'fine' of 'coarse' or 'temp' 'parameters '(temp is read only)\n");
    printf("- WRITE_VALUE: mandatory if command is write.\n");
    printf("- -h: prints help\n");
    return;
}

static int check_device(char * device)
{
    if (device == NULL) {
        printf("Device path not provided!\n");
        return -1;
    }

    if (access(device, F_OK ) != -1)
        return 0;
    else {
        printf("Device path does not exist\n");
        return -1;
    }
}


static int check_command(char * command)
{
    if (command == NULL) {
        printf("Command not specified!\n");
        return -1;
    }

    if (strcmp(command, "read") == 0)
        return COMMAND_READ;
    else if (strcmp(command, "write") == 0)
        return COMMAND_WRITE;
    else {
        printf("Unknown command %s\n", command);
        return -1;
    }
}


static int check_type(char * type)
{
    if (type == NULL) {
        printf("Type not specified!\n");
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
        printf("Unknown type %s\n", type);
        return -1;
    }
}


static void print_disciplining_parameters(struct disciplining_parameters *calibration)
{
    printf("Calibration parameters:\n");
    printf("ctrl_nodes_length = %d\n", calibration->ctrl_nodes_length);
    printf("ctrl_load_nodes[] =");
    if (calibration->ctrl_nodes_length > 0 && calibration->ctrl_nodes_length <= CALIBRATION_POINTS_MAX)
        for (int i = 0; i < calibration->ctrl_nodes_length; i++)
            printf(" %f",calibration->ctrl_load_nodes[i]);
    printf("\n");

    printf("ctrl_drift_coeffs[] =");
    if (calibration->ctrl_nodes_length > 0 && calibration->ctrl_nodes_length <= CALIBRATION_POINTS_MAX)
        for (int i = 0; i < calibration->ctrl_nodes_length; i++)
            printf(" %f", calibration->ctrl_drift_coeffs[i]);
    printf("\n");
    char buff[20];
    struct tm * timeinfo;
    timeinfo = localtime(&calibration->calibration_date);
    strftime(buff, sizeof(buff), "%b %d %Y", timeinfo);
    printf("Date of calibration: %s\n", buff);

    printf("coarse_equilibrium = %d\n", calibration->coarse_equilibrium);
    printf("calibration_valid = %d\n", calibration->calibration_valid);

    printf("ctrl_nodes_length_factory = %d\n", calibration->ctrl_nodes_length_factory);
    printf("ctrl_load_nodes_factory[] =");
    if (calibration->ctrl_nodes_length_factory > 0 && calibration->ctrl_nodes_length_factory <= CALIBRATION_POINTS_MAX)
        for (int i = 0; i < calibration->ctrl_nodes_length_factory; i++)
    printf(" %f", calibration->ctrl_load_nodes_factory[i]);
    printf("\n");

    printf("ctrl_drift_coeffs_factory[] =");
    if (calibration->ctrl_nodes_length_factory > 0 && calibration->ctrl_nodes_length_factory <= CALIBRATION_POINTS_MAX)
        for (int i = 0; i < calibration->ctrl_nodes_length_factory; i++)
            printf(" %f", calibration->ctrl_drift_coeffs_factory[i]);
    printf("\n");

    printf("coarse_equilibrium_factory = %d\n", calibration->coarse_equilibrium_factory);

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
          fprintf (stderr, "Option -%c requires an control values to command: either fine or coarse.\n", optopt);
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
       printf("Could not open mRo50 serial\n");
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
            printf("Write value not specified!\n");
            return -1;
        }
    }

    printf ("device = %s, command = %s, type = %s\n",
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
                printf("Error executing IOCTL\n");
                close(fd);
                return -1;
            }
            memcpy(&params, buf, sizeof(struct disciplining_parameters));
            print_disciplining_parameters(&params);
            close(fd);
            return 0;
        } else
            return -1;

        err = ioctl(fd, ioctl_command, &read_value);
        if (err != 0) {
            printf("Error reading %s value\n", type);
            return -1;
        }
        if (type_int == TYPE_TEMP) {
            double temperature = compute_temp(read_value);
            printf("Temperature is %f °C", temperature);
        } else
            printf("%s value read is %d\n", type, read_value);
    } else if (command_int == COMMAND_WRITE) {
        if (type_int == TYPE_FINE) {
            ioctl_command = MRO50_ADJUST_FINE;
            if (write_value < FINE_RANGE_MIN
                || write_value > FINE_RANGE_MAX) {
                    printf("value is out of range for fine control !\n");
                    return -1;
                }
        }
        else if (type_int == TYPE_COARSE) {
            ioctl_command = MRO50_ADJUST_COARSE;
            if (write_value > COARSE_RANGE_MAX) {
                    printf("value is out of range for coarse control !\n");
                    return -1;
                }
        } else if (type_int == TYPE_TEMP) {
            printf("Cannot write temperature \n");
            return 0;
        } else if (type_int == TYPE_PARAM) {
            ioctl_command = MRO50_WRITE_EEPROM_BLOB;
            struct disciplining_parameters params = {
                .ctrl_nodes_length = 3,
                .ctrl_load_nodes = {0.25,0.5,0.75},
                .ctrl_drift_coeffs = {0.121212,0.424242,-0.181818},
                .coarse_equilibrium = 4186417,
                .ctrl_nodes_length_factory = 3,
                .ctrl_load_nodes_factory = {0.25,0.5,0.75},
                .ctrl_drift_coeffs_factory = {1.2,0.0,-1.2},
                .coarse_equilibrium_factory = -1,
                .calibration_valid = true,
                .calibration_date = 0
            };
            printf("Disciplining parameters written:\n");
            print_disciplining_parameters(&params);
            u8 buf[256] = {0};
            memcpy(buf, &params, sizeof(struct disciplining_parameters));
            err = ioctl(fd, ioctl_command, buf);
            close(fd);
            if (err != 0) {
                printf("Error occured writing disciplining parameters: %d\n", err);
                return -1;
            }
            return 0;
        }
        else
            return -1;

        err = ioctl(fd, ioctl_command, &write_value);
        if (err != 0) {
            printf("Error Writing %s value\n", type);
            return -1;
        }

        printf("Wrote %s value\n", type);
    }

    close(fd);

    return 0;
}
