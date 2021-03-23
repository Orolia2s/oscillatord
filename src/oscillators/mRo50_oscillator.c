#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <error.h>

#include "config.h"
#include "log.h"
#include "utils.h"

#include "../oscillator.h"
#include "../oscillator_factory.h"

#define FACTORY_NAME "mRO50"
#define MRO50_CMD_READ_TEMP 0x3e
#define MRO50_CMD_GET_DAC 0x41
#define MRO50_CMD_PROD_ID 0x50
#define MRO50_CMD_READ_FW_REV 0x51
#define MRO50_CMD_SET_DAC 0xA0
#define MRO50_CMD_SAVE 0xc2
#define MRO50_SETPOINT_MIN 0
#define MRO50_SETPOINT_MAX 1000000
#define READ_MAX_TRY 400

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

struct mRo50_oscillator {
	struct oscillator oscillator;
	int serial_fd;
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
	if (r->serial_fd != 0) {
		close(r->serial_fd);
		info("Closed oscillator's serial port\n");
	}
	memset(o, 0, sizeof(*o));
	free(o);
	*oscillator = NULL;
}

static struct oscillator *mRo50_oscillator_new(struct config *config)
{
	struct mRo50_oscillator *mRo50;
	int serial_port;
	char * serial_port_name;
	struct oscillator *oscillator;

	mRo50 = calloc(1, sizeof(*mRo50));
	if (mRo50 == NULL)
		return NULL;
	oscillator = &mRo50->oscillator;
	mRo50->serial_fd = 0;

	serial_port_name = (char *) config_get(config, "mro50-device");
	if (serial_port_name == NULL) {
		err("mro50-device config key must be provided\n");
		goto error;
	}
	
	serial_port = open(serial_port_name, O_RDWR);
	if (serial_port < 0) {
		err("Could not open mRo50 serial\n");
		goto error;
	}
	mRo50->serial_fd = serial_port;

	oscillator_factory_init(FACTORY_NAME, oscillator, FACTORY_NAME "-%d",
			mRo50_oscillator_index);
	mRo50_oscillator_index++;

	debug("instantiated " FACTORY_NAME " oscillator \n");

	return oscillator;
error:
	mRo50_oscillator_destroy(&oscillator);
	return NULL;
}

static bool get_value_from_serial(int fd, char * buffer, int buffer_length, char * command, int command_length)
{
	int ret;
		ret = write(fd, command, command_length);
	if (ret == -1) {
		err("Could not write to serial");
		return false;
	} else {
		debug("Wrote %d characters \n", ret);
	}

	int i = 0;
	do {
		usleep(200);
		ret = read(fd, buffer, buffer_length);
		i++;
	} while (i < READ_MAX_TRY && (ret <= 1 || !strncmp(buffer, "?", 1)));

	return i < READ_MAX_TRY ? true : false;
}

static int mRo50_oscillator_get_ctrl(struct oscillator *oscillator, struct oscillator_ctrl *ctrl)
{
	struct mRo50_oscillator *mRo50;
	char *ptr;
	char read_buf[32];
	bool read_ok;

	char coarse_cmd[4] = "FD\r\n";
	char fine_cmd[21] = "MON_tpcbPIL_cfieldC\r\n";

	memset(&read_buf, '\0', sizeof(read_buf));
	debug("Reading Coarse parameter\n");
	mRo50 = container_of(oscillator, struct mRo50_oscillator, oscillator);

	int i = 0;
	do {
		read_ok = get_value_from_serial(mRo50->serial_fd, read_buf, sizeof(read_buf), coarse_cmd, sizeof(coarse_cmd));
		i++;
	} while (i < 30 && !read_ok);

	unsigned long coarse = strtoul(read_buf, &ptr, 16);
	debug("Coarse is %lu\n", coarse);
	ctrl->coarse_ctrl = (uint32_t) coarse;

	
	debug("Reading Fine parameter\n");
	memset(&read_buf, '\0', sizeof(read_buf));
	do {
		read_ok = get_value_from_serial(mRo50->serial_fd, read_buf, sizeof(read_buf), fine_cmd, sizeof(fine_cmd));
		i++;
	} while (i < 30 && !read_ok);
	
	unsigned long fine = strtoul(read_buf, &ptr, 16);
	debug("Fine is %lu\n", fine);
	ctrl->fine_ctrl = (uint32_t) fine;

	return 0;
}

static int mRo50_oscillator_apply_output(struct oscillator *oscillator, struct od_output *output)
{
	struct mRo50_oscillator *mRo50;
	int ret;
	char command[32];

	mRo50 = container_of(oscillator, struct mRo50_oscillator, oscillator);
	if (output->action == ADJUST_FINE)
	{
		ret = snprintf(command, sizeof(command), "MON_tpcbPIL_cfieldC %.4x\r\n", output->setpoint);
		if (ret < 0) {
			err("Could not prepare command request to adjust fine frequency, error %d\n", ret);
			return -1;
		}
	} else if (output->action == ADJUST_COARSE) {
		ret = snprintf(command, sizeof(command), "FD %.8x\r\n", output->setpoint);
		if (ret < 0) {
			err("Could not prepare command request to adjust fine frequency, error %d\n", ret);
			return -1;
		}
	} else {
		err("Calling mRo50_oscillator_apply_output with action different from ADJUST_COARSE or ADJUST_FINE");
		return -1;
	}

	debug("Command sent to mRO is %s\n", command);
	ret = write(mRo50->serial_fd, command, sizeof(command));
	if (ret == -1) {
		err("Could not write to serial");
		return -1;
	} else {
		debug("Wrote %d characters \n", ret);
	}

	return 0;
}

static struct calibration_results * mRo50_oscillator_calibrate(struct oscillator *oscillator,
		struct calibration_parameters *calib_params, int phase_descriptor, int phase_sign)
{
	struct mRo50_oscillator *mRo50;
	int ret;
	char command[32];
	int32_t phase_error;

	mRo50 = container_of(oscillator, struct mRo50_oscillator, oscillator);

	struct calibration_results *results = malloc(sizeof(struct calibration_results));
	if (results == NULL) {
		err("Could not allocate memory to create calibration_results\n");
		return NULL;
	}

	results->length = calib_params->length;
	results->nb_calibration = calib_params->nb_calibration;
	results->measures = malloc(results->length * results->nb_calibration * sizeof(struct timespec));
	debug("Phase sign is %d\n", phase_sign);
	if (results->measures == NULL) {
		err("Could not allocate memory to create calibration measures\n");
		free(results);
		results = NULL;
		return NULL;
	}
	info("Starting measure for calibration\n");
	for (int i = 0; i < results->length; i ++) {
		uint16_t ctrl_point = calib_params->ctrl_points[i];
		info("Applying fine adjustment of %d\n", ctrl_point);
		ret = snprintf(command, sizeof(command), "MON_tpcbPIL_cfieldC %.4x\r\n", ctrl_point);
		if (ret < 0) {
			err("Could not prepare command request to adjust fine frequency, error %d\n", ret);
			free(results->measures);
			results->measures = NULL;
			free(results);
			results = NULL;
			return NULL;
		}
		ret = write(mRo50->serial_fd, command, sizeof(command));
		if (ret == -1) {
			err("Could not write to serial\n");
			free(results->measures);
			results->measures = NULL;
			free(results);
			results = NULL;
			return NULL;
		} else {
			debug("Wrote %d characters \n", ret);
		}
		sleep(calib_params->settling_time);

		debug("Check control point is correctly set \n");
		struct oscillator_ctrl ctrl;
		ret = mRo50_oscillator_get_ctrl(oscillator, &ctrl);
		if (ctrl.fine_ctrl != ctrl_point) {
			debug("ctrl measured is %d and ctrl point is %d\n", ctrl.fine_ctrl, ctrl_point);
			err("CTRL POINTS HAS NOT BEEN SET !\n");
		}

		info("Starting phase error measures %d/%d\n", i+1, results->length);
		for (int j = 0; j < results->nb_calibration; j++) {
			ret = read(phase_descriptor, &phase_error, sizeof(phase_error));
			if (ret == -1) {
				info("Error reading phasemeter\n");
				error(EXIT_FAILURE, errno, "read phase descriptor");
			}

			*(results->measures + i * results->nb_calibration + j) = (struct timespec) {
				.tv_sec = phase_sign * phase_error / NS_IN_SECOND,
				.tv_nsec = phase_sign * phase_error % NS_IN_SECOND,
			};
			debug("%09ldns \n", (results->measures + i * results->nb_calibration + j)->tv_nsec);
			sleep(1);
		}
	}

	return results;
}

static const struct oscillator_factory mRo50_oscillator_factory = {
	.class = {
			.name = FACTORY_NAME,
			.get_ctrl = mRo50_oscillator_get_ctrl,
			.save = NULL,
			.get_temp = NULL,
			.apply_output = mRo50_oscillator_apply_output,
			.calibrate = mRo50_oscillator_calibrate,
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
		perr("oscillator_factory_register", ret);
}
