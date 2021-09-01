#ifndef OSCILLATORD_PHASEMETER_H
#define OSCILLATORD_PHASEMETER_H

#include <pthread.h>
#include <stdint.h>

enum PHASEMETER_STATUS {
	PHASEMETER_INIT,
	PHASEMETER_NO_GNSS_TIMESTAMPS,
	PHASEMETER_NO_ART_INTERNAL_TIMESTAMPS,
	PHASEMETER_BOTH_TIMESTAMPS,
	PHASEMETER_ERROR
};

struct phasemeter {
	pthread_t thread;
	pthread_mutex_t mutex;
	int32_t phase_error;
	int status;
	int fd;
	bool stop;
};

struct phasemeter* phasemeter_init(int fd);
void phasemeter_stop(struct phasemeter *phasemeter);
int get_phase_error(struct phasemeter *phasemeter, int64_t *phase_error);

#endif /* OSCILLATORD_PHASEMETER_H */
