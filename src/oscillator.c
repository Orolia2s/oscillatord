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

int oscillator_get_ctrl(struct oscillator *oscillator, struct oscillator_ctrl *ctrl)
{
	if (oscillator == NULL || ctrl == NULL)
		return -EINVAL;
	if (oscillator->class->get_ctrl == NULL)
		return -ENOSYS;

	return oscillator->class->get_ctrl(oscillator, ctrl);
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

int oscillator_apply_output(struct oscillator *oscillator, struct od_output *output) {
	if (oscillator == NULL || output == NULL)
		return -EINVAL;
	if (oscillator->class->apply_output == NULL)
		return -ENOSYS;

	return oscillator->class->apply_output(oscillator, output);
}

