#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "config.h"
#include "log.h"
#include "utils.h"

#include "../oscillator.h"
#include "../oscillator_factory.h"

#define FACTORY_NAME "sa3x"

struct sa3x_oscillator {
	struct oscillator oscillator;
	FILE *osc_fd;
};

struct sa3x_attributes {
	char bite;           // BITE
	char version[5];     // Version
	char serial[12];     // SerialNumber
	int  teccontrol;     // TEC Control (mDegC)
	int  rfcontrol;      // RF Control (0.1mv)
	int  ddscurrent;     // DDS Frequency Center Current (0.01Hz)
	int  cellcurrent;    // CellHeaterCurrent (ma)
	int  dcsignal;       // DCSignal (mv)
	int  temperature;    // Temperature (mDegC)
	int  digitaltuning;  // Digital Tuning (0.01Hz)
	char analogtuningon; // Analog Tuning On/Off
	int  analogtuning;   // Analog Tuning (mv)
};

static unsigned int sa3x_oscillator_index;

static void sa3x_oscillator_destroy(struct oscillator **oscillator)
{
	struct oscillator *o;
	struct sa3x_oscillator *r;

	if (oscillator == NULL || *oscillator == NULL)
		return;

	o = *oscillator;
	r = container_of(o, struct sa3x_oscillator, oscillator);
	if (r->osc_fd != NULL) {
		fclose(r->osc_fd);
		log_info("Closed oscillator's serial port");
	}
	memset(o, 0, sizeof(*o));
	free(o);
	*oscillator = NULL;
}

static int set_serial_attributes(FILE *fd)
{
	struct termios tty;

	if (tcgetattr(fileno(fd), &tty) != 0){
		log_error("error from tcgetattr\n");
		return -1;
	}

	cfsetospeed(&tty, B57600);
	cfsetispeed(&tty, B57600);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
	// disable IGNBRK for mismatched speed tests; otherwise receive break
	// as \000 chars
	tty.c_iflag &= ~IGNBRK;         // disable break processing
	tty.c_lflag = 0;                // no signaling chars, no echo,
	                                // no canonical processing
	tty.c_oflag = 0;                // no remapping, no delays

	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

	tty.c_cflag |= (CLOCAL | CREAD);   // ignore modem controls,
	                                   // enable reading
	tty.c_cflag &= ~(PARENB | PARODD); // shut off parity
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	if (tcsetattr(fileno(fd), TCSANOW, &tty) != 0){
		log_error("error from tcsetattr\n");
		return -1;
	}

	return 0;
}

static struct oscillator *sa3x_oscillator_new(struct config *config)
{
	struct sa3x_oscillator *sa3x;
	FILE *fd;
	char *osc_device_name;
	struct oscillator *oscillator;

	sa3x = calloc(1, sizeof(*sa3x));
	if (sa3x == NULL)
		return NULL;
	oscillator = &sa3x->oscillator;
	sa3x->osc_fd = NULL;

	osc_device_name = (char *) config_get(config, "sa3x-device");
	if (osc_device_name == NULL) {
		log_error("sa3x-device config key must be provided");
		goto error;
	}
	
	fd = fopen(osc_device_name, "r+");
	if (fd == NULL) {
		log_error("Could not open sa3x device\n");
		goto error;
	}

	if (set_serial_attributes(fd) != 0)
		goto error;
	sa3x->osc_fd = fd;

	oscillator_factory_init(FACTORY_NAME, oscillator, FACTORY_NAME "-%d",
			sa3x_oscillator_index);
	sa3x_oscillator_index++;

	log_debug("instantiated " FACTORY_NAME " oscillator");

	return oscillator;
error:
	sa3x_oscillator_destroy(&oscillator);
	return NULL;
}

static int sa3x_oscillator_get_ctrl(struct oscillator *oscillator, struct oscillator_ctrl *ctrl)
{
	// Used for mRO50
	ctrl->fine_ctrl = 0;
	ctrl->coarse_ctrl = 0;
	ctrl->lock = 0;
	return 0;
}

static int sa3x_oscillator_get_attributes(struct oscillator *oscillator, struct sa3x_attributes *a)
{
	struct sa3x_oscillator *sa3x;
	sa3x = container_of(oscillator, struct sa3x_oscillator, oscillator);

	char *command = "^";
	fwrite(command, 1, 1, sa3x->osc_fd);

	char line[64];
	char *l = line;
	size_t n;
	getline(&l, &n, sa3x->osc_fd);

	sscanf(line, "%c,%[^,],%[^,],%d,%d,%d,%d,%d,%d,%d,%c,%d",
		&a->bite, a->version, a->serial, &a->teccontrol, &a->rfcontrol, &a->ddscurrent,
		&a->cellcurrent, &a->dcsignal, &a->temperature, &a->digitaltuning, &a->analogtuningon,
		&a->analogtuning);

	return 0;
}

static int sa3x_oscillator_get_temp(struct oscillator *oscillator, double *temp)
{
	struct sa3x_attributes a;
	sa3x_oscillator_get_attributes(oscillator, &a);
	// mDegC to DegC
	*temp = a.temperature / 1000.0;
	return 0;
}

static const struct oscillator_factory sa3x_oscillator_factory = {
	.class = {
			.name = FACTORY_NAME,
			.get_ctrl = sa3x_oscillator_get_ctrl,
			.get_temp = sa3x_oscillator_get_temp,
	},
	.new = sa3x_oscillator_new,
	.destroy = sa3x_oscillator_destroy,
};

static void __attribute__((constructor)) sa3x_oscillator_constructor(void)
{
	int ret;

	ret = oscillator_factory_register(&sa3x_oscillator_factory);
	if (ret < 0)
		log_error("oscillator_factory_register", ret);
}
