#ifndef SRC_OSCILLATOR_H_
#define SRC_OSCILLATOR_H_
#include <inttypes.h>

#include "config.h"

#ifndef OSCILLATOR_NAME_LENGTH
#define OSCILLATOR_NAME_LENGTH 50
#endif

struct oscillator;

typedef struct oscillator *(*oscillator_new_cb)(struct config *config);
typedef int (*oscillator_set_dac_cb)(struct oscillator *oscillator,
		uint32_t value);
typedef int (*oscillator_get_dac_cb)(struct oscillator *oscillator,
		uint32_t *value);
typedef int (*oscillator_save_cb)(struct oscillator *oscillator);
typedef int (*oscillator_get_temp_cb)(struct oscillator *oscillator,
		uint16_t *temp);
typedef void (*oscillator_destroy_cb)(struct oscillator **oscillator);

struct oscillator_class {
	const char *name;
	oscillator_set_dac_cb set_dac;
	oscillator_get_dac_cb get_dac;
	oscillator_save_cb save;
	oscillator_get_temp_cb get_temp;
	/* default values use if per-instance ones haven't been set */
	uint32_t dac_max;
	uint32_t dac_min;
};

struct oscillator {
	char name[OSCILLATOR_NAME_LENGTH];
	const struct oscillator_class *class;
	/* UINT32_MAX if not specified */
	uint32_t dac_min;
	/* 0 if not specified */
	uint32_t dac_max;
};

int oscillator_set_dac_min(struct oscillator *oscillator, uint32_t dac_min);
int oscillator_set_dac_max(struct oscillator *oscillator, uint32_t dac_max);
int oscillator_set_dac(struct oscillator *oscillator, uint32_t value);
int oscillator_get_dac(struct oscillator *oscillator, uint32_t *value);
int oscillator_save(struct oscillator *oscillator);
int oscillator_get_temp(struct oscillator *oscillator, uint16_t *temp);

#endif /* SRC_OSCILLATOR_H_ */
