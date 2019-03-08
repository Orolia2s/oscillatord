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

#include <error.h>

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
#define NS_IN_SECOND 1000000000l

int main (int argc, char *argv[])
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
	struct __attribute__((cleanup(oscillator_factory_destroy)))
			oscillator *oscillator = NULL;

	if (argc != 2)
		error(EXIT_FAILURE, 0, "usage: %s pps_device_path", argv[0]);
	path = argv[1];

	ret = config_init(&config, path);
	if (ret != 0)
		error(EXIT_FAILURE, -ret, ERR "config_init(%s)", path);

	device = config_get(&config, "pps-device");
	if (device == NULL)
		error(EXIT_FAILURE, errno, ERR "PPS device not defined in "
				"config %s", path);
	printf("PPS device %s\n", device);

	oscillator = oscillator_factory_new(&config);
	if (oscillator == NULL)
		error(EXIT_FAILURE, errno, ERR "oscillator_factory_new");
	printf("oscillator model %s\n", oscillator->factory_name);

	fd = open(device, O_RDWR);
	if (fd == -1)
		error(EXIT_FAILURE, errno, "open(%s)", device);

	od = od_new(CLOCK_MONOTONIC);
	if (od == NULL)
		error(EXIT_FAILURE, errno, "od_new");

	/* correct the phase error by applying an opposite offset */
	sret = read(fd, &phase_error, sizeof(phase_error));
	if (sret == -1)
		error(EXIT_FAILURE, errno, "read");
	phase_error = -phase_error;
	sret = write(fd, &phase_error, sizeof(phase_error));
	if (sret == -1)
		error(EXIT_FAILURE, errno, "write");

	while (true) {
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
			error(EXIT_FAILURE, errno, "select");
		}

		sret = read(fd, &phase_error, sizeof(phase_error));
		if (sret == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;

			error(EXIT_FAILURE, errno, "read");
		}

		input = (struct od_input) {
			.phase_error = (struct timespec) {
				.tv_sec = phase_error / NS_IN_SECOND,
				.tv_nsec = phase_error % NS_IN_SECOND,
			},
			/* TODO hardcoded until we can get the GNSS status */
			.valid = true,
		};
		ret = od_process(od, &input, &output);
		if (ret < 0)
			error(EXIT_FAILURE, -ret, "od_process");

		printf("input:"
				"\tphase_error = %ld.%09ld\n"
				"\tvalid = %s\n",
				input.phase_error.tv_sec,
				input.phase_error.tv_nsec,
				input.valid ? "true" : "false");
		printf("output: setpoint = %"PRIu32"\n", output.setpoint);
		ret = oscillator_set_dac(oscillator, output.setpoint);
		if (ret < 0)
			error(EXIT_FAILURE, -ret, "oscillator_set_dac");

	}

	od_destroy(&od);

	return EXIT_SUCCESS;
}
