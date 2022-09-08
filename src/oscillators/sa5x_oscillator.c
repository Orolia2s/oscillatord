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

#define BIT(nr)			(1UL << (nr))

#define ATTR_FW_SERIAL    		BIT(0)
#define ATTR_CTRL         		BIT(1)
#define ATTR_STATUS       		BIT(2)
#define ATTR_PHASE		  		BIT(3)
#define ATTR_STATUS_PPS         BIT(4)
#define ATTR_STATUS_TEMPERATURE BIT(5)

#define CMD_SWVER                     "\\{swrev?}"
#define CMD_SERIAL                    "{serial?}"
#define CMD_LATCH					  "{latch}"
#define CMD_GET_ALARMS                "{get,Alarms}"
#define CMD_GET_LOCKED                "{get,Locked}"
#define CMD_GET_DISCIPLINE_LOCKED     "{get,DisciplineLocked}"
#define CMD_GET_GNSS_PPS              "{get,PpsInDetected}"
#define CMD_GET_PHASE                 "{get,Phase}"
#define CMD_GET_LASTCORRECTION        "{get,LastCorrection}"
#define CMD_GET_TEMPERATURE           "{get,Temperature}"
#define CMD_GET_ANALOG_TUNING         "{get,AnalogTuning}"
#define CMD_GET_DIGITAL_TUNING        "{get,DigitalTuning}"
#define CMD_SET_DIGITAL_TUNING        "{set,DigitalTuning,%d}"
#define CMD_GET_ANALOG_TUNING_ENABLED "{get,AnalogTuningEnabled}"
#define CMD_GET_TAU                   "{get,TauPps0}"
#define CMD_SET_TAU                   "{set,TauPps0,%d}"
#define CMD_GET_DISCIPLINING          "{get,Disciplining}"
#define CMD_SET_DISCIPLINING          "{set,Disciplining,%d}"
#define DISCIPLINING_PHASES 3

static const unsigned int tau_values[DISCIPLINING_PHASES] = {50, 500, 10000};
static const unsigned int tau_interval[DISCIPLINING_PHASES] = {600, 7200, 86400}; // in seconds

enum SA5x_ClockClass {
	SA5X_CLOCK_CLASS_UNCALIBRATED,
	SA5X_CLOCK_CLASS_CALIBRATING,
	SA5X_CLOCK_CLASS_HOLDOVER,
	SA5X_CLOCK_CLASS_LOCK,
	SA5X_CLOCK_CLASS_NUM
};

enum SA5x_Disciplining_State {
	/** Initialization State */
	SA5X_INIT,
	/** Quick convergence phase, tracking phase error to reach 0 */
	SA5X_TRACKING,
	/** Holdover state, when gnss data is not valid */
	SA5X_HOLDOVER,
	/** Calibration state, when drift coefficients are computed */
	SA5X_CALIBRATION,
	SA5X_NUM_STATES
};

struct sa5x_disciplining_status {
	enum SA5x_Disciplining_State status;
	enum SA5x_ClockClass clock_class;
	int current_phase_convergence_count;
	int valid_phase_convergence_threshold;
	float convergence_progress;
	bool holdover_ready;
};

struct sa5x_oscillator {
	struct oscillator oscillator;
	int	osc_fd;
	int disciplining_phase;
	struct sa5x_disciplining_status status;
	struct timespec disciplining_start;
	struct timespec gnss_last_fix;
	bool gnss_fix_status;
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
	uint32_t tau;               // Current TAU value
	// General Status bits
	uint8_t  ppsindetected:1;	// PpsInDetected
	uint8_t  locked:1;		//Locked
	uint8_t  disciplinelocked:1;	//DisciplineLocked
	uint8_t  analogtuningon:1;	// Analog Tuning On/Off
	uint8_t  hwinforead:1;		// Initial info rbytesflag
	uint8_t  disciplining:1;    // Is disciplining enabled
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
		res = sscanf(answer_str, "[=%d]\n\n", val);
		// we have to clean buffer if it has something
		memset(answer_str, 0, size);
	}
	return res;
}

static int sa5x_oscillator_read_phase(int32_t *val, int size)
{
	int res = size;
	double phase;
	if (size > 0) {
		res = sscanf(answer_str, "[=%lf]\n\n", &phase);
		// we have to clean buffer if it has something
		memset(answer_str, 0, size);
	}
	if (res)
		*val = (int32_t)(phase + (phase >= 0 ? 0.5 : -0.5));
	return res;
}

static int sa5x_oscillator_get_attributes(struct oscillator *oscillator, struct sa5x_attributes *a,
										  unsigned int attributes_mask)
{
	struct sa5x_oscillator *sa5x;
	sa5x = container_of(oscillator, struct sa5x_oscillator, oscillator);
	int err, val;

	if (attributes_mask & (ATTR_CTRL|ATTR_STATUS|ATTR_STATUS_PPS|ATTR_PHASE|ATTR_STATUS_TEMPERATURE) && !a) {
		log_error("scillator_get_attributes no structure provided");
		return -1;
	}

	if (attributes_mask & ATTR_FW_SERIAL) {
		err = sa5x_oscillator_cmd(sa5x, CMD_SWVER, sizeof(CMD_SWVER));
		if (err > 0) {
			sscanf(answer_str, "[=%19[^,],", sa5x->version);
			memset(answer_str, 0, err);
		}

		err = sa5x_oscillator_cmd(sa5x, CMD_SERIAL, sizeof(CMD_SERIAL));
		if (err > 0) {
			sscanf(answer_str, "[=%11c]\r\n", sa5x->serial);
			memset(answer_str, 0, err);
		}
		return 0;
	}

	if (attributes_mask & (ATTR_STATUS_PPS | ATTR_STATUS)) {
		err = sa5x_oscillator_read_intval(&val, sa5x_oscillator_cmd(sa5x, CMD_GET_DISCIPLINE_LOCKED, sizeof(CMD_GET_DISCIPLINE_LOCKED)));
		if (err > 0) {
			a->disciplinelocked = val;
		}

		err = sa5x_oscillator_read_intval(&val, sa5x_oscillator_cmd(sa5x, CMD_GET_GNSS_PPS, sizeof(CMD_GET_GNSS_PPS)));
		if (err > 0) {
			a->ppsindetected = val;
			if (!val)
				log_debug("SA5x reports no PPS-in");
		} else {
			// this is the only parameter that we depend on
			log_warn("SA5x doesn't return status of PPS signal");
			return err;
		}

	}

	if (attributes_mask & ATTR_CTRL) {
		err = sa5x_oscillator_read_intval(&val, sa5x_oscillator_cmd(sa5x, CMD_GET_LOCKED, sizeof(CMD_GET_LOCKED)));
		if (err > 0) {
			a->locked = val;
		}

		if (sa5x_oscillator_read_intval(&val, sa5x_oscillator_cmd(sa5x, CMD_GET_TAU, sizeof(CMD_GET_TAU))) > 0) {
			a->tau = val;
		}
		sa5x_oscillator_read_intval(&a->lastcorrection, sa5x_oscillator_cmd(sa5x, CMD_GET_LASTCORRECTION, sizeof(CMD_GET_LASTCORRECTION)));
	}

	if (attributes_mask & ATTR_STATUS) {
		sa5x_oscillator_read_intval(&a->digitaltuning, sa5x_oscillator_cmd(sa5x, CMD_GET_DIGITAL_TUNING, sizeof(CMD_GET_DIGITAL_TUNING)));

		if(sa5x_oscillator_read_intval(&val, sa5x_oscillator_cmd(sa5x, CMD_GET_ALARMS, sizeof(CMD_GET_ALARMS)))) {
			a->alarms = (uint32_t)val;
		}

		err = sa5x_oscillator_read_intval(&val, sa5x_oscillator_cmd(sa5x, CMD_GET_DISCIPLINING, sizeof(CMD_GET_DISCIPLINING)));
		if (err > 0) {
			a->disciplining = val;
			if (!val)
				log_debug("SA5x reports no internal disciplining");
		}
	}

	if (attributes_mask & ATTR_PHASE) {

		sa5x_oscillator_read_phase(&a->phaseoffset, sa5x_oscillator_cmd(sa5x, CMD_GET_PHASE, sizeof(CMD_GET_PHASE)));

	}

	if (attributes_mask & ATTR_STATUS_TEMPERATURE) {
		err = sa5x_oscillator_read_intval(&a->temperature,sa5x_oscillator_cmd(sa5x, CMD_GET_TEMPERATURE, sizeof(CMD_GET_TEMPERATURE)));
		if (err <= 0) {
			// this is the only parameter that we depend on
			return err;
		}

	}
	return 0;
}

static struct oscillator *sa5x_oscillator_new(struct devices_path *devices_path)
{
	struct sa5x_oscillator *sa5x;
	int fd;
	struct oscillator *oscillator;
	int cmd_len;

	sa5x = calloc(1, sizeof(*sa5x));
	if (sa5x == NULL)
		return NULL;
	oscillator = &sa5x->oscillator;
	sa5x->osc_fd = -1;

	fd = open(devices_path->mac_path, O_RDWR|O_NONBLOCK);
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

	sa5x->status.clock_class = SA5X_CLOCK_CLASS_CALIBRATING;
	sa5x->status.status = SA5X_INIT;
	sa5x->status.current_phase_convergence_count = -1;
	sa5x->status.valid_phase_convergence_threshold = -1;
	sa5x->status.convergence_progress = 0.0f;
	sa5x->status.holdover_ready = false;
	clock_gettime(CLOCK_MONOTONIC, &sa5x->disciplining_start);

	cmd_len = snprintf(answer_str, answer_len, CMD_SET_TAU, tau_values[0]);
	if (sa5x_oscillator_cmd(sa5x, answer_str, cmd_len) == -1) {
		log_debug("couldn't reset TAU for oscillator");
	}

	return oscillator;
error:
	if (sa5x->osc_fd != -1)
		close(sa5x->osc_fd);
	sa5x_oscillator_destroy(&oscillator);
	return NULL;
}

// Issue latch sequence to restore from out-of-range tuning values
static int sa5x_oscillator_latch(struct sa5x_oscillator *sa5x, struct sa5x_attributes *a)
{
	int cmd_len;

	if (a->disciplining) {
		cmd_len = snprintf(answer_str, answer_len, CMD_SET_DISCIPLINING, 0);
		if (sa5x_oscillator_cmd(sa5x, answer_str, cmd_len) == -1) {
			log_warn("SA5x: couldn't disable disciplining for latch command");
			return 1;
		}
	}

	if (sa5x_oscillator_cmd(sa5x, CMD_LATCH, sizeof(CMD_LATCH)) == -1)
		log_warn("SA5x: error with latch command");

	cmd_len = snprintf(answer_str, answer_len, CMD_SET_DIGITAL_TUNING, 0);
	if (sa5x_oscillator_cmd(sa5x, answer_str, cmd_len) == -1) {
		log_warn("SA5x: couldn't clear digital tuning value");
	}
	cmd_len = snprintf(answer_str, answer_len, CMD_SET_DISCIPLINING, 1);
	if (sa5x_oscillator_cmd(sa5x, answer_str, cmd_len) == -1) {
		log_warn("SA5x: couldn't enable disciplining after latch command");
	}
	a->disciplining = 1;
	return 0;
}

static int sa5x_oscillator_get_ctrl(struct oscillator *oscillator, struct oscillator_ctrl *ctrl)
{
	struct sa5x_oscillator *sa5x = container_of(oscillator, struct sa5x_oscillator, oscillator);
	bool adjust_tau = false, latch = false;
	struct sa5x_attributes a = {};
	struct timespec ts;
	int cmd_len;
	int err = sa5x_oscillator_get_attributes(oscillator, &a, ATTR_CTRL|ATTR_STATUS);

	// coarse_ctrl wil contain TAU value
	if (!err) {
		ctrl->fine_ctrl = a.lastcorrection;
		ctrl->coarse_ctrl = a.tau;
	} else {
		ctrl->fine_ctrl = -1;
		ctrl->coarse_ctrl = 0;
		return 0;
	}

	// check if we stuck with disciplining error
	if (a.alarms) {
		if ((a.alarms & BIT(18)) && !a.lastcorrection) {
			// digital correction is out of range, needs latch command
			log_warn("SA5x: Digital tuning is out of range, adjust base frequency initiated");
			if (sa5x_oscillator_latch(sa5x, &a)) {
				log_error("SA5x: Couldn't make latch command");
			}
			latch = true;
			adjust_tau = true;
		} else {
			log_warn("SA5x: Alarms are raised, 0x%8X", a.alarms);
		}
	}

	clock_gettime(CLOCK_MONOTONIC, &ts);
	if (a.ppsindetected == 0 && sa5x->gnss_fix_status) {
		log_debug("SA5x reports loss of PPS while GNSS fix is OK");
	}
	if (!sa5x->gnss_fix_status || latch) {
		// we are out of GNSS sync, have to restart disciplining
		// or change state to UNCALIBRATED if we are in HOLDOVER more than 24h
		adjust_tau = (sa5x->disciplining_phase != 0) ||
					ts.tv_sec - sa5x->gnss_last_fix.tv_sec > 24 * 3600;
		sa5x->disciplining_phase = 0;
		sa5x->disciplining_start = ts;
	} else {
		if (sa5x->status.clock_class == SA5X_CLOCK_CLASS_HOLDOVER ||
			sa5x->status.clock_class == SA5X_CLOCK_CLASS_UNCALIBRATED) {
			// indicate that we are not in holdover anymore
			sa5x->status.clock_class = SA5X_CLOCK_CLASS_CALIBRATING;
			sa5x->status.status = SA5X_TRACKING;
		}
	}
	if (sa5x->disciplining_phase < (DISCIPLINING_PHASES - 1) &&
	    ts.tv_sec - sa5x->disciplining_start.tv_sec > tau_interval[sa5x->disciplining_phase]) {
		adjust_tau = true;
		sa5x->disciplining_phase++;
	}

	if (adjust_tau) {
		cmd_len = snprintf(answer_str, answer_len, CMD_SET_TAU, tau_values[sa5x->disciplining_phase]);
		if (sa5x_oscillator_cmd(sa5x, answer_str, cmd_len) == -1) {
			log_debug("couldn't set TAU to %d", tau_values[sa5x->disciplining_phase]);
		}
		if (!sa5x->gnss_fix_status) {
			sa5x->status.clock_class = ((sa5x->status.clock_class == SA5X_CLOCK_CLASS_CALIBRATING) ||
										(ts.tv_sec - sa5x->gnss_last_fix.tv_sec > 24 * 3600)) ?
										SA5X_CLOCK_CLASS_UNCALIBRATED : SA5X_CLOCK_CLASS_HOLDOVER;
			sa5x->status.status = SA5X_HOLDOVER;
		} else if (sa5x->disciplining_phase == 0) {
			sa5x->status.clock_class = SA5X_CLOCK_CLASS_CALIBRATING;
			sa5x->status.status = SA5X_TRACKING;
		} else {
			sa5x->status.clock_class = SA5X_CLOCK_CLASS_LOCK;
			sa5x->status.status = SA5X_CALIBRATION;
		}
		sa5x->status.holdover_ready = sa5x->disciplining_phase == (DISCIPLINING_PHASES - 1);
	}

	return 0;
}

static int sa5x_oscillator_parse_attributes(struct oscillator *oscillator, struct oscillator_attributes *attributes)
{
	struct sa5x_attributes a = {};
	int err = sa5x_oscillator_get_attributes(oscillator, &a, ATTR_STATUS_TEMPERATURE|ATTR_STATUS_PPS);
	if (!err) {
		// mDegC to DegC
		attributes->temperature = a.temperature / 1000.0;
		attributes->locked = a.ppsindetected && a.disciplinelocked;
	} else {
		attributes->temperature = -400.0;
		attributes->locked = 0;
	}
	// we cannot propagate error further
	return 0;
}

static int sa5x_oscillator_get_phase_error(struct oscillator *oscillator, int64_t *phase_error)
{
	struct sa5x_attributes a;
	int err = sa5x_oscillator_get_attributes(oscillator, &a, ATTR_PHASE);
	if (!err) {
		*phase_error = a.phaseoffset;
	} else {
		*phase_error = 0;
		return -EINVAL;
	}
	// we cannot propagate error further
	return 0;
}

static int sa5x_oscillator_get_disciplining_status(struct oscillator *oscillator, void *data)
{
	// we assume that all the values are already requested
	struct sa5x_oscillator *sa5x = container_of(oscillator, struct sa5x_oscillator, oscillator);
	struct sa5x_disciplining_status *mon = (struct sa5x_disciplining_status *)data;

	*mon = sa5x->status;
	return 0;
}

static int sa5x_oscillator_push_gnss_info(struct oscillator *oscillator, bool fixOk, const struct timespec *last_fix_utc_time)
{
	struct sa5x_oscillator *sa5x = container_of(oscillator, struct sa5x_oscillator, oscillator);
	sa5x->gnss_fix_status = fixOk;
	if (last_fix_utc_time)
		sa5x->gnss_last_fix = *last_fix_utc_time;
	return 0;
}

static const struct oscillator_factory sa5x_oscillator_factory = {
	.class = {
		.name = FACTORY_NAME,
		.get_ctrl = sa5x_oscillator_get_ctrl,
		.get_phase_error = sa5x_oscillator_get_phase_error,
		.get_disciplining_status = sa5x_oscillator_get_disciplining_status,
		.parse_attributes = sa5x_oscillator_parse_attributes,
		.push_gnss_info = sa5x_oscillator_push_gnss_info,
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
