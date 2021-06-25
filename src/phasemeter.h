#ifndef OSCILLATORD_PHASEMETER_H
#define OSCILLATORD_PHASEMETER_H

#include <pthread.h>
#include <stdint.h>

struct phasemeter {
	pthread_t thread;
	pthread_mutex_t mutex;
	int32_t phase_error;
	int fd;
	bool stop;
};

struct phasemeter* phasemeter_init(int fd);
void phasemeter_stop(struct phasemeter *phasemeter);
int get_phase_error(struct phasemeter *phasemeter);

#endif /* OSCILLATORD_PHASEMETER_H */
