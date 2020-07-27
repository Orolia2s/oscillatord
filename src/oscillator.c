#include <errno.h>

#include "log.h"
#include "oscillator.h"

int oscillator_set_dac_min(struct oscillator *oscillator, uint32_t dac_min)
{
	if (oscillator == NULL)
		return -EINVAL;

	oscillator->dac_min = dac_min;

	return 0;
}

int oscillator_set_dac_max(struct oscillator *oscillator, uint32_t dac_max)
{
	if (oscillator == NULL)
		return -EINVAL;

	oscillator->dac_min = dac_max;

	return 0;
}

int oscillator_set_dac(struct oscillator *o, uint32_t value)
{
	uint32_t dac_max;
	uint32_t dac_min;

	if (o == NULL)
		return -EINVAL;

	dac_max = o->dac_max == 0 ? o->class->dac_max : o->dac_max;
	if (value > dac_max) {
		warn("dac value %u too high, clipped to %d\n", value, dac_max);
		value = dac_max;
	}
	dac_min = o->dac_min == UINT32_MAX ? o->class->dac_min : o->dac_min;
	if (value > dac_max) {
		warn("dac value %u too low, clipped to %d\n", value, dac_min);
		value = dac_min;
	}

	return o->class->set_dac(o, value);
}

int oscillator_get_dac(struct oscillator *oscillator, uint32_t *value)
{
	if (oscillator == NULL || value == NULL)
		return -EINVAL;
	if (oscillator->class->get_dac == NULL)
		return -ENOSYS;

	return oscillator->class->get_dac(oscillator, value);
}

int oscillator_save(struct oscillator *oscillator)
{
	if (oscillator == NULL)
		return -EINVAL;
	if (oscillator->class->save == NULL)
		return -ENOSYS;

	return oscillator->class->save(oscillator);
}

int oscillator_get_temp(struct oscillator *oscillator, uint16_t *temp)
{
	if (oscillator == NULL || temp == NULL)
		return -EINVAL;
	if (oscillator->class->get_temp == NULL)
		return -ENOSYS;

	return oscillator->class->get_temp(oscillator, temp);
}

