#ifndef SRC_OSCILLATOR_H_
#define SRC_OSCILLATOR_H_
#include <inttypes.h>

#include "config.h"

#ifndef OSCILLATOR_NAME_LENGTH
#define OSCILLATOR_NAME_LENGTH 50
#endif

#define OSCILLATOR_DAC_MIN 31500
#define OSCILLATOR_DAC_MAX 1016052

struct oscillator;

typedef struct oscillator *(*oscillator_new_cb)(struct config *config);
typedef int (*oscillator_set_dac_cb)(struct oscillator *oscillator,
		unsigned value);
typedef int (*oscillator_get_dac_cb)(struct oscillator *oscillator,
		unsigned *value);
typedef int (*oscillator_save_cb)(struct oscillator *oscillator);
typedef int (*oscillator_get_temp_cb)(struct oscillator *oscillator,
		uint16_t *temp);
typedef void (*oscillator_destroy_cb)(struct oscillator **oscillator);

struct oscillator {
	char name[OSCILLATOR_NAME_LENGTH];
	oscillator_set_dac_cb set_dac;
	oscillator_get_dac_cb get_dac;
	oscillator_save_cb save;
	oscillator_get_temp_cb get_temp;
	const char *factory_name;
};

int oscillator_set_dac(struct oscillator *oscillator, unsigned value);
int oscillator_get_dac(struct oscillator *oscillator, unsigned *value);
int oscillator_save(struct oscillator *oscillator);
int oscillator_get_temp(struct oscillator *oscillator, uint16_t *temp);

#endif /* SRC_OSCILLATOR_H_ */
