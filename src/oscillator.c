#include <errno.h>

#include "oscillator.h"

int oscillator_set_dac(struct oscillator *oscillator, unsigned value)
{
	if (oscillator == NULL)
		return -EINVAL;

	return oscillator->set_dac(oscillator, value);
}

int oscillator_get_dac(struct oscillator *oscillator, unsigned *value)
{
	if (oscillator == NULL || value == NULL)
		return -EINVAL;

	return oscillator->get_dac(oscillator, value);
}

int oscillator_save(struct oscillator *oscillator)
{
	if (oscillator == NULL)
		return -EINVAL;

	return oscillator->save(oscillator);
}

int oscillator_get_temp(struct oscillator *oscillator, uint16_t *temp)
{
	if (oscillator == NULL || temp == NULL)
		return -EINVAL;

	return oscillator->get_temp(oscillator, temp);
}

