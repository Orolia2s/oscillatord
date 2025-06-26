#pragma once

#include "ppsthread.h"

#include <log.h>

void               ppsthread_log(volatile struct pps_thread_t* pps_thread, int level, const char* fmt, ...);

static inline void print_temperature_table(uint16_t mean_fine_over_temperature[MEAN_TEMPERATURE_ARRAY_MAX], int level)
{
	bool table_empty = true;
	/* Print temperature table */
	log_log(level, __FILE__, __LINE__, "Temperature compensation table:");
	for (int i = 0; i < MEAN_TEMPERATURE_ARRAY_MAX; i++)
	{
		if (mean_fine_over_temperature[i] > 0 && mean_fine_over_temperature[i] <= 48000)
		{
			table_empty = false;
			log_log(level,
			        __FILE__,
			        __LINE__,
			        "Read mean value of %.2f in temperature range [%.2f, %.2f[",
			        (float)mean_fine_over_temperature[i] / 10.0,
			        (i + STEPS_BY_DEGREE * MIN_TEMPERATURE) / STEPS_BY_DEGREE,
			        (i + 1 + STEPS_BY_DEGREE * MIN_TEMPERATURE) / STEPS_BY_DEGREE);
		}
	}
	if (table_empty)
		log_log(level, __FILE__, __LINE__, "Temperature table is empty (filled with 0)");
}

static inline void print_disciplining_config(struct disciplining_config* dsc_config, int level)
{
	log_log(level, __FILE__, __LINE__, "Disciplining config:");
	log_log(level, __FILE__, __LINE__, "ctrl_nodes_length = %d", dsc_config->ctrl_nodes_length);
	log_log(level, __FILE__, __LINE__, "ctrl_load_nodes[] =");
	if (dsc_config->ctrl_nodes_length > 0 && dsc_config->ctrl_nodes_length <= CALIBRATION_POINTS_MAX)
		for (int i = 0; i < dsc_config->ctrl_nodes_length; i++)
			log_log(level, __FILE__, __LINE__, " %f", dsc_config->ctrl_load_nodes[i]);

	log_log(level, __FILE__, __LINE__, "ctrl_drift_coeffs[] =");
	if (dsc_config->ctrl_nodes_length > 0 && dsc_config->ctrl_nodes_length <= CALIBRATION_POINTS_MAX)
		for (int i = 0; i < dsc_config->ctrl_nodes_length; i++)
			log_log(level, __FILE__, __LINE__, " %f", dsc_config->ctrl_drift_coeffs[i]);
	char       buff[20];
	struct tm* timeinfo;
	timeinfo = localtime(&dsc_config->calibration_date);
	strftime(buff, sizeof(buff), "%b %d %Y", timeinfo);
	log_log(level, __FILE__, __LINE__, "Date of calibration: %s", buff);

	log_log(level, __FILE__, __LINE__, "coarse_equilibrium = %d", dsc_config->coarse_equilibrium);
	log_log(level, __FILE__, __LINE__, "calibration_valid = %d", dsc_config->calibration_valid);

	log_log(level, __FILE__, __LINE__, "ctrl_nodes_length_factory = %d", dsc_config->ctrl_nodes_length_factory);
	log_log(level, __FILE__, __LINE__, "ctrl_load_nodes_factory[] =");
	if (dsc_config->ctrl_nodes_length_factory > 0 && dsc_config->ctrl_nodes_length_factory <= CALIBRATION_POINTS_MAX)
		for (int i = 0; i < dsc_config->ctrl_nodes_length_factory; i++)
			log_log(level, __FILE__, __LINE__, " %f", dsc_config->ctrl_load_nodes_factory[i]);

	log_log(level, __FILE__, __LINE__, "ctrl_drift_coeffs_factory[] =");
	if (dsc_config->ctrl_nodes_length_factory > 0 && dsc_config->ctrl_nodes_length_factory <= CALIBRATION_POINTS_MAX)
		for (int i = 0; i < dsc_config->ctrl_nodes_length_factory; i++)
			log_log(level, __FILE__, __LINE__, " %f", dsc_config->ctrl_drift_coeffs_factory[i]);

	log_log(level, __FILE__, __LINE__, "coarse_equilibrium_factory = %d", dsc_config->coarse_equilibrium_factory);
	log_log(level, __FILE__, __LINE__, "estimated_equilibrium_ES = %d", dsc_config->estimated_equilibrium_ES);
}

static inline void print_disciplining_parameters(struct disciplining_parameters* dsc_params, int level)
{
	print_disciplining_config(&dsc_params->dsc_config, level);
	print_temperature_table(dsc_params->temp_table.mean_fine_over_temperature, level);
}

void pps_log_set_level(int level);
