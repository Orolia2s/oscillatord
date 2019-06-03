#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>

#include <error.h>

#include <tsync.h>

#include <oscillator-disciplining/oscillator-disciplining.h>

#include "log.h"
#include "oscillator.h"
#include "oscillator_factory.h"
#include "config.h"
#include "utils.h"

/*
 * The driver has a watchdog which resets the 1PPS device if no interrupt has
 * been received in the last two seconds, so a timeout of more than 4 seconds
 * means that even the watchdog couldn't "repair" the 1PPS device.
 */
#define LOOP_TIMEOUT 4

static void dummy_print_progname(void)
{
	fprintf(stderr, ERR);
}

static volatile int loop = true;

static void signal_handler(int signum)
{
	info("Caught signal %s.\n", strsignal(signum));
	if (!loop) {
		err("Signalled twice, brutal exit.\n");
		exit(EXIT_FAILURE);
	}
	loop = false;
}

int main(int argc, char *argv[])
{
	fd_set rfds;
	struct __attribute__((cleanup(od_destroy)))od *od = NULL;
	struct od_input input;
	struct od_output output;
	int ret;
	int __attribute__((cleanup(fd_cleanup))) fd = -1;
	struct timeval tv;
	ssize_t sret;
	int32_t phase_error;
	const char *device;
	struct config config;
	const char *path;
	const char *libod_conf_path;
	struct __attribute__((cleanup(oscillator_factory_destroy)))
			oscillator *oscillator = NULL;
	TSYNC_BoardHandle hnd;
	TSYNC_ERROR tsync_error;
	int _;
	const char *tsync_device;
	int pps_valid;
	int gnss_device_index;
	bool opposite_phase_error;
	const char *value;
	int sign;
	unsigned turns;
	char err_msg[OD_ERR_MSG_LEN];
	uint16_t temperature;

	/* remove the line startup in error() calls */
	error_print_progname = dummy_print_progname;

	if (argc != 2)
		error(EXIT_FAILURE, 0, "usage: %s config_file_path", argv[0]);
	path = argv[1];

	ret = config_init(&config, path);
	if (ret != 0)
		error(EXIT_FAILURE, -ret, "config_init(%s)", path);

	log_enable_debug(config_get_bool_default(&config, "enable-debug",
			false));

	value = config_get(&config, "turns");
	if (value != NULL) {
		turns = atoll(value);
	} else {
		turns = 0;
	}

	oscillator = oscillator_factory_new(&config);
	if (oscillator == NULL)
		error(EXIT_FAILURE, errno, "oscillator_factory_new");
	info("oscillator model %s\n", oscillator->class->name);

	device = config_get(&config, "pps-device");
	if (device == NULL)
		error(EXIT_FAILURE, errno, "pps-device not defined in "
				"config %s", path);
	info("PPS device %s\n", device);

	tsync_device = config_get(&config, "tsync-device");
	if (tsync_device == NULL)
		error(EXIT_FAILURE, errno, "tsync-device not defined in "
				"config %s", path);
	info("tsync device %s\n", tsync_device);

	gnss_device_index = config_get_uint8_t(&config, "device-index");
	if (gnss_device_index < 0)
		error(EXIT_FAILURE, errno, "device-index not defined in config "
				"%s", path);
	info("GPS index %d\n", gnss_device_index);

	fd = open(device, O_RDWR);
	if (fd == -1)
		error(EXIT_FAILURE, errno, "open(%s)", device);

	libod_conf_path = config_get_default(&config, "libod-config-path",
			path);
	od = od_new_from_config(libod_conf_path, err_msg);
	if (od == NULL)
		error(EXIT_FAILURE, errno, "od_new %s", err_msg);

	/* correct the phase error by applying an opposite offset */
	sret = read(fd, &phase_error, sizeof(phase_error));
	if (sret == -1)
		error(EXIT_FAILURE, errno, "read");
	phase_error = -phase_error;
	sret = write(fd, &phase_error, sizeof(phase_error));
	if (sret == -1)
		error(EXIT_FAILURE, errno, "write");
	info("applied an initial offset correction of %"PRIi32"ns\n",
			phase_error);

	opposite_phase_error = config_get_bool_default(&config,
			"opposite-phase-error", false);
	sign = opposite_phase_error ? -1 : 1;
	if (opposite_phase_error)
		info("taking the opposite of the phase error reported\n");
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	do {
		turns--;
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);

		tv = (struct timeval) { .tv_sec = LOOP_TIMEOUT, .tv_usec = 0 };
		ret = select(fd + 1, &rfds, NULL, NULL, &tv);
		switch (ret) {
		case 0:
			error(EXIT_FAILURE, 0, "Timeout, shouldn't happen!");
			/* no fall through GCC, error(1, ...) doesn't return */
			__attribute__ ((fallthrough));

		case -1:
			if (errno == EINTR)
				continue;
			error(EXIT_FAILURE, errno, "select");
		}

		sret = read(fd, &phase_error, sizeof(phase_error));
		if (sret == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;

			error(EXIT_FAILURE, errno, "read");
		}
		ret = oscillator_get_temp(oscillator, &temperature);
		if (ret < 0)
			error(0, -ret, "oscillator_get_temp");

		tsync_error = TSYNC_open(&hnd, tsync_device);
		if (tsync_error != TSYNC_SUCCESS)
			error(EXIT_FAILURE, 0, "TSYNC_open: %s",
					tsync_strerror(tsync_error));

		tsync_error = TSYNC_GR_getValidity(hnd, gnss_device_index, &_,
				&pps_valid);
		if (tsync_error != TSYNC_SUCCESS)
			error(EXIT_FAILURE, 0, "TSYNC_GR_getValidity: %s",
					tsync_strerror(tsync_error));

		tsync_error = TSYNC_close(hnd);
		if (tsync_error != TSYNC_SUCCESS)
			error(EXIT_FAILURE, 0, "TSYNC_close: %s",
					tsync_strerror(tsync_error));

		input = (struct od_input) {
			.phase_error = (struct timespec) {
				.tv_sec = sign * phase_error / NS_IN_SECOND,
				.tv_nsec = sign * phase_error % NS_IN_SECOND,
			},
			.valid = pps_valid,
			.temperature = temperature,
		};
		ret = od_process(od, &input, &output);
		if (ret < 0)
			error(EXIT_FAILURE, -ret, "od_process");

		debug("input: phase_error = (%lds, %09ldns), valid = %s\n",
				input.phase_error.tv_sec,
				input.phase_error.tv_nsec,
				input.valid ? "true" : "false");
		debug("output: setpoint = %"PRIu32"\n", output.setpoint);
		ret = oscillator_set_dac(oscillator, output.setpoint);
		if (ret < 0)
			error(EXIT_FAILURE, -ret, "oscillator_set_dac");
	} while (loop && turns != 1);


	od_destroy(&od);

	return EXIT_SUCCESS;
}
