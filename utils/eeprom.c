#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "eeprom.h"
#include "mRO50_ioctl.h"

#define min(a,b) a < b ? a : b

struct disciplining_parameters factory_parameters = {
    .ctrl_nodes_length = 3,
    .ctrl_load_nodes = {0.25,0.5,0.75},
    .ctrl_drift_coeffs = {0.0,0.0,0.0},
    .coarse_equilibrium = -1,
    .ctrl_nodes_length_factory = 3,
    .ctrl_load_nodes_factory = {0.25,0.5,0.75},
    .ctrl_drift_coeffs_factory = {1.2,0.0,-1.2},
    .coarse_equilibrium_factory = -1,
    .calibration_valid = false,
    .calibration_date = 0
};

static uint8_t gencrc(uint8_t *data, size_t len)
{
    uint8_t crc = 0xff;
    size_t i, j;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if ((crc & 0x80) != 0)
                crc = (uint8_t)((crc << 1) ^ 0x31);
            else
                crc <<= 1;
        }
    }
    return crc;
}

int write_eeprom(const char *path, struct eeprom_data *data, struct disciplining_parameters *calibration)
{
    int err = 0;

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        log_error("ERROR Opening file");
        return -1;
    }
    fwrite(calibration, 1, sizeof(*calibration), fp);
    int ret = fseek(fp, 2 * 256, SEEK_SET);
    if (ret != 0) {
        log_error("Error moving pointer accross file");
        err = -1;
    }
    else
        fwrite(data, 1, sizeof(*data), fp);
    fclose(fp);
    return err;
}

int read_eeprom_data(const char *path, struct eeprom_data *data)
{
    FILE *fp;
    int ret;

    if (!data) {
        return -EFAULT;
    }

    fp = fopen(path,"rb");
    if(!fp) {
        log_error("Could not open file at %s", path);
    }

    ret = fseek(fp, 2 * 256, SEEK_SET);
    if (ret != 0)
        log_error("Error moving pointer accross file");
    else {
        ret = fread(data, sizeof(struct eeprom_data), 1, fp);
        if(ret != 1)
            log_error("Could not read eeprom data");
        else
            print_eeprom_data(data);
    }
    fclose(fp);
    return ret;
}

int read_disciplining_parameters(const char *path, struct disciplining_parameters *dsc_parameters)
{
    FILE *fp;
    int ret;

    if (!dsc_parameters) {
        return -EFAULT;
    }

    fp = fopen(path,"rb");
    if(!fp) {
        log_error("Could not open file at %s", path);
    }

    ret = fread(dsc_parameters, sizeof(struct disciplining_parameters), 1, fp);
    if(ret != 1)
        log_error("Could no read calibration parameters from file %s", path);
    else
        print_disciplining_parameters(dsc_parameters, LOG_DEBUG);
    fclose(fp);
    return ret;
}

void init_eeprom_data(struct eeprom_data *data, char *serial_number)
{
    time_t s;
    struct tm* current_time;

    data->magic = 0xFBFB;
    data->format_version = 3;
    data->product_prodution_state = MP;
    data->product_version = 5;
    data->product_sub_version = 0;

    strncpy(data->product_name, "TIME CARD", min(sizeof("TIME CARD"), 20));

    strncpy(data->system_assembly_part_number, "19002225", min(sizeof("19002225") , 12));
    strncpy(data->fb_pcba_part_number, "13200014402", min(sizeof("13200014402"), 12));
    strncpy(data->fb_pcb_part_number, "13100010902", min(sizeof("13100010902") , 12));
    strncpy(data->od_pcba_part_number, "1003066A00", min(sizeof("1003066A00") , 12));

    strncpy(data->od_pcba_serial_number, serial_number, min(strlen(serial_number) , 12));
    strncpy(data->product_serial_number, serial_number, min(strlen(serial_number), 13));

    strncpy(data->system_manufacturer, "OROLIA", min(sizeof("OROLIA"), 8));
    strncpy(data->assembled_at, "ASTEEL", min(sizeof("ASTEEL"), 7));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
    strncpy(data->product_part_number, "00000000", min(sizeof("00000000"), 8));
    strncpy(data->product_asset_tag, "000000000000", min(sizeof("000000000000"), 10));
    strncpy(data->local_mac_address, "000000000000", min(sizeof("000000000000"),12));
    strncpy(data->extended_mac_address_base, "000000000000", min(sizeof("000000000000"),12));
    data->extended_mac_address_size = 0;
    strncpy(data->eeprom_location_on_fabric, "TIME CARD", min(sizeof("TIME CARD"), 9));
    strncpy(data->pcb_manufacturer, "JOVE", min(sizeof("JOVE"), 4));
#pragma GCC diagnostic pop

    /*Fill in date with local time */
    s = time(NULL);
    current_time = localtime(&s);

    data->system_manufacturing_date_day = current_time->tm_mday;
    data->system_manufacturing_date_month = current_time->tm_mon + 1;
    data->system_manufacturing_date_year = current_time->tm_year + 1900;

    data->crc8 = gencrc((uint8_t *) data, sizeof(struct eeprom_data) - 1);
}

int write_disciplining_parameters_to_mro50(const char * path, struct disciplining_parameters *calibration)
{
    int fp = open(path, O_RDWR);
    unsigned char buf[512] = {0x0};
    int ret = 0;

    if (fp < 0) {
        log_error("Could not open file at %s", path);
        return -1;
    }
    memcpy(buf, calibration, sizeof(*calibration));
    if (ioctl(fp, MRO50_WRITE_EXTENDED_EEPROM_BLOB, buf) != 0) {
      log_error("Could not write EEPROM BLOB");
      ret = -1;
    }
    close(fp);
    return ret;
}

void read_disciplining_parameters_from_mro50(const char *path, struct disciplining_parameters *dsc_parameters)
{
    int fp = open(path, O_RDWR);
    unsigned char buf[512] = {0};
    if (fp < 0) {
        log_error("Could not open file at %s", path);
        return;
    }
    if (ioctl(fp, MRO50_READ_EXTENDED_EEPROM_BLOB, buf) != 0) {
        log_error("Could not read EEPROM BLOB");
    } else {
        memcpy(dsc_parameters, buf, sizeof(*dsc_parameters));
    }
    return;
}
