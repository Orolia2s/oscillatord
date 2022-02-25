#ifndef EEPROM_H
#define EEPROM_H

#include <stdint.h>

#include <oscillator-disciplining/oscillator-disciplining.h>

#include "log.h"

enum PRODUCT_PRODUCTION_STATE {
    EVT,
    DVT,
    PVT,
    MP
};

/**
 * @brief ART card EEPROM format data
 */
struct __attribute__((__packed__)) eeprom_data {
    /** Magic word, fixed always "FBFB" */
    uint16_t magic; // 0x00
    /** Binary as unsigned integer, small endian */
    uint8_t format_version;// 0x02
    /** String */
    char product_name[20];//0x03
    /** XX-XXXXXX, dash not in the field */
    char product_part_number[8]; //0x17
    /** Will report to top lvel and consists of full system assembly */
    char system_assembly_part_number[12]; //0x1F
    char fb_pcba_part_number[12];// 0x2B
    char fb_pcb_part_number[12];// 0x37
    char od_pcba_part_number[13];// 0x43
    char od_pcba_serial_number[13];// 0x50
    /** 1=EVT, 2=DVT, 3=PVT, 4=MP */
    uint8_t product_prodution_state;// 0x5D
    /** Revision of the ART Card board */
    uint8_t product_version;// 0x5E
    /** Sub version of the revision */
    uint8_t product_sub_version;// 0x5F
    char product_serial_number[13];// 0x60
    char product_asset_tag[12];// 0x6D
    /** ODM name */
    char system_manufacturer[8];// 0x79
    /** mm-dd-yyyy, dash will not appear in field */
    uint16_t system_manufacturing_date_year;// 0x81
    uint8_t system_manufacturing_date_month;// 0x83
    uint8_t system_manufacturing_date_day;// 0x84
    char pcb_manufacturer[8];// 0x85
    char assembled_at[8];// 0x8D
    /** No MAC address on ART card, equals 00:00:00:00:00:00 */
    char local_mac_address[12];// 0x95
    /** No MAC address on ART card, equals 00:00:00:00:00:00 */
    char extended_mac_address_base[12];// 0xA1
    /** No MAC address, equal 0 */
    uint16_t extended_mac_address_size;// 0xAD
    char eeprom_location_on_fabric[20];// 0xAF
    /** Checksum CRC8 */
    char crc8;// 0xC3
};

/**
 * @brief Default value for disciplining parameters
 */
extern const struct disciplining_parameters factory_parameters;

static inline void print_eeprom_data(struct eeprom_data *data)
{
    log_info("EEPROM data is:");
    log_info("Magic: 0x%x", data->magic);
    log_info("Format version: %d", data->format_version);
    log_info("Product Name: %s", data->product_name);
    log_info("Product PN: %s", data->product_part_number);
    log_info("System assembly PN: %s", data->system_assembly_part_number);
    log_info("FB PCBA PN: %s", data->fb_pcba_part_number);
    log_info("FB PCB PN: %s", data->fb_pcb_part_number);
    log_info("OD PCBA PN: %s", data->od_pcba_part_number);
    log_info("OD PCVA SN: %s", data->od_pcba_serial_number);
    log_info("Product Production state: %d", data->product_prodution_state);
    log_info("Product version: %d", data->product_version);
    log_info("Product subversion: %d", data->product_sub_version);
    log_info("Product SN: %s", data->product_serial_number);
    log_info("Product asset tag: %s", data->product_asset_tag);
    log_info("System manufacturer: %s", data->system_manufacturer);
    log_info("System manufacturer date: %u-%u-%u", data->system_manufacturing_date_day, data->system_manufacturing_date_month, data->system_manufacturing_date_year);
    log_info("PCB Manufacturer: %s", data->pcb_manufacturer);
    log_info("Assembled at: %s", data->assembled_at);
    log_info("Local MAC address: %s", data->local_mac_address);
    log_info("Extended MAC address: %s", data->extended_mac_address_base);
    log_info("Extended MAC address size: %d", data->extended_mac_address_size);
    log_info("EEPROM Location on fabric: %s", data->eeprom_location_on_fabric);
    log_info("CRC8: %c", data->crc8);
    return;
}

int write_eeprom(const char *path, struct eeprom_data *data, struct disciplining_parameters *calibration);
int read_eeprom_data(const char *path, struct eeprom_data *data);
int read_disciplining_parameters(const char *path, struct disciplining_parameters *dsc_parameters);
void init_eeprom_data(struct eeprom_data *data, char *serial_number);


#endif /* EEPROM_H */