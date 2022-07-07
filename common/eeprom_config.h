/**
 * @file eeprom_config.h
 * @brief Header for functions managing eeprom R/W part (disciplining_config and temperature_table)
 */
#ifndef EEPROM_CONFIG_H_
#define EEPROM_CONFIG_H_

#include <linux/limits.h>
#include <oscillator-disciplining/oscillator-disciplining.h>

#define DISCIPLINING_CONFIG_FILE_SIZE 144
#define TEMPERATURE_TABLE_FILE_SIZE 368

int read_disciplining_parameters_from_eeprom(
    char disciplining_config_path[PATH_MAX],
    char temperature_table_path[PATH_MAX],
    struct disciplining_parameters *dsc_params
);
int write_disciplining_parameters_in_eeprom(
    char disciplining_config_path[PATH_MAX],
    char temperature_table_path[PATH_MAX],
    struct disciplining_parameters *dsc_params
);
#endif /* EEPROM_CONFIG_H_ */
