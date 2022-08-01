#include <assert.h>
#include <stdio.h>

#include "log.h"
#include "eeprom_config.h"

#include <oscillator-disciplining/oscillator-disciplining.h>

int main(int argc, char *argv[])
{
    struct disciplining_parameters_V_0 dsc_params;
    int i, ret;

    log_set_level(LOG_INFO);

    // fill in structure
    for (i = 0; i < CALIBRATION_POINTS_MAX; i++) {
        dsc_params.ctrl_load_nodes[i] = i;
        dsc_params.ctrl_drift_coeffs[i] = (float) i / 10.0;
    }

    for (i = 0; i < 3; i ++) {
        dsc_params.ctrl_load_nodes_factory[i] = i * 10;
        dsc_params.ctrl_drift_coeffs_factory[i] = (float) i * 10 + (float) i / 10.0;
    }
    dsc_params.coarse_equilibrium_factory = 0;
    dsc_params.coarse_equilibrium = 0;
    dsc_params.calibration_date = 0;
    dsc_params.ctrl_nodes_length = 0;
    dsc_params.ctrl_nodes_length_factory = 0;
    dsc_params.calibration_valid = true;
    dsc_params.estimated_equilibrium_ES = 0;
    for (i = 0; i < MEAN_TEMPERATURE_ARRAY_MAX; i++) {
        dsc_params.mean_fine_over_temperature[i] = i;
    }

    log_info("Size of struct disciplining_parameters_V_0 dsc_params IS %d", sizeof(struct disciplining_parameters_V_0));

    log_info("Offset in struct for field ctrl_load_nodes is %d", (uint8_t *) &dsc_params.ctrl_load_nodes - (uint8_t *) &dsc_params);
    log_info("Offset in struct for field ctrl_drift_coeffs is %d", (uint8_t *) &dsc_params.ctrl_drift_coeffs - (uint8_t *) &dsc_params);
    log_info("Offset in struct for field ctrl_load_nodes_factory is %d", (uint8_t *) &dsc_params.ctrl_load_nodes_factory - (uint8_t *) &dsc_params);
    log_info("Offset in struct for field ctrl_drift_coeffs_factory is %d", (uint8_t *) &dsc_params.ctrl_drift_coeffs_factory - (uint8_t *) &dsc_params);
    log_info("Offset in struct for field coarse_equilibrium_factory is %d", (uint8_t *) &dsc_params.coarse_equilibrium_factory - (uint8_t *) &dsc_params);
    log_info("Offset in struct for field coarse_equilibrium is %d", (uint8_t *) &dsc_params.coarse_equilibrium - (uint8_t *) &dsc_params);
    log_info("Offset in struct for field calibration_date is %d", (uint8_t *) &dsc_params.calibration_date - (uint8_t *) &dsc_params);
    log_info("Offset in struct for field ctrl_nodes_length_factory is %d", (uint8_t *) &dsc_params.ctrl_nodes_length_factory - (uint8_t *) &dsc_params);
    log_info("Offset in struct for field ctrl_nodes_length is %d", (uint8_t *) &dsc_params.ctrl_nodes_length - (uint8_t *) &dsc_params);
    log_info("Offset in struct for field calibration_valid is %d", (uint8_t *) &dsc_params.calibration_valid - (uint8_t *) &dsc_params);
    log_info("Offset in struct for field estimated_equilibrium_ES is %d", (uint8_t *) &dsc_params.estimated_equilibrium_ES - (uint8_t *) &dsc_params);
    log_info("Offset in struct for field mean_fine_over_temperature is %d", (uint8_t *) &dsc_params.mean_fine_over_temperature - (uint8_t *) &dsc_params);

    ret = write_file("/tmp/test_structures.txt", (char *) &dsc_params, sizeof(struct disciplining_parameters_V_0));
    log_info("Wrote data, ret is %d", ret);



    struct disciplining_config_V_1 dsc_params_v1;
        // fill in structure
    dsc_params_v1.header = 'O';
    dsc_params_v1.version = 1;
    for (i = 0; i < CALIBRATION_POINTS_MAX; i++) {
        dsc_params_v1.ctrl_load_nodes[i] = i;
        dsc_params_v1.ctrl_drift_coeffs[i] = (float) i / 10.0;
    }

    for (i = 0; i < 3; i ++) {
        dsc_params_v1.ctrl_load_nodes_factory[i] = i * 10;
        dsc_params_v1.ctrl_drift_coeffs_factory[i] = (float) i * 10 + (float) i / 10.0;
    }
    dsc_params_v1.coarse_equilibrium_factory = 0;
    dsc_params_v1.coarse_equilibrium = 0;
    dsc_params_v1.calibration_date = 0;
    dsc_params_v1.ctrl_nodes_length = 0;
    dsc_params_v1.ctrl_nodes_length_factory = 0;
    dsc_params_v1.calibration_valid = true;
    dsc_params_v1.estimated_equilibrium_ES = 0;

    log_info("Size of struct disciplining_config_V_1 dsc_params_v1 IS %d", sizeof(struct disciplining_config_V_1));

    log_info("Offset in struct for field header is %d", (uint8_t *) &dsc_params_v1.header - (uint8_t *) &dsc_params_v1);
    log_info("Offset in struct for field version is %d", (uint8_t *) &dsc_params_v1.version - (uint8_t *) &dsc_params_v1);
    log_info("Offset in struct for field ctrl_load_nodes is %d", (uint8_t *) &dsc_params_v1.ctrl_load_nodes - (uint8_t *) &dsc_params_v1);
    log_info("Offset in struct for field ctrl_drift_coeffs is %d", (uint8_t *) &dsc_params_v1.ctrl_drift_coeffs - (uint8_t *) &dsc_params_v1);
    log_info("Offset in struct for field ctrl_load_nodes_factory is %d", (uint8_t *) &dsc_params_v1.ctrl_load_nodes_factory - (uint8_t *) &dsc_params_v1);
    log_info("Offset in struct for field ctrl_drift_coeffs_factory is %d", (uint8_t *) &dsc_params_v1.ctrl_drift_coeffs_factory - (uint8_t *) &dsc_params_v1);
    log_info("Offset in struct for field coarse_equilibrium_factory is %d", (uint8_t *) &dsc_params_v1.coarse_equilibrium_factory - (uint8_t *) &dsc_params_v1);
    log_info("Offset in struct for field coarse_equilibrium is %d", (uint8_t *) &dsc_params_v1.coarse_equilibrium - (uint8_t *) &dsc_params_v1);
    log_info("Offset in struct for field calibration_date is %d", (uint8_t *) &dsc_params_v1.calibration_date - (uint8_t *) &dsc_params_v1);
    log_info("Offset in struct for field ctrl_nodes_length_factory is %d", (uint8_t *) &dsc_params_v1.ctrl_nodes_length_factory - (uint8_t *) &dsc_params_v1);
    log_info("Offset in struct for field ctrl_nodes_length is %d", (uint8_t *) &dsc_params_v1.ctrl_nodes_length - (uint8_t *) &dsc_params_v1);
    log_info("Offset in struct for field calibration_valid is %d", (uint8_t *) &dsc_params_v1.calibration_valid - (uint8_t *) &dsc_params_v1);
    log_info("Offset in struct for field estimated_equilibrium_ES is %d", (uint8_t *) &dsc_params_v1.estimated_equilibrium_ES - (uint8_t *) &dsc_params_v1);
    ret = write_file("/tmp/test_structures.txt", (char *) &dsc_params_v1, sizeof(struct disciplining_config_V_1));

    struct temperature_table_V_1 temp_table_v1;

    temp_table_v1.header = 'O';
    temp_table_v1.version = 1;
    for (i = 0; i < MEAN_TEMPERATURE_ARRAY_MAX; i++) {
        temp_table_v1.mean_fine_over_temperature[i] = i;
    }

    log_info("Size of struct temperature_table_V_1 IS %d", sizeof(struct temperature_table_V_1));

    log_info("Offset in struct for field header is %d", (uint8_t *) &temp_table_v1.header - (uint8_t *) &temp_table_v1);
    log_info("Offset in struct for field version is %d", (uint8_t *) &temp_table_v1.version - (uint8_t *) &temp_table_v1);
    log_info("Offset in struct for field mean_fine_over_temperature is %d", (uint8_t *) &temp_table_v1.mean_fine_over_temperature - (uint8_t *) &temp_table_v1);

    ret = write_file("/tmp/test_structures.txt", (char *) &temp_table_v1, sizeof(struct temperature_table_V_1));


    return 0;
}
