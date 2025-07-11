#pragma once

#include <linux/ptp_clock.h> // ptp_clock_time
#include <pthread.h>         // pthread_*

#include <stdbool.h> // bool
#include <stdint.h>  // int64_t

enum ART_phase_source {
	PPS_GNSS,
	PPS_SMA1,
	PPS_SMA2,
	PPS_SMA3,
	PPS_SMA4,
	PPS_MAC,
	PPS_count
};

struct ART_timestamp
{
	struct ptp_clock_time time;
	enum ART_phase_source subject;
	bool                  received;
};

struct ART_phasemeter
{
	pthread_t             thread;
	pthread_mutex_t       mutex;
	struct ART_timestamp  last_timestamp[PPS_count];
	int64_t               phase_offset[PPS_MAC];
	bool                  ready[PPS_MAC];
	pthread_cond_t        new_offset[PPS_MAC];
	int                   file_descriptor;
	enum ART_phase_source current_reference;
	_Atomic(bool)         stop;
};

#define phasemeter_init(FD, REFERENCE) \
	(struct ART_phasemeter){ \
	    .file_descriptor   = FD, \
	    .mutex             = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP, \
	    .next_second       = {[0 ... PPS_count] = PTHREAD_COND_INITIALIZER}, \
	    .current_reference = REFERENCE, \
	};

bool        phasemeter_thread_start(struct ART_phasemeter* self);
void        phasemeter_thread_stop(struct ART_phasemeter* self);
int64_t     phasemeter_get_phase_offset(struct ART_phasemeter* self, enum ART_phase_source source);
const char* phase_source_to_cstring(enum ART_phase_source source);
