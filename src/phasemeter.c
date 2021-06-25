#include <errno.h>
#include <linux/ptp_clock.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/timex.h>

#include "log.h"
#include "phasemeter.h"

static int read_extts(int fd, unsigned int extts_index,
		int32_t *nsec)
{	
	struct ptp_extts_event event = {0};

	if (read(fd, &event, sizeof(event)) != sizeof(event)) {
		log_error("failed to read extts event");
		return -1;
	}

	if (event.index != extts_index) {
		log_error("extts event index %d caught looking for %d",
			event.index, extts_index);
		return -1;
	}

	if (event.t.sec < 0) {
		errno = -EINVAL;
		log_error("EXTTS second field is supposed to be positive");
		return -EINVAL;
	}

	/* Timestamp is passed as two unsigned 32 bits integers,
	 * We hack the data structure to get a signed 32 bits
	 */
	*nsec = (int32_t) event.t.sec * 1000000000 + event.t.nsec;

	return 0;
}

#define EXTTS_INDEX_PHASEMETER 0

static int enable_extts(int fd, unsigned int extts_index)
{
	struct ptp_extts_request extts_request = {
		.index = extts_index,
		.flags = PTP_RISING_EDGE | PTP_ENABLE_FEATURE
	};

	if (ioctl(fd, PTP_EXTTS_REQUEST, &extts_request) < 0) {
		log_error("PTP_EXTTS_REQUEST enable");
		return -1;
	}

	return 0;
}

static int disable_extts(int fd, unsigned int extts_index)
{
	struct ptp_extts_request extts_request = {
		.index = extts_index,
		.flags = 0
	};

	if (ioctl(fd, PTP_EXTTS_REQUEST, &extts_request) < 0) {
		log_error("PTP_EXTTS_REQUEST disable");
		return 1;
	}

	return 0;
}

static void* phasemeter_thread(void *p_data)
{
	int ret;
	int32_t phase_error;
	bool stop;
	struct phasemeter *phasemeter = (struct phasemeter *) p_data;

	stop = phasemeter->stop;

	ret = enable_extts(phasemeter->fd, EXTTS_INDEX_PHASEMETER);
	if (ret != 0) {
		log_error("Could not enable phasemeter external events");
		return NULL;
	}

	while(!stop) {
		ret = read_extts(phasemeter->fd, EXTTS_INDEX_PHASEMETER, &phase_error);
		if (ret != 0) {
			log_warn("Could not read ptp clock external timestamp for phasemeter");
			continue;
		}
		pthread_mutex_lock(&phasemeter->mutex);
		phasemeter->phase_error = phase_error;
		stop = phasemeter->stop;
		pthread_mutex_unlock(&phasemeter->mutex);
	}

	log_debug("Closing phasemeter thread");
	ret = disable_extts(phasemeter->fd, EXTTS_INDEX_PHASEMETER);
	if (ret != 0) {
		log_error("Could not disable phasemeter external events");
	}
	return NULL;
}

struct phasemeter* phasemeter_init(int fd)
{
	int ret;

	struct phasemeter *phasemeter = malloc(sizeof(struct phasemeter));
	if (phasemeter == NULL) {
		log_error("Could not allocate memory for phasemeter thread");
		return NULL;
	}
	phasemeter->fd = fd;
	phasemeter->stop = false;

	if (pthread_mutex_init(&phasemeter->mutex, NULL) != 0) {
		printf("\n mutex init failed\n");
		free(phasemeter);
		return NULL;
	}

	ret = pthread_create(
		&phasemeter->thread,
		NULL,
		phasemeter_thread,
		phasemeter
	);
	if (ret != 0) {
		log_error("Could not create phasemeter thread");
		free(phasemeter);
		return NULL;
	}

	return phasemeter;
}

void phasemeter_stop(struct phasemeter *phasemeter)
{
	pthread_mutex_lock(&phasemeter->mutex);
	phasemeter->stop = true;
	pthread_mutex_unlock(&phasemeter->mutex);
	pthread_join(phasemeter->thread, NULL);
	free(phasemeter);
	phasemeter = NULL;
	return;
}


int get_phase_error(struct phasemeter *phasemeter)
{
	int32_t phase_error;
	pthread_mutex_lock(&phasemeter->mutex);
	phase_error = phasemeter->phase_error;
	pthread_mutex_unlock(&phasemeter->mutex);
	
	return phase_error;
}
