#ifndef SRC_OSCILLATOR_FACTORY_H_
#define SRC_OSCILLATOR_FACTORY_H_
#include "oscillator.h"
#include "config.h"

struct oscillator_factory {
	const char *name;
	oscillator_new_cb new;
	oscillator_destroy_cb destroy;
};

struct oscillator *oscillator_factory_new(struct config *config);
int oscillator_factory_register(const struct oscillator_factory *factory);
void oscillator_factory_destroy(struct oscillator **oscillator);

#endif /* SRC_OSCILLATOR_FACTORY_H_ */
