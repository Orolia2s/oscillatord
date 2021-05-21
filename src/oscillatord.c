#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
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

#include <oscillator-disciplining/oscillator-disciplining.h>
#include <linux/ptp_clock.h>
#include <sys/timex.h>

#include "log.h"
#include "oscillator.h"
#include "oscillator_factory.h"
#include "gnss.h"
#include "config.h"
#include "utils.h"

#define CLOCKFD 3
#define FD_TO_CLOCKID(fd)	((clockid_t) ((((unsigned int) ~fd) << 3) | CLOCKFD))

/*
 * The driver has a watchdog which resets the 1PPS device if no interrupt has
 * been received in the last two seconds, so a timeout of more than 4 seconds
 * means that even the watchdog couldn't "repair" the 1PPS device.
 */
#define LOOP_TIMEOUT 4

__attribute__((cleanup(gnss_cleanup))) struct gnss gnss = {0};

static void dummy_print_progname(void)
{
	fprintf(stderr, ERR);
}

static volatile int loop = true;

/*
 * Signal Handler to kill program gracefully
 */
static void signal_handler(int signum)
{
	info("Caught signal %s.\n", strsignal(signum));
	if (!loop) {
		err("Signalled twice, brutal exit.\n");
		exit(EXIT_FAILURE);
	}
	loop = false;
}

static int apply_phase_offset(int fd, const char *device_name,
				  int32_t phase_error)
{
	int ret;

	ret = write(fd, &phase_error, sizeof(phase_error));
	if (ret == -1) {
		err("Can't write %s ", device_name);
		return -errno;
	}
	info("%s: applied a phase offset correction of %"PRIi32"ns\n",
			device_name, phase_error);

	return ret;
}


int main(int argc, char *argv[])
{
	struct od_input input;
	struct od_output output;
	struct config config;
	struct timeval tv;
	struct gnss_data gnss_data;
	const char *phasemeter_device;
	const char *ptp_clock;
	const char *path;
	const char *libod_conf_path;
	const char *value;
	char err_msg[OD_ERR_MSG_LEN];
	fd_set rfds;
	uint16_t temperature;
	int32_t phase_error;
	ssize_t sret;
	unsigned int turns;
	int ret;
	int sign;
	bool opposite_phase_error;
	bool ignore_next_irq = false;

	__attribute__((cleanup(od_destroy))) struct od *od = NULL;
	__attribute__((cleanup(fd_cleanup))) int fd_phasemeter = -1;
	__attribute__((cleanup(fd_cleanup))) int fd_clock = -1;
	__attribute__((cleanup(oscillator_factory_destroy)))
			struct oscillator *oscillator = NULL;

	/* remove the line startup in error() calls */
	error_print_progname = dummy_print_progname;

	if (argc != 2)
		error(EXIT_FAILURE, 0, "usage: %s config_file_path", argv[0]);
	path = argv[1];


	/* Read Config file */
	ret = config_init(&config, path);
	if (ret != 0) {
		error(EXIT_FAILURE, -ret, "config_init(%s)", path);
		return -EINVAL;
	}
	/* Set log level according to configuration */
	log_enable_debug(config_get_bool_default(&config, "debug", false));

	value = config_get(&config, "turns");
	turns = (value != NULL) ? atoll(value) : 0;

	oscillator = oscillator_factory_new(&config);
	if (oscillator == NULL)
		error(EXIT_FAILURE, errno, "oscillator_factory_new");
	info("oscillator model %s\n", oscillator->class->name);

	phasemeter_device = config_get(&config, "phasemeter-device");
	if (phasemeter_device == NULL) {
		error(EXIT_FAILURE, errno, "pps-device not defined in "
				"config %s", path);
		return -EINVAL;
	}
	info("PPS device %s\n", phasemeter_device);

	fd_phasemeter = open(phasemeter_device, O_RDWR);
	if (fd_phasemeter == -1) {
		error(EXIT_FAILURE, errno, "open(%s)", phasemeter_device);
		return -EINVAL;
	}

	/* Get path to disciplining shared library */
	libod_conf_path = config_get_default(&config, "libod-config-path",
		path);
	
	/* Create shared library oscillator object */
	od = od_new_from_config(libod_conf_path, err_msg);
	if (od == NULL) {
		error(EXIT_FAILURE, errno, "od_new %s", err_msg);
		return -EINVAL;
	}

	opposite_phase_error = config_get_bool_default(&config,
			"opposite-phase-error", false);
	sign = opposite_phase_error ? -1 : 1;

	/* Start GNSS Thread */
	ret = gnss_init(&config, &gnss);	
	if (ret < 0) {
		error(EXIT_FAILURE, errno, "Failed to listen to the receiver");
		return -EINVAL;
	}

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	ptp_clock = config_get(&config, "ptp-clock");
	if  (ptp_clock == NULL) {
		error(EXIT_FAILURE, errno, "ptp-clock not defined in "
				"config %s", path);
		return -EINVAL;
	}

	bool clock_set = false;
	// TODO: Implement Timeout
	while(!clock_set) {
		struct gnss_data data = gnss_get_data(&gnss);
		if (data.valid) {
			/* Test open and configure PHC */
			clockid_t clkid;
			struct timespec ts;

			ts.tv_sec = data.time;
			ts.tv_nsec = 0;

			fd_clock = open(ptp_clock, O_RDWR);
			if (fd_clock < 0)
				return -1;

			clkid = FD_TO_CLOCKID(fd_clock);

			ret = clock_settime(clkid, &ts);
			if (ret == 0)
				clock_set = true;
		} else {
			sleep(2);
		}
	}

	/* Main Loop */
	do {
		turns--;
		FD_ZERO(&rfds);
		FD_SET(fd_phasemeter, &rfds);

		tv = (struct timeval) { .tv_sec = LOOP_TIMEOUT, .tv_usec = 0 };
		ret = select(fd_phasemeter + 1, &rfds, NULL, NULL, &tv);
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

		/* Read phase error from phasemeter */
		sret = read(fd_phasemeter, &phase_error, sizeof(phase_error));
		if (sret == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			error(EXIT_FAILURE, errno, "read");
		}


		if (ignore_next_irq) {
			debug("ignoring 1 input due to phase jump\n");
			ignore_next_irq = false;
			continue;
		}

		ret = oscillator_get_temp(oscillator, &temperature);
		if (ret == -ENOSYS)
			temperature = 0;
		else if (ret < 0)
			error(EXIT_FAILURE, -ret, "oscillator_get_temp");

		/* Get GNSS Fix*/
		gnss_data = gnss_get_data(&gnss);

		/* Get Oscillator control values needed
		 * for the disciplining algorithm
		 */
		struct oscillator_ctrl ctrl_values;
		oscillator_get_ctrl(oscillator, &ctrl_values);

		input = (struct od_input) {
			.phase_error = (struct timespec) {
				.tv_sec = sign * phase_error / NS_IN_SECOND,
				.tv_nsec = sign * phase_error % NS_IN_SECOND,
			},
			.valid = gnss_data.valid,
			.lock = ctrl_values.lock,
			.temperature = temperature,
			// .qErr = gnss.data.qErr,
			.fine_setpoint = ctrl_values.fine_ctrl,
			.coarse_setpoint = ctrl_values.coarse_ctrl,
		};
		info("input: phase_error = (%lds, %09ldns),"
			"valid = %s, lock = %s, fine = %d, coarse = %d\n",
			input.phase_error.tv_sec,
			input.phase_error.tv_nsec,
			input.valid ? "true" : "false",
			input.lock ? "true" : "false",
			input.fine_setpoint,
			input.coarse_setpoint);

		/* Call disciplining algorithm process loop */
		ret = od_process(od, &input, &output);
		if (ret < 0)
			error(EXIT_FAILURE, -ret, "od_process");

		/* Process output result of the algorithm */
		if (output.action == PHASE_JUMP) {
			info("Phase jump requested\n");
			ret = apply_phase_offset(
				fd_phasemeter,
				phasemeter_device,
				-output.value_phase_ctrl
			);

			if (ret < 0)
				error(EXIT_FAILURE, -ret, "apply_phase_offset");
			ignore_next_irq = true;

		} else if (output.action == CALIBRATE) {
			info("Calibration requested\n");
			struct calibration_parameters * calib_params = od_get_calibration_parameters(od);
			if (calib_params == NULL) {
				error(EXIT_FAILURE, -ENOMEM, "od_get_calibration_parameters");
			}

			struct calibration_results *results = oscillator_calibrate(oscillator, calib_params, fd_phasemeter, sign);
			if (results == NULL) {
				error(EXIT_FAILURE, -ENOMEM, "oscillator_calibrate");
			}

			od_calibrate(od, calib_params, results);
		} else {
			ret = oscillator_apply_output(oscillator, &output);
			if (ret < 0)
				error(EXIT_FAILURE, -ret, "oscillator_apply_output");
		}

		sleep(5);
	} while (loop && turns != 1);

	pthread_mutex_lock(&gnss.mutex_data);
	gnss.stop = true;
	pthread_mutex_unlock(&gnss.mutex_data);
	pthread_join(gnss.thread, NULL);

	od_destroy(&od);

	return EXIT_SUCCESS;
}
