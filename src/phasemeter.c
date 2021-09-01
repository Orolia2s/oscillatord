/*
 * Phasemeter computing phase difference between two PPS
 * Both PPS's timestamps are received through a PTP clock event
 */
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

#define EXTTS_INDEX_ART_INTERNAL_PPS 0
#define EXTTS_INDEX_GNSS_PPS 1

#define MILLISECONDS_500 500000000

struct external_timestamp {
	int64_t timestamp; // ns
	int index;
};

/* Return index of external timestamp received on success, -1 on error */
static int read_extts(int fd, int64_t *nsec)
{	
	struct ptp_extts_event event = {0};

	if (read(fd, &event, sizeof(event)) != sizeof(event)) {
		log_error("failed to read extts event");
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
	*nsec = (int64_t) event.t.sec * 1000000000ULL + event.t.nsec;
	log_trace(
		"%s timestamp: %llu",
		event.index == 1? "Internal" : "GNSS    ",
		*nsec);

	return event.index;
}

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
	bool stop;
	struct phasemeter *phasemeter = (struct phasemeter *) p_data;

	stop = phasemeter->stop;

	ret = enable_extts(phasemeter->fd, EXTTS_INDEX_ART_INTERNAL_PPS);
	if (ret != 0) {
		log_error("Could not enable ART internal pps external events");
		return NULL;
	}
	ret = enable_extts(phasemeter->fd, EXTTS_INDEX_GNSS_PPS);
	if (ret != 0) {
		log_error("Could not enable GNSS pps external events");
		return NULL;
	}

	while(!stop) {
		/* Get first timestamp */
		struct external_timestamp ts1;
		do {
			ts1.index = read_extts(phasemeter->fd, &ts1.timestamp);
			if (ts1.index < 0) {
				log_warn("Could not read ptp clock external timestamp for phasemeter");
			}
		} while (ts1.index != EXTTS_INDEX_ART_INTERNAL_PPS && ts1.index != EXTTS_INDEX_GNSS_PPS);

		/* Get Second timestamp */
		struct external_timestamp ts2;
		do {
			ts2.index = read_extts(phasemeter->fd, &ts2.timestamp);
			if (ts2.index < 0) {
				log_warn("Could not read ptp clock external timestamp for phasemeter");
			}
		} while (ts2.index != EXTTS_INDEX_ART_INTERNAL_PPS && ts2.index != EXTTS_INDEX_GNSS_PPS);

		/*
		 * Did not received GNSS PPS external event
		 * GNSS receiver PPS output can be deactivated if GNSS is not locked
		 */
		if (ts1.index == EXTTS_INDEX_ART_INTERNAL_PPS && ts1.index == ts2.index) {
			log_warn("Phasemeter: Did not receive GNSS pps event");
			pthread_mutex_lock(&phasemeter->mutex);
			phasemeter->status = PHASEMETER_NO_GNSS_TIMESTAMPS;
			stop = phasemeter->stop;
			pthread_mutex_unlock(&phasemeter->mutex);
		/*
		 * Did not received ART Internal PPS event
		 * This case should not happen
		 */
		} else if (ts1.index == EXTTS_INDEX_GNSS_PPS && ts1.index == ts2.index) {
			log_warn("Phasemeter: Did not receive ART internal pps event");
			pthread_mutex_lock(&phasemeter->mutex);
			phasemeter->status = PHASEMETER_NO_ART_INTERNAL_TIMESTAMPS;
			stop = phasemeter->stop;
			pthread_mutex_unlock(&phasemeter->mutex);

		/*
		 * One timestamp comes from GNSS receiver and the one come froms ART Internal PPS
		 */
		} else {
			log_trace("Timestamp 1: type %s, ts %lld", (ts1.index == EXTTS_INDEX_GNSS_PPS)? "GNSS" : "INT ", ts1.timestamp);
			log_trace("Timestamp 2: type %s, ts %lld", (ts2.index == EXTTS_INDEX_GNSS_PPS)? "GNSS" : "INT ", ts2.timestamp);
			int64_t timestamp_diff = ts2.timestamp - ts1.timestamp;
			timestamp_diff = (ts1.index == EXTTS_INDEX_GNSS_PPS) ? -timestamp_diff : timestamp_diff;
			/*
			 * Phase error is superior to 500ms
			 * We should get another external timestamp to compute phase error
			 */
			if (timestamp_diff > MILLISECONDS_500 && timestamp_diff < -MILLISECONDS_500) {
				log_warn("Diff is sup to 500 ms, getting a third timestamp");
				struct external_timestamp ts3;
				do {
					ts3.index = read_extts(phasemeter->fd, &ts3.timestamp);
					if (ts3.index < 0) {
						log_warn("Could not read ptp clock external timestamp for phasemeter");
					}
				} while (ts3.index != EXTTS_INDEX_ART_INTERNAL_PPS && ts3.index != EXTTS_INDEX_GNSS_PPS);
				if (ts3.index == ts2.index) {
					log_warn("Got 2 external events of the same index");
					pthread_mutex_lock(&phasemeter->mutex);
					phasemeter->status = PHASEMETER_ERROR;
					stop = phasemeter->stop;
					pthread_mutex_unlock(&phasemeter->mutex);
					continue;
				}
				log_trace("Timestamp 3: type %s, ts %lld", (ts3.index == EXTTS_INDEX_GNSS_PPS)? "GNSS" : "INT ", ts3.timestamp);
				timestamp_diff = ts3.timestamp - ts2.timestamp;
				timestamp_diff = (ts2.index == EXTTS_INDEX_GNSS_PPS) ? -timestamp_diff : timestamp_diff;
				if (timestamp_diff > MILLISECONDS_500 && timestamp_diff < -MILLISECONDS_500) {
					log_warn("Could not get timestamp diff inferior to 500ms, restarting");
					pthread_mutex_lock(&phasemeter->mutex);
					phasemeter->status = PHASEMETER_ERROR;
					stop = phasemeter->stop;
					pthread_mutex_unlock(&phasemeter->mutex);
					continue;
				}
			}
			int32_t phase_error = (int32_t) timestamp_diff;
			log_debug("Phasemeter: phase_error: %ldns", phase_error);
			pthread_mutex_lock(&phasemeter->mutex);
			phasemeter->status = PHASEMETER_BOTH_TIMESTAMPS;
			phasemeter->phase_error = phase_error;
			stop = phasemeter->stop;
			pthread_mutex_unlock(&phasemeter->mutex);
		}
	}

	log_info("Closing phasemeter thread");
	ret = disable_extts(phasemeter->fd, EXTTS_INDEX_ART_INTERNAL_PPS);
	if (ret != 0) {
		log_error("Could not disable ART internal pps external events");
	}
	ret = disable_extts(phasemeter->fd, EXTTS_INDEX_GNSS_PPS);
	if (ret != 0) {
		log_error("Could not disable GNSS pps external events");
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
	phasemeter->status = PHASEMETER_INIT;

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


int get_phase_error(struct phasemeter *phasemeter, int32_t *phase_error)
{
	int status;
	pthread_mutex_lock(&phasemeter->mutex);
	*phase_error = phasemeter->phase_error;
	status = phasemeter->status;
	pthread_mutex_unlock(&phasemeter->mutex);
	
	return status;
}
