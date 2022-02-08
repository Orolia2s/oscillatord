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

	oscillator->dac_max = dac_max;

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

int oscillator_get_temp(struct oscillator *oscillator, double *temp)
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

struct calibration_results * oscillator_calibrate(
	struct oscillator *oscillator,
	struct phasemeter *phasemeter,
	struct calibration_parameters * calib_params,
	int phase_sign)
{
	if (oscillator == NULL || calib_params == NULL) {
		log_error("oscillator_calibrate: one input is NULL");
		return NULL;
	}
	if (oscillator->class->calibrate == NULL) {
		log_error("oscillator_calibrate: calibrate function is null in class !");
		return NULL;
	}

	return oscillator->class->calibrate(oscillator, phasemeter, calib_params, phase_sign);
}

int oscillator_get_disciplining_parameters(struct oscillator *oscillator, struct disciplining_parameters *disciplining_parameters)
{
	if(oscillator == NULL || disciplining_parameters == NULL)
		return -EINVAL;
	if(oscillator->class->get_disciplining_parameters == NULL)
		return -ENOSYS;
	return oscillator->class->get_disciplining_parameters(oscillator, disciplining_parameters);
}
