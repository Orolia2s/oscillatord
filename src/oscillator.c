#include <errno.h>

#include "oscillator.h"

int oscillator_set_dac(struct oscillator *oscillator, uint32_t value)
{
	unsigned dac_max;

	if (oscillator == NULL)
		return -EINVAL;

	dac_max = oscillator->class->dac_max;
	if (value > dac_max) {
		warn("dac value %u clipped to %d\n", value, dac_max);
		value = dac_max;
	}

	return oscillator->class->set_dac(oscillator, value);
}

int oscillator_get_dac(struct oscillator *oscillator, uint32_t *value)
{
	if (oscillator == NULL || value == NULL)
		return -EINVAL;

	return oscillator->class->get_dac(oscillator, value);
}

int oscillator_save(struct oscillator *oscillator)
{
	if (oscillator == NULL)
		return -EINVAL;

	return oscillator->class->save(oscillator);
}

int oscillator_get_temp(struct oscillator *oscillator, uint16_t *temp)
{
	if (oscillator == NULL || temp == NULL)
		return -EINVAL;

	return oscillator->class->get_temp(oscillator, temp);
}

