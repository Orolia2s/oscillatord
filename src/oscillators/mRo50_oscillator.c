#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <error.h>

#include "config.h"
#include "log.h"
#include "mRO50_ioctl.h"
#include "utils.h"

#include "../oscillator.h"
#include "../oscillator_factory.h"

#define FACTORY_NAME "mRO50"
#define MRO50_CMD_READ_TEMP 0x3e
#define MRO50_CMD_GET_DAC 0x41
#define MRO50_CMD_PROD_ID 0x50
#define MRO50_CMD_READ_FW_REV 0x51
#define MRO50_CMD_SET_DAC 0xA0
#define MRO50_SETPOINT_MIN 0
#define MRO50_SETPOINT_MAX 1000000
#define READ_MAX_TRY 400

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

typedef u_int32_t uint32_t;
typedef u_int32_t u32;

struct mRo50_oscillator {
	struct oscillator oscillator;
	int osc_fd;
};

static unsigned int mRo50_oscillator_index;


static void mRo50_oscillator_destroy(struct oscillator **oscillator)
{
	struct oscillator *o;
	struct mRo50_oscillator *r;

	if (oscillator == NULL || *oscillator == NULL)
		return;

	o = *oscillator;
	r = container_of(o, struct mRo50_oscillator, oscillator);
	if (r->osc_fd != 0) {
		close(r->osc_fd);
		log_info("Closed oscillator's serial port");
	}
	memset(o, 0, sizeof(*o));
	free(o);
	*oscillator = NULL;
}

static struct oscillator *mRo50_oscillator_new(struct config *config)
{
	struct mRo50_oscillator *mRo50;
	int fd;
	char * osc_device_name;
	struct oscillator *oscillator;

	mRo50 = calloc(1, sizeof(*mRo50));
	if (mRo50 == NULL)
		return NULL;
	oscillator = &mRo50->oscillator;
	mRo50->osc_fd = 0;

	osc_device_name = (char *) config_get(config, "mro50-device");
	if (osc_device_name == NULL) {
		log_error("mro50-device config key must be provided");
		goto error;
	}
	
	fd = open(osc_device_name, O_RDWR);
	if (fd < 0) {
		log_error("Could not open mRo50 device\n");
		goto error;
	}
	mRo50->osc_fd = fd;

	oscillator_factory_init(FACTORY_NAME, oscillator, FACTORY_NAME "-%d",
			mRo50_oscillator_index);
	mRo50_oscillator_index++;

	log_debug("instantiated " FACTORY_NAME " oscillator");

	return oscillator;
error:
	mRo50_oscillator_destroy(&oscillator);
	return NULL;
}

static int mRo50_oscillator_get_ctrl(struct oscillator *oscillator, struct oscillator_ctrl *ctrl)
{
	struct mRo50_oscillator *mRo50;
	uint32_t coarse, fine, ctrl_reg;
	int ret;

	mRo50 = container_of(oscillator, struct mRo50_oscillator, oscillator);

	ret = ioctl(mRo50->osc_fd, MRO50_READ_COARSE, &coarse); /* ioctl call to get partition size */
	if (ret != 0) {
		log_error("Fail reading Coarse Parameters, err %d", ret);
		return -1;
	}
	ctrl->coarse_ctrl = coarse;

	
	ret = ioctl(mRo50->osc_fd, MRO50_READ_FINE, &fine); /* ioctl call to get partition size */
	if (ret != 0) {
		log_error("Fail reading Fine Parameters, err %d", ret);
		return -1;
	}
	ctrl->fine_ctrl = fine;

	ret = ioctl(mRo50->osc_fd, MRO50_READ_CTRL, &ctrl_reg);
	if (ret != 0) {
		log_error("Fail reading ctrl registers, err %d", ret);
		return -1;
	}

	ctrl->lock = ctrl_reg & 0X2;
	return 0;
}

static int mRO50_oscillator_get_temp(struct oscillator *oscillator, double *temp)
{
	struct mRo50_oscillator *mRo50;
	int ret;
	uint32_t read_value;
	double temperature;
	mRo50 = container_of(oscillator, struct mRo50_oscillator, oscillator);

	ret = ioctl(mRo50->osc_fd, MRO50_READ_TEMP, &read_value);
	if (ret != 0) {
		log_error("Could not read temperature from mRO50 oscillator, err %d", ret);
		return -1;
	}
	temperature = compute_temp(read_value);
	if (temperature == DUMMY_TEMPERATURE_VALUE)
		return -1;
	*temp = temperature;
	return 0;
}

static int mRo50_oscillator_apply_output(struct oscillator *oscillator, struct od_output *output)
{
	struct mRo50_oscillator *mRo50;
	int command;
	int ret;


	mRo50 = container_of(oscillator, struct mRo50_oscillator, oscillator);
	
	if (output->action == ADJUST_FINE) {
		log_info("Fine adjustement to value %d requested", output->setpoint);
		command = MRO50_ADJUST_FINE;
	} else if (output->action == ADJUST_COARSE) {
		log_info("Coarse adjustment to value %d requested", output->setpoint);
		command = MRO50_ADJUST_COARSE;
	} else {
		log_error("Calling mRo50_oscillator_apply_output with action different from ADJUST_COARSE or ADJUST_FINE");
		log_error("Action is %d", output->action);
		return 0;
	}
	ret = ioctl(mRo50->osc_fd, command, &output->setpoint);
	if (ret != 0) {
		log_error("Could not prepare command request to adjust fine frequency, error %d", ret);
		return -1;
	}
	return 0;
}

static struct calibration_results * mRo50_oscillator_calibrate(struct oscillator *oscillator,
		struct phasemeter *phasemeter, struct gnss *gnss, struct calibration_parameters *calib_params,
		int phase_sign)
{
	struct mRo50_oscillator *mRo50;
	int ret;
	int64_t phase_error;

	mRo50 = container_of(oscillator, struct mRo50_oscillator, oscillator);

	struct calibration_results *results = malloc(sizeof(struct calibration_results));
	if (results == NULL) {
		log_error("Could not allocate memory to create calibration_results");
		return NULL;
	}

	results->length = calib_params->length;
	results->nb_calibration = calib_params->nb_calibration;
	results->measures = malloc(results->length * results->nb_calibration * sizeof(struct timespec));
	if (results->measures == NULL) {
		log_error("Could not allocate memory to create calibration measures");
		free(results);
		results = NULL;
		return NULL;
	}
	log_info("Starting measure for calibration");
	for (int i = 0; i < results->length; i ++) {
		if (!loop)
			goto clean_calibration;
		uint32_t ctrl_point = (uint32_t) calib_params->ctrl_points[i];
		log_info("Applying fine adjustment of %d", ctrl_point);
		ret = ioctl(mRo50->osc_fd, MRO50_ADJUST_FINE, &ctrl_point);
		if ((ret =! sizeof(ctrl_point))) {
			free(results->measures);
			log_error("Could not write to mRO50");
			results->measures = NULL;
			free(results);
			results = NULL;
			return NULL;
		}
		sleep(SETTLING_TIME);

		struct oscillator_ctrl ctrl;
		ret = mRo50_oscillator_get_ctrl(oscillator, &ctrl);
		if (ctrl.fine_ctrl != ctrl_point) {
			log_info("ctrl measured is %d and ctrl point is %d", ctrl.fine_ctrl, ctrl_point);
			log_error("CTRL POINTS HAS NOT BEEN SET !");
		}

		log_info("Starting phase error measures %d/%d", i+1, results->length);
		for (int j = 0; j < results->nb_calibration; j++) {
			if (!loop)
				goto clean_calibration;
			int phasemeter_status = get_phase_error(phasemeter, &phase_error);
			if (phasemeter_status != PHASEMETER_BOTH_TIMESTAMPS) {
				log_error("Could not get phase error during calibration, aborting");
				free(results->measures);
				results->measures = NULL;
				free(results);
				results = NULL;
				return NULL;
			}
			/* Get qErr in ps*/
			int32_t qErr;
			if (gnss_get_epoch_data(gnss, NULL, NULL, &qErr) != 0) {
				log_error("Could not get gnss data");
				free(results->measures);
				results->measures = NULL;
				free(results);
				results = NULL;
				return NULL;
			}
			
			*(results->measures + i * results->nb_calibration + j) = phase_error + (float) qErr / 1000;
			log_debug("ctrl_point %d measure[%d]: phase error = %lld, qErr = %d, result = %f",
				ctrl_point, j, phase_error, qErr, phase_error + (float) qErr / 1000);
			sleep(1);
		}
	}

	return results;

clean_calibration:
	free(results->measures);
	results->measures = NULL;
	free(results);
	results = NULL;
	return NULL;
}

static int mRo50_oscillator_get_disciplining_parameters(struct oscillator *oscillator, struct disciplining_parameters *disciplining_parameters)
{
	struct mRo50_oscillator *mRo50;
	int ret;
	mRo50 = container_of(oscillator, struct mRo50_oscillator, oscillator);
	uint8_t buf[256];
	ret = ioctl(mRo50->osc_fd, MRO50_READ_EEPROM_BLOB, buf);
	if (ret != 0) {
		log_error("Fail reading disciplining parameters, err %d", ret);
		return -1;
	}
	memcpy(disciplining_parameters, buf, sizeof(struct disciplining_parameters));
	return 0;
}

static int mRo50_oscillator_update_disciplining_parameters(struct oscillator *oscillator, struct disciplining_parameters *disciplining_parameters)
{
	struct mRo50_oscillator *mRo50;
	int ret;
	mRo50 = container_of(oscillator, struct mRo50_oscillator, oscillator);
	uint8_t buf[256];
	memcpy(buf, disciplining_parameters, sizeof(struct disciplining_parameters));
	ret = ioctl(mRo50->osc_fd, MRO50_WRITE_EEPROM_BLOB, buf);
	if (ret != 0) {
		log_error("Fail updating disciplining parameters, err %d", ret);
		return -1;
	}
	return 0;
}

static const struct oscillator_factory mRo50_oscillator_factory = {
	.class = {
			.name = FACTORY_NAME,
			.get_ctrl = mRo50_oscillator_get_ctrl,
			.save = NULL,
			.get_temp = mRO50_oscillator_get_temp,
			.apply_output = mRo50_oscillator_apply_output,
			.calibrate = mRo50_oscillator_calibrate,
			.get_disciplining_parameters = mRo50_oscillator_get_disciplining_parameters,
			.update_disciplining_parameters = mRo50_oscillator_update_disciplining_parameters,
			.dac_min = MRO50_SETPOINT_MIN,
			.dac_max = MRO50_SETPOINT_MAX,
	},
	.new = mRo50_oscillator_new,
	.destroy = mRo50_oscillator_destroy,
};

static void __attribute__((constructor)) mRo50_oscillator_constructor(void)
{
	int ret;

	ret = oscillator_factory_register(&mRo50_oscillator_factory);
	if (ret < 0)
		log_error("oscillator_factory_register", ret);
}
