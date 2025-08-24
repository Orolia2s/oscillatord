#include "phasemeter.h"

#include <linux/ptp_clock.h> // ptp_extts_event
#include <log.h>             // log_*

#include <sys/ioctl.h> // ioctl

#include <errno.h>  // errno
#include <iso646.h> // and not
#include <string.h> // strerror
#include <unistd.h> // read

#define NS_PER_US 1000L
#define US_PER_MS 1000
#define NS_PER_MS NS_PER_US* US_PER_MS
#define MS_PER_S  1000
#define US_PER_S  US_PER_MS* MS_PER_S
#define NS_PER_S  NS_PER_US* US_PER_S

#define ABS(x)    ((x) < 0 ? -(x) : x)

static void* phasemeter_thread(struct ART_phasemeter* self);
static bool  phasemeter_read_timestamp(int file_descriptor, struct ART_timestamp* output);
static bool  phasemeter_phase_source_switch(int file_descriptor, enum ART_phase_source source, bool on);

bool         phasemeter_thread_start(struct ART_phasemeter* self)
{
	if (not phasemeter_phase_source_switch(self->file_descriptor, self->current_reference, true))
		return false;
	if (not phasemeter_phase_source_switch(self->file_descriptor, PPS_MAC, true))
		return false;

	int status = pthread_create(&self->thread, NULL, (void* (*)(void*))phasemeter_thread, self);
	if (status != 0)
	{
		log_error("Unable to spawn phasemeter thread: %s", strerror(status));
		return false;
	}
	return true;
}

void phasemeter_thread_stop(struct ART_phasemeter* self)
{
	self->stop = true;
	pthread_join(self->thread, NULL);
	(void)phasemeter_phase_source_switch(self->file_descriptor, self->current_reference, false);
	(void)phasemeter_phase_source_switch(self->file_descriptor, PPS_MAC, false);
}

/**
 * @return
 *  - INT64_MAX in case of timeout or interruption
 *  - INT64_MIN if the source is invalid
 */
int64_t phasemeter_get_phase_offset(struct ART_phasemeter* self, enum ART_phase_source source)
{
	int64_t         result = 0;
	struct timespec limit;

	if (source >= PPS_MAC)
	{
		log_fatal("offsets are computed relatively to the MAC");
		return INT64_MIN;
	}
	time(&limit.tv_sec);
	limit.tv_sec += 2;
	limit.tv_nsec = 500 * NS_PER_MS;
	pthread_mutex_lock(&self->mutex);
	do
	{
		if (pthread_cond_timedwait(&self->new_offset[source], &self->mutex, &limit) != 0)
		{
			log_error("Timing out waiting for a phase measurement for %s", phase_source_to_cstring(source));
			result = INT64_MAX;
			goto pgpo_exit;
		}
	} while (not self->ready[source]);
	result = self->phase_offset[source];
pgpo_exit:
	pthread_mutex_unlock(&self->mutex);
	return result;
}

/**
 * @see @ref phasemeter_get_phase_offset
 */
int64_t phasemeter_get_reference_phase_offset(struct ART_phasemeter* self)
{
	enum ART_phase_source source;
	pthread_mutex_lock(&self->mutex);
	source = self->current_reference;
	pthread_mutex_unlock(&self->mutex);
	return phasemeter_get_phase_offset(self, source);
}

bool phasemeter_set_reference(struct ART_phasemeter* self, enum ART_phase_source new_reference)
{
	if (new_reference >= PPS_MAC)
	{
		log_error("Refusing to switch to an invalid reference");
		return false;
	}

	pthread_mutex_lock(&self->mutex);
	if (new_reference != self->current_reference)
	{
		(void)phasemeter_phase_source_switch(self->file_descriptor, self->current_reference, false);
		self->ready[self->current_reference]                   = false; // Those 2 lines
		self->last_timestamp[self->current_reference].received = false; // Are not really useful
		self->current_reference                                = new_reference;

		if (not phasemeter_phase_source_switch(self->file_descriptor, new_reference, true))
		{
			log_error("Unable to enable phase measurements of the desired source");
			pthread_mutex_unlock(&self->mutex);
			return false;
		}
	}
	pthread_mutex_unlock(&self->mutex);
	return true;
}

static inline int64_t _offset(struct ptp_clock_time mac, struct ptp_clock_time ext)
{
	return (ext.sec - mac.sec) * NS_PER_S + ext.nsec - mac.nsec;
}

static bool phasemeter_update_offset(struct ART_phasemeter* self, enum ART_phase_source source)
{
	if (not(self->last_timestamp[PPS_MAC].received and self->last_timestamp[source].received))
		return false;

	int64_t offset = _offset(self->last_timestamp[PPS_MAC].time, self->last_timestamp[source].time);

	if (ABS(offset) > 500 * NS_PER_MS)
	{
		self->ready[source] = false;
	}
	else
	{
		self->phase_offset[source] = offset;
		self->ready[source]        = true;
		pthread_cond_broadcast(&self->new_offset[source]);
	}
	return true;
}

static void* phasemeter_thread(struct ART_phasemeter* self)
{
	struct ART_timestamp timestamp;

	while (not self->stop)
	{
		if (phasemeter_read_timestamp(self->file_descriptor, &timestamp))
		{
			pthread_mutex_lock(&self->mutex);

			self->last_timestamp[timestamp.subject] = timestamp;
			if (timestamp.subject == PPS_MAC)
			{
				for (int source = PPS_GNSS; source < PPS_MAC; source++)
					phasemeter_update_offset(self, source);
			}
			else
				phasemeter_update_offset(self, timestamp.subject);
			pthread_mutex_unlock(&self->mutex);
		}
	}
	return NULL;
}

static bool phasemeter_phase_source_switch(int file_descriptor, enum ART_phase_source source, bool on)
{
	struct ptp_extts_request request = {
	    .index = source,
	    .flags = (on ? PTP_RISING_EDGE | PTP_ENABLE_FEATURE : 0),
	};

	if (ioctl(file_descriptor, PTP_EXTTS_REQUEST, &request) < 0)
	{
		log_error("Unable to %sable phase source '%s': %s",
		          (on ? "en" : "dis"),
		          phase_source_to_cstring(source),
		          strerror(errno));
		return false;
	}
	return true;
}

static bool phasemeter_read_timestamp(int file_descriptor, struct ART_timestamp* output)
{
	struct ptp_extts_event event;
	ssize_t                status;

	output->received = false;
	status           = read(file_descriptor, &event, sizeof(event));
	if (status < 0)
		log_error("Unable to read the phasemeter: %s", strerror(errno));
	else if (status != sizeof(event))
		log_error("Misread, USE A LOOP");
	else if (event.t.sec < 0) // PTP clock value of 0 means: 1969‑12‑31 23:59:51 TAI
		log_error("Found a negative number of seconds");
	else if (event.index >= PPS_count)
		log_error("Unknown PTP PPS index: %u", event.index);
	else
	{
		output->time     = event.t;
		output->subject  = event.index;
		output->received = true;
	}
	return output->received;
}

const char* phase_source_to_cstring(enum ART_phase_source source)
{
	switch (source) {
	case PPS_GNSS: return "GNSS";
	case PPS_SMA1: return "SMA1";
	case PPS_SMA2: return "SMA2";
	case PPS_SMA3: return "SMA3";
	case PPS_SMA4: return "SMA4";
	case PPS_MAC: return "MAC";
	default: return NULL;
	};
}

enum ART_phase_source phase_source_from_cstring(const char* string)
{
	if (strcmp(string, "GNSS") == 0) return PPS_GNSS;
	if (strcmp(string, "SMA1") == 0) return PPS_SMA1;
	if (strcmp(string, "SMA2") == 0) return PPS_SMA2;
	if (strcmp(string, "SMA3") == 0) return PPS_SMA3;
	if (strcmp(string, "SMA4") == 0) return PPS_SMA4;
	if (strcmp(string, "MAC") == 0) return PPS_MAC;
	return PPS_count;
}
