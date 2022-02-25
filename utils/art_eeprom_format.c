/**
 * @file art_eeprom_format.c
 * @author Charles PARENT (charles.parent@orolia2s.com)
 * @brief Program to write factory data on EEPROM of ART cards
 * @version 0.1
 * @date 2022-03-04
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
#include <time.h>


#include "log.h"
#include "eeprom.h"

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
    printf("art-eeprom-format -p PATH -s SERIAL_NUMBER\n");
    printf("Parameters:\n");
    printf("PATH: path of the file/EEPROM data should be written from\n");
    printf("SERIAL_NUMBER: Serial number that should be written within data." \
        "Serial must start with an F followed by 8 numerical caracters\n");
}

int main(int argc, char *argv[])
{
    char *serial_number = NULL;
    char *path = NULL;
    int ret;
    int c;


    while ((c = getopt(argc, argv, "p:s:h")) != -1) {
        switch (c) {
        case 'p':
            path = optarg;
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
                fprintf (stderr, "Option -%c requires path to eeprom.\n", optopt);
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
    log_info("Writing data to %s", path);

    struct eeprom_data *data = (struct eeprom_data *) calloc(1, sizeof(struct eeprom_data));

    init_eeprom_data(data, serial_number);

    print_eeprom_data(data);
    log_info("\n");
    ret = write_eeprom(path, data, (struct disciplining_parameters *) &factory_parameters);
    if (ret != 0) {
        log_error("Error writing eeprom data");
        goto end;
    }

    log_info("Reading back data just written:");
    read_eeprom_data(path, data);

    struct disciplining_parameters dsc_params;
    read_disciplining_parameters(path, &dsc_params);

end:
    free(data);
    data = NULL;
    return 0;
}
