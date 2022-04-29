/**
 * @file art_eeprom_display.c
 * @author Hugo FOLCHER (hugo.folcher@orolia2s.com)
 * @brief Program to read data from EEPROM of ART cards
 * @version 0.1
 * @date 2022-04-29
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <time.h>
#include <unistd.h>

#include "eeprom.h"
#include "log.h"
#include "mRO50_ioctl.h"

static void removeChar(char * str, char charToRemmove){
    int i, j;
    int len = strlen(str);
    for(i=0; i<len; i++)
        if(str[i] == charToRemmove) {
            for(j=i; j<len; j++)
                str[j] = str[j+1];
            len--;
            i--;
        }
}

static bool validate_serial(char *serial) {
    if (!serial)
        return false;
    removeChar(serial, '-');
    if (strncmp("F", serial, 1) != 0) {
        printf("First letter of the serial must be an F\n");
        return false;
    } else if(strlen(serial) != 9) {
        printf("Serial must contain exactly 9 characters without \'-\'\n");
        return false;
    }
    for (int i = 1; i < 9; i++){
        if (!isdigit(serial[i])) {
            printf("character %d is not a digit\n", i+1);
            return false;
        }
    }
    return true;
}

static void print_help(void) {
    printf("art-eeprom-display -p PATH -s SERIAL_NUMBER -d MRO50_DEVICE -c COARSE_VALUE\n");
    printf("Parameters:\n");
    printf("- -p PATH: path of the file/EEPROM data should be written from\n");

}

int main(int argc, char *argv[])
{
    bool factory_coarse_valid = false;
    char *serial_number = NULL;
    char *mro50_path = NULL;
    uint32_t factory_coarse;
    char *path = NULL;
    int ret = 0;
    int c;

    struct eeprom_data data_read0;

    struct disciplining_parameters dsc_params0;

    while ((c = getopt(argc, argv, "c:d:p:s:h:r")) != -1) {
        switch (c) {
        case 'p':
            path = optarg;
            read_disciplining_parameters(path, &dsc_params0);
            read_eeprom_data(path, &data_read0);
            break;
            break;
        
            return -1;
            break;
        }
    }
    if (!path) {
        printf("Please provide path to EEPROM file to write\n");
        return -1;
    }
    if (!validate_serial(serial_number)) {
        printf("Serial number is not valid\n");
        return -1;
    }

    log_set_level(LOG_INFO);

    if (factory_coarse_valid) {
        log_info("%d will be used as coarse value", factory_coarse);
    } else if(mro50_path) {
        int mro50 = open(mro50_path, O_RDWR);
        if (mro50 > 0) {
            ret = ioctl(mro50, MRO50_READ_COARSE, &factory_coarse);
            if (ret != 0)
                log_error("Error reading factory coarse of mRO50, will not be saved in EEPROM");
            else
                factory_coarse_valid = true;
            close(mro50);
        } else {
            log_error("Could not open mRO50 device at %s", mro50_path);
        }
    } else {
        log_warn("No Coarse value given nor path to mRO50 device provided, factory value of mRO50 will not be written in EEPROM");
    }
    log_info("Writing data to %s...", path);

    struct eeprom_data *data = (struct eeprom_data *) calloc(1, sizeof(struct eeprom_data));

    init_eeprom_data(data, serial_number);

    print_eeprom_data(data);
    if (factory_coarse_valid)
        factory_parameters.coarse_equilibrium_factory = (int32_t) factory_coarse;

    ret = write_eeprom(path, data, (struct disciplining_parameters *) &factory_parameters);
    if (ret != 0) {
        log_error("Error writing eeprom data");
        goto end;
    }

    log_info("Reading back data just written...");
    struct eeprom_data data_read;
    read_eeprom_data(path, &data_read);
    ret = memcmp(data, &data_read, sizeof(struct eeprom_data));
    print_eeprom_data(&data_read);
    if (ret != 0) {
        log_error("Error writing data to eeprom");
        ret = -1;
    } else {
        log_info("Data correctly written");
    }

    struct disciplining_parameters dsc_params;
    read_disciplining_parameters(path, &dsc_params);

end:
    free(data);
    data = NULL;
    return ret;
}
