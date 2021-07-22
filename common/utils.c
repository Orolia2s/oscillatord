#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "utils.h"
#include "log.h"

void file_cleanup(FILE **f)
{
	if (f == NULL || *f == NULL)
		return;

	fclose(*f);
	*f = NULL;
}

void string_cleanup(char **s)
{
	if (s == NULL || *s == NULL)
		return;

	free(*s);
	*s = NULL;
}

void fd_cleanup(int *fd)
{
	if (fd == NULL)
		return;

	close(*fd);
	*fd = -1;
}

// Formula to compute mRO50 temperature
double compute_temp(uint32_t reg)
{
	double x = (double) reg / 4095.0;
	if (x == 1.0) {
		log_warn("Cannot compute temperature\n");
		// Return absurd value so that user know it is not possible
		return DUMMY_TEMPERATURE_VALUE;
	}
	double resistance = 47000.0 * x / (1.0 - x);
	double temperature = (
		4100.0 * 298.15
		/ (298.15 * log(pow(10, -5) * resistance) + 4100.0)
		) - 273.14;
	return temperature;
}
