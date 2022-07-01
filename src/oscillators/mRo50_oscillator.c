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
#include <termios.h>
#include <poll.h>

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

#define CMD_READ_COARSE "FD\r"
#define CMD_READ_FINE   "MON_tpcb PIL_cfield C\r"
#define CMD_READ_STATUS "MONITOR1\r"

#define STATUS_ANSWER_SIZE 62
#define STATUS_EP_TEMPERATURE_INDEX 52
#define STATUS_CLOCK_LOCKED_INDEX 56
#define STATUS_CLOCK_LOCKED_BIT 2
#define STATUS_ANSWER_FIELD_SIZE 4

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

typedef u_int32_t uint32_t;
typedef u_int32_t u32;

struct mRo50_oscillator {
	struct oscillator oscillator;
	int serial_fd;
	int osc_fd;
};

struct mRo50_attributes {
	double EP_temperature;	// Main electronique board temperature (DegC)
	uint8_t locked:1;		//Locked
};

// From datasheet we assume answers cannot be larger than 60+2+2+2 characters
static char answer_str[66] = {0};
const size_t mro_answer_len = 66;

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
		log_info("Closed oscillator's device");
	}
	if (r->serial_fd != 0) {
		close(r->serial_fd);
		log_info("Closed oscillator's serial port");
	}
	memset(o, 0, sizeof(*o));
	free(o);
	*oscillator = NULL;
}

static int set_serial_attributes(int fd)
{
	struct termios tty;
	int err = tcgetattr(fd, &tty);
	if (err != 0){
		log_error("error from tcgetattr: %d\n", errno);
		return -1;
	}

	cfsetospeed(&tty, B9600);
	cfsetispeed(&tty, B9600);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
	// disable IGNBRK for mismatched speed tests; otherwise receive break
	// as \000 chars
	tty.c_iflag &= ~IGNBRK;			// disable break processing
	tty.c_lflag = 0;			// no signaling chars, no echo,
						// no canonical processing
	tty.c_oflag = 0;			// no remapping, no delays

	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

	tty.c_cflag |= (CLOCAL | CREAD);	// ignore modem controls,
						// enable reading
	tty.c_cflag &= ~(PARENB | PARODD);	// shut off parity
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	if (tcsetattr(fd, TCSANOW, &tty) != 0){
		log_error("error from tcsetattr\n");
		return -1;
	}

	return 0;
}

static struct oscillator *mRo50_oscillator_new(struct config *config)
{
	struct mRo50_oscillator *mRo50;
	int fd, ret;
	int serial_fd;
	char * osc_device_name;
	char * osc_serial_name;
	struct oscillator *oscillator;

	mRo50 = calloc(1, sizeof(*mRo50));
	if (mRo50 == NULL)
		return NULL;
	oscillator = &mRo50->oscillator;
	mRo50->osc_fd = 0;

	/* Get device path for mRo50 device */
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

	/* Activate serial in order to use mro50-serial device */
	uint32_t serial_activate = 1;
	ret = ioctl(fd, MRO50_BOARD_CONFIG_WRITE, &serial_activate);
	if (ret != 0) {
		log_error("Could not activate mro50 serial");
		goto error;
	}

	/* Get device path for mRo50 serial */
	osc_serial_name = (char *) config_get(config, "mro50-serial");
	if (osc_serial_name == NULL) {
		log_error("mro50-serial config key must be provided");
		goto error;
	}
	log_info("mRO50 serial device: %s", osc_serial_name);
	serial_fd = open(osc_serial_name, O_RDWR|O_NONBLOCK);
	if (serial_fd < 0) {
		log_error("Could not open mRo50 device\n");
		goto error;
	}
	mRo50->serial_fd = serial_fd;
	if (set_serial_attributes(serial_fd) != 0)
		goto error;

	oscillator_factory_init(FACTORY_NAME, oscillator, FACTORY_NAME "-%d",
			mRo50_oscillator_index);
	mRo50_oscillator_index++;

	log_debug("instantiated " FACTORY_NAME " oscillator");

	return oscillator;
error:
	close(fd);
	close(serial_fd);
	mRo50_oscillator_destroy(&oscillator);
	return NULL;
}

static int mRo50_oscillator_cmd(struct mRo50_oscillator *mRo50, const char *cmd, int cmd_len)
{
	struct pollfd pfd = {};
	int err, rbytes = 0;
	if (write(mRo50->serial_fd, cmd, cmd_len) != cmd_len) {
		log_error("oscillator_get_attributes send command error: %d (%s)", errno, strerror(errno));
		return -1;
	}
	pfd.fd = mRo50->serial_fd;
	pfd.events = POLLIN;
	while (1) {
		err = poll(&pfd, 1, 50);
		if (err == -1) {
			log_error("oscillator_get_attributes poll error: %d (%s)", errno, strerror(errno));
			return -1;
		}
		// poll call timed out - check the answer
		if (!err)
			break;
		err = read(mRo50->serial_fd, &answer_str[rbytes], mro_answer_len - rbytes);
		if (err < 0) {
			log_error("oscillator_get_attributes rbyteserror: %d (%s)", errno, strerror(errno));
			return -1;
		}
		rbytes += err;
	}
	if (rbytes == 0) {
		log_error("oscillator_get_attributes didn't get answer, zero length");
		return -1;
	}
	// Verify that first caracter of the answer is not equal to '?'
	if (answer_str[0] == '?') {
		// answer format doesn't fit protocol
		log_error("oscillator_get_attributes answer protocol error: %s", answer_str);
		memset(answer_str, 0, rbytes);
		return -1;
	}
	return rbytes;
}

static int mRo50_oscillatord_get_attributes(struct oscillator *oscillator, struct mRo50_attributes *a)
{
	struct mRo50_oscillator *mRo50;
	mRo50 = container_of(oscillator, struct mRo50_oscillator, oscillator);
	char EP_temperature[4];
	uint32_t read_value;
	int err;

	err = mRo50_oscillator_cmd(mRo50, CMD_READ_STATUS, sizeof(CMD_READ_STATUS) - 1);
	if (err == STATUS_ANSWER_SIZE) {
		/* Parse mRo50 EP temperature */
		strncpy(EP_temperature, &answer_str[STATUS_EP_TEMPERATURE_INDEX], STATUS_ANSWER_FIELD_SIZE);
		read_value = strtoul(EP_temperature, NULL, 16);
		double temperature = compute_temp(read_value);
		if (temperature == DUMMY_TEMPERATURE_VALUE)
			return -1;
		a->EP_temperature = temperature;

		/* Parse mRO50 clock lock flag */
		uint8_t lock = answer_str[STATUS_CLOCK_LOCKED_INDEX] & (1 << STATUS_CLOCK_LOCKED_BIT);
		a->locked = lock >> STATUS_CLOCK_LOCKED_BIT;
		memset(answer_str, 0, STATUS_ANSWER_SIZE);
	} else {
		log_error("Fail reading attributes, err %d, errno %d", err, errno);
		return -1;
	}
	return 0;
}

static int mRo50_oscillator_get_ctrl(struct oscillator *oscillator, struct oscillator_ctrl *ctrl)
{
	struct mRo50_attributes mRo50_attributes;
	struct mRo50_oscillator *mRo50;
	uint32_t coarse, fine;
	int ret;

	mRo50 = container_of(oscillator, struct mRo50_oscillator, oscillator);

	ret = mRo50_oscillator_cmd(mRo50, CMD_READ_COARSE, sizeof(CMD_READ_COARSE) - 1);
	if (ret > 0) {
		int res = sscanf(answer_str, "%x\r\n", &coarse);
		memset(answer_str, 0, ret);
		if (res > 0) {
			ctrl->coarse_ctrl = coarse;
		} else {
			log_error("Could not parse coarse parameter");
			return -1;
		}
	} else {
		log_error("Fail reading Coarse Parameters, err %d, errno %d", ret, errno);
		return -1;
	}

	ret = mRo50_oscillator_cmd(mRo50, CMD_READ_FINE, sizeof(CMD_READ_FINE) - 1);
	if (ret > 0) {
		int res = sscanf(answer_str, "%x\r\n", &fine);
		memset(answer_str, 0, ret);
		if (res > 0) {
			ctrl->fine_ctrl = fine;
		} else {
			log_error("Could not parse fine parameter");
			return -1;
		}
	} else {
		log_error("Fail reading Fine Parameters, err %d, errno %d", ret, errno);
		return -1;
	}

	ret = mRo50_oscillatord_get_attributes(oscillator, &mRo50_attributes);
	if (ret != 0) {
		log_error("Fail reading mRo attributes, err %d", ret);
		return -1;
	}
	ctrl->lock = mRo50_attributes.locked;

	return 0;
}

static int mRO50_oscillator_get_temp(struct oscillator *oscillator, double *temp)
{
	struct mRo50_attributes mRo50_attributes;
	int ret;
	ret = mRo50_oscillatord_get_attributes(oscillator, &mRo50_attributes);
	if (ret != 0) {
		log_error("Fail reading mRo attributes, err %d", ret);
		return -1;
	}
	*temp = mRo50_attributes.EP_temperature;
	return 0;
}

static int mRo50_oscillator_apply_output(struct oscillator *oscillator, struct od_output *output)
{
	struct mRo50_oscillator *mRo50;
	char command[128];
	int ret;

	memset(command, '\0', 128);
	mRo50 = container_of(oscillator, struct mRo50_oscillator, oscillator);
	
	if (output->action == ADJUST_FINE) {
		log_trace("mRo50_oscillator_apply_output: Fine adjustement to value %lu requested", output->setpoint);
		sprintf(command, "MON_tpcb PIL_cfield C %04X\r", output->setpoint);
	} else if (output->action == ADJUST_COARSE) {
		log_trace("mRo50_oscillator_apply_output: Coarse adjustment to value %lu requested", output->setpoint);
		sprintf(command, "FD %08X\r", output->setpoint);
	} else {
		log_error("Calling mRo50_oscillator_apply_output with action different from ADJUST_COARSE or ADJUST_FINE");
		log_error("Action is %d", output->action);
		return 0;
	}
	ret = mRo50_oscillator_cmd(mRo50, command, strlen(command));
	if (ret != 2) {
		log_error("Could not prepare command request to adjust fine frequency, error %d, errno %d", ret, errno);
		return -1;
	}
	memset(answer_str, 0, mro_answer_len);
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
	uint8_t buf[512];
	ret = ioctl(mRo50->osc_fd, MRO50_READ_EXTENDED_EEPROM_BLOB, buf);
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
	uint8_t buf[512];
	memcpy(buf, disciplining_parameters, sizeof(struct disciplining_parameters));
	ret = ioctl(mRo50->osc_fd, MRO50_WRITE_EXTENDED_EEPROM_BLOB, buf);
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
