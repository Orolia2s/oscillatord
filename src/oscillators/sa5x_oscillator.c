#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <poll.h>

#include "config.h"
#include "log.h"
#include "utils.h"

#include "../oscillator.h"
#include "../oscillator_factory.h"

#define FACTORY_NAME "sa5x"
#define MAX_VER_LENGTH 20
#define MAX_SERIALNUM_LENGTH 11

#define ATTR_FW_SERIAL  1
#define ATTR_CTRL       2
#define ATTR_STATUS     4

#define CMD_SWVER                     "\\{swrev?}"
#define CMD_SERIAL                    "{serial?}"
#define CMD_GET_LOCKED                "{get,Locked}"
#define CMD_GET_PHASE                 "{get,Phase}"
#define CMD_GET_LASTCORRECTION        "{get,LastCorrection}"
#define CMD_GET_TEMPERATURE           "{get,Temperature}"
#define CMD_GET_ANALOG_TUNING         "{get,AnalogTuning}"
#define CMD_GET_DIGITAL_TUNING        "{get,DigitalTuning}"
#define CMD_GET_ANALOG_TUNING_ENABLED "{get,AnalogTuningEnabled}"

struct sa5x_oscillator {
	struct oscillator oscillator;
	int	osc_fd;
	char   version[20];      // SW Rev
	char   serial[12];       // SerialNumber
};

struct sa5x_attributes {

	uint32_t alarms;		// Alarams bits

	int32_t  phaseoffset;		// Most recent phase offset
	int32_t  lastcorrection;	// Most recent freq correction
					// due to disciplining
	int32_t  temperature;		// Temperature (mDegC)
	int32_t  digitaltuning;		// Digital Tuning (0.01Hz)
	int32_t  analogtuning;		// Analog Tuning (mv)
	// General Status bits
	uint8_t  ppsindetected:1;	// PpsInDetected
	uint8_t  locked:1;		//Locked
	uint8_t  disciplinelocked:1;	//DisciplineLocked
	uint8_t  analogtuningon:1;	// Analog Tuning On/Off
	uint8_t  hwinforead:1;		// Initial info rbytesflag
	uint8_t  lockprogress;		//Percent of progress to Locked state
};

// Datasheet states that answer will be no more than 4096+2+1+2 characters
static char answer_str[4101] = {0};
const size_t answer_len = 4101;

static unsigned int sa5x_oscillator_index;

static void sa5x_oscillator_destroy(struct oscillator **oscillator)
{
	struct oscillator *o;
	struct sa5x_oscillator *r;

	if (oscillator == NULL || *oscillator == NULL)
		return;

	o = *oscillator;
	r = container_of(o, struct sa5x_oscillator, oscillator);
	if (r->osc_fd != -1) {
		close(r->osc_fd);
		log_info("Closed oscillator's serial port");
	}
	memset(o, 0, sizeof(*o));
	free(o);
	*oscillator = NULL;
}

static int set_serial_attributes(int fd)
{
	struct termios tty;

	if (tcgetattr(fd, &tty) != 0){
		log_error("error from tcgetattr\n");
		return -1;
	}

	cfsetospeed(&tty, B57600);
	cfsetispeed(&tty, B57600);

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

static int sa5x_oscillator_cmd(struct sa5x_oscillator *sa5x, const char *cmd, int cmd_len)
{
	struct pollfd pfd = {};
	int err, rbytes = 0;
	if (write(sa5x->osc_fd, cmd, cmd_len) != cmd_len) {
		log_error("oscillator_get_attributes send command error: %d (%s)", errno, strerror(errno));
		return -1;
	}

	pfd.fd = sa5x->osc_fd;
	pfd.events = POLLIN;
	// give MAC 10ms to respond with telemetry
	while (1) {
		err = poll(&pfd, 1, 10);
		if (err == -1) {
			log_error("oscillator_get_attributes poll error: %d (%s)", errno, strerror(errno));
			return -1;
		}
		// poll call timed out - check the answer
		if (!err)
			break;
		err = read(sa5x->osc_fd, &answer_str[rbytes], answer_len - rbytes);
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

	if (rbytes < 5) {
		// answer size doesn't fit protocol
		log_error("oscillator_get_attributes answer protocol error: %s", answer_str);
		memset(answer_str, 0, rbytes);
		return -1;
	}

	if (answer_str[0] != '[') {
		// answer format doesn't fit protocol
		log_error("oscillator_get_attributes answer protocol error: %s", answer_str);
		memset(answer_str, 0, rbytes);
		return -1;
	}
	if (answer_str[1] != '=') {
		// there is an error indicated in answer to command
		log_error("oscillator_get_attributes answer is error: %s", answer_str);
		memset(answer_str, 0, rbytes);
		return -1;
	}

	return rbytes;
}

static int sa5x_oscillator_read_intval(int *val, int size)
{
	int res = size;
	if (size > 0) {
		res = sscanf(answer_str, "[=%d]\r\n", val);
		// we have to clean buffer if it has something
		memset(answer_str, 0, size);
	}
	return res;
}

static int sa5x_oscillator_get_attributes(struct oscillator *oscillator, struct sa5x_attributes *a,
										  unsigned int attributes_mask)
{
	struct sa5x_oscillator *sa5x;
	sa5x = container_of(oscillator, struct sa5x_oscillator, oscillator);
	int err, val;

	if (attributes_mask & (ATTR_CTRL|ATTR_STATUS) && !a) {
		log_error("scillator_get_attributes no structure provided");
		return -1;
	}

	if (attributes_mask & ATTR_FW_SERIAL) {
		err = sa5x_oscillator_cmd(sa5x, CMD_SWVER, sizeof(CMD_SWVER));
		if (err > 0) {
			sscanf(answer_str, "[=%20[^,],", sa5x->version);
			memset(answer_str, 0, err);
		}

		err = sa5x_oscillator_cmd(sa5x, CMD_SERIAL, sizeof(CMD_SERIAL));
		if (err > 0) {
			sscanf(answer_str, "[=%12s]\r\n", sa5x->serial);
			memset(answer_str, 0, err);
		}
		return 0;
	}

	if (attributes_mask & ATTR_CTRL) {
		err = sa5x_oscillator_read_intval(&val, sa5x_oscillator_cmd(sa5x, CMD_GET_LOCKED, sizeof(CMD_GET_LOCKED)));
		if (err > 0) {
			a->locked = val;
		} else {
			// this is the only parameter that we depend on
			return err;
		}

		sa5x_oscillator_read_intval(&a->phaseoffset, sa5x_oscillator_cmd(sa5x, CMD_GET_PHASE, sizeof(CMD_GET_PHASE)));

		sa5x_oscillator_read_intval(&a->lastcorrection, sa5x_oscillator_cmd(sa5x, CMD_GET_LASTCORRECTION, sizeof(CMD_GET_LASTCORRECTION)));
	}

	if (attributes_mask & ATTR_STATUS) {
		err = sa5x_oscillator_read_intval(&a->temperature,sa5x_oscillator_cmd(sa5x, CMD_GET_TEMPERATURE, sizeof(CMD_GET_TEMPERATURE)));
		if (err <= 0) {
			// this is the only parameter that we depend on
			return err;
		}


		sa5x_oscillator_read_intval(&a->analogtuning, sa5x_oscillator_cmd(sa5x, CMD_GET_ANALOG_TUNING, sizeof(CMD_GET_ANALOG_TUNING)));

		sa5x_oscillator_read_intval(&a->digitaltuning, sa5x_oscillator_cmd(sa5x, CMD_GET_DIGITAL_TUNING, sizeof(CMD_GET_DIGITAL_TUNING)));

		err = sa5x_oscillator_read_intval(&val, sa5x_oscillator_cmd(sa5x, CMD_GET_ANALOG_TUNING_ENABLED, sizeof(CMD_GET_ANALOG_TUNING_ENABLED)));
		if (err > 0) {
			a->analogtuningon = val;
		}
	}
	return 0;
}

static struct oscillator *sa5x_oscillator_new(struct config *config)
{
	struct sa5x_oscillator *sa5x;
	int fd;
	char *osc_device_name;
	struct oscillator *oscillator;

	sa5x = calloc(1, sizeof(*sa5x));
	if (sa5x == NULL)
		return NULL;
	oscillator = &sa5x->oscillator;
	sa5x->osc_fd = -1;

	osc_device_name = (char *) config_get(config, "sa5x-device");
	if (osc_device_name == NULL) {
		log_error("sa5x-device config key must be provided");
		goto error;
	}

	fd = open(osc_device_name, O_RDWR|O_NONBLOCK);
	if (fd == -1) {
		log_error("Could not open sa5x device\n");
		goto error;
	}

	sa5x->osc_fd = fd;
	if (set_serial_attributes(fd) != 0)
		goto error;

	oscillator_factory_init(FACTORY_NAME, oscillator, FACTORY_NAME "-%d",
			sa5x_oscillator_index);
	sa5x_oscillator_index++;

	log_debug("instantiated " FACTORY_NAME " oscillator");

	if (!sa5x_oscillator_get_attributes(oscillator, NULL, ATTR_FW_SERIAL)) {
		log_debug("connected to MAC with serial %s, fw: %20s", sa5x->serial, sa5x->version);
	}

	return oscillator;
error:
	if (sa5x->osc_fd != -1)
		close(sa5x->osc_fd);
	sa5x_oscillator_destroy(&oscillator);
	return NULL;
}

static int sa5x_oscillator_get_ctrl(struct oscillator *oscillator, struct oscillator_ctrl *ctrl)
{
	// Used for mRO50
	struct sa5x_attributes a;
	int err = sa5x_oscillator_get_attributes(oscillator, &a, ATTR_CTRL);

	ctrl->fine_ctrl = 0;
	ctrl->coarse_ctrl = 0;

	if (!err) {
		ctrl->lock = a.locked;
	} else {
		ctrl->lock = 0;
	}
	return 0;
}

static int sa5x_oscillator_get_temp(struct oscillator *oscillator, double *temp)
{
	struct sa5x_attributes a;
	int err = sa5x_oscillator_get_attributes(oscillator, &a, ATTR_STATUS);
	if (!err) {
		// mDegC to DegC
		*temp = a.temperature / 1000.0;
	} else {
		*temp = -400.0;
	}
	// we cannot propagate error further
	return 0;
}

static const struct oscillator_factory sa5x_oscillator_factory = {
	.class = {
		.name = FACTORY_NAME,
		.get_ctrl = sa5x_oscillator_get_ctrl,
		.get_temp = sa5x_oscillator_get_temp,
	},
	.new = sa5x_oscillator_new,
	.destroy = sa5x_oscillator_destroy,
};

static void __attribute__((constructor)) sa5x_oscillator_constructor(void)
{
	int ret;

	ret = oscillator_factory_register(&sa5x_oscillator_factory);
	if (ret < 0)
		log_error("oscillator_factory_register", ret);
}
