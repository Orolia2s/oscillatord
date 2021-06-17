#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>

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

static volatile int loop = true;

/*
 * Signal Handler to kill program gracefully
 */
static void signal_handler(int signum)
{
	log_info("Caught signal %s.", strsignal(signum));
	if (!loop) {
		log_error("Signalled twice, brutal exit.");
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
		log_error("Can't write %s", device_name);
		return -errno;
	}
	log_info("%s: applied a phase offset correction of %"PRIi32"ns",
			device_name, phase_error);

	return ret;
}

static int get_phase_error(int fd_phasemeter, int32_t *phase_error)
{
	fd_set rfds;
	struct timeval tv;
	int ret;

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
				return -1;
			error(EXIT_FAILURE, errno, "select");
	}

	/* Read phase error from phasemeter */
	ret = read(fd_phasemeter, phase_error, sizeof(*phase_error));
	if (ret == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return -1;
		error(EXIT_FAILURE, errno, "read");
	}

	return 0;
}

static int init_ptp_clock_time(const char* ptp_clock)
{
	__attribute__((cleanup(fd_cleanup))) int fd_clock = -1;
	clockid_t clkid;
	struct timespec ts;
	int ret;
	bool clock_set = false;
	
	fd_clock = open(ptp_clock, O_RDWR);
	if (fd_clock < 0) {
		log_warn("Could not open ptp clock fd");
		return -1;
	}
	
	log_info("Initialize time of ptp clock %s", ptp_clock);
	clkid = FD_TO_CLOCKID(fd_clock);
	while(!clock_set) {
		struct gnss_data data = gnss_get_data(&gnss);
		if (data.valid) {
			/* Configure PHC time */
			/* First get clock time to preserve nanoseconds */
			ret = clock_gettime(clkid, &ts);
			if (ret == 0) {
				ts.tv_sec = data.time;

				ret = clock_settime(clkid, &ts);
				if (ret == 0)
					clock_set = true;
			}
		} else {
			sleep(2);
		}
	}
	close(fd_clock);
	log_info("PTP clock time set");
	return 0;
}

int main(int argc, char *argv[])
{
	struct od_input input;
	struct od_output output;
	struct config config;
	struct gnss_data gnss_data;
	const char *phasemeter_device;
	const char *ptp_clock;
	const char *path;
	const char *libod_conf_path;
	const char *value;
	char err_msg[OD_ERR_MSG_LEN];
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
	__attribute__((cleanup(oscillator_factory_destroy)))
			struct oscillator *oscillator = NULL;

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
	log_set_level(
		config_get_bool_default(&config, "debug", false) ?
		LOG_DEBUG :
		LOG_INFO
	);

	value = config_get(&config, "turns");
	turns = (value != NULL) ? atoll(value) : 0;

	oscillator = oscillator_factory_new(&config);
	if (oscillator == NULL)
		error(EXIT_FAILURE, errno, "oscillator_factory_new");
	log_info("oscillator model %s", oscillator->class->name);

	phasemeter_device = config_get(&config, "phasemeter-device");
	if (phasemeter_device == NULL) {
		error(EXIT_FAILURE, errno, "pps-device not defined in "
				"config %s", path);
		return -EINVAL;
	}
	log_info("PPS device %s", phasemeter_device);

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

	ret = get_phase_error(fd_phasemeter, &phase_error);
	if (ret != 0) {
		log_error("Could not get phase error for initial phase jump");
	} else {
		log_info("Applying initial phase jump before setting PTP clock time");
		ret = apply_phase_offset(
			fd_phasemeter,
			phasemeter_device,
			-phase_error
		);

		if (ret < 0)
			error(EXIT_FAILURE, -ret, "apply_phase_offset");
	}
	sleep(SETTLING_TIME);
	ret = init_ptp_clock_time(ptp_clock);
	if (ret != 0)
		log_error("Could not set ptp clock time");

	/* Main Loop */
	do {
		turns--;

		// Get Phase error
		ret = get_phase_error(fd_phasemeter, &phase_error);
		if (ret != 0)
			continue;

		if (ignore_next_irq) {
			log_debug("ignoring 1 input due to phase jump");
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
		log_info("input: phase_error = (%lds, %09ldns),"
			"valid = %s, lock = %s, fine = %d, coarse = %d",
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
			log_info("Phase jump requested");
			ret = apply_phase_offset(
				fd_phasemeter,
				phasemeter_device,
				-output.value_phase_ctrl
			);

			if (ret < 0)
				error(EXIT_FAILURE, -ret, "apply_phase_offset");
			ignore_next_irq = true;

		} else if (output.action == CALIBRATE) {
			log_info("Calibration requested");
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

		sleep(SETTLING_TIME);
	} while (loop && turns != 1);

	pthread_mutex_lock(&gnss.mutex_data);
	gnss.stop = true;
	pthread_mutex_unlock(&gnss.mutex_data);
	pthread_join(gnss.thread, NULL);

	od_destroy(&od);

	return EXIT_SUCCESS;
}
