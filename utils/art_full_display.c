/**
 * @file art_full_display.c
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

static void print_help(void) {
    printf("art-full-display -p PATH -m MRO50_DEVICE\n");
    printf("Parameters:\n");
    printf("- -p PATH: path of the file/EEPROM data\n");
    printf("- -m PATH: path of the MRO\n");

}

int main(int argc, char *argv[])
{
    uint32_t factory_coarse;
    uint32_t factory_fine;
    char *mro50_path = NULL;
    char *path = NULL;
    int ret = 0;
    int c;
    int mro50;

    struct eeprom_data data_read0;
    struct disciplining_parameters dsc_params0;

    while ((c = getopt(argc, argv, "p:m:h")) != -1) {
        switch (c) {
        case 'p':
            path = optarg;
            break;
        case 'm':
            mro50_path = optarg;
            break;
        case 'h':
            print_help();
            break;
        }
    }

    if (!path) {
        printf("Please provide path to EEPROM file\n");
        return -1;
    }

        read_disciplining_parameters(path, &dsc_params0);
        printf("\n");
        read_eeprom_data(path, &data_read0);

    if (!mro50_path) {
        printf("Please provide path to MRO\n");
        return -1;
    }

    mro50 = open(mro50_path, O_RDWR);
    if (mro50 > 0) {
        ret = ioctl(mro50, MRO50_READ_COARSE, &factory_coarse);
        if (ret != 0)
            log_error("Error reading factory coarse of mRO50");
        else
            printf("MRO red coarse: %u",factory_coarse);
        close(mro50);
    } else {
        log_error("Could not open mRO50 device at %s", mro50_path);
    }

    log_set_level(LOG_INFO);

    mro50 = open(mro50_path, O_RDWR);
    if (mro50 > 0) {
        ret = ioctl(mro50, MRO50_READ_FINE, &factory_fine);
        if (ret != 0)
            log_error("Error reading factory fine of mRO50");
        else
            printf("MRO red fine: %u",factory_fine);
        close(mro50);
    } else {
        log_error("Could not open mRO50 device at %s", mro50_path);
    }
}
