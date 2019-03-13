#include <stdlib.h>
#include <errno.h>

#include "../oscillator.h"
#include "../oscillator_factory.h"
#include "../config.h"
#include "../log.h"

#define FACTORY_NAME "dummy"

static unsigned dummy_oscillator_index;

static int dummy_oscillator_set_dac(struct oscillator *oscillator,
		unsigned value)
{
	info("%s(%p, %u)\n", __func__, oscillator, value);

	return 0;
}

static int dummy_oscillator_get_dac(struct oscillator *oscillator,
		unsigned *value)
{
	*value = (rand() % (OSCILLATOR_DAC_MAX - OSCILLATOR_DAC_MIN)) +
			OSCILLATOR_DAC_MIN;

	info("%s(%p, %u)\n", __func__, oscillator, *value);

	return 0;
}

static int dummy_oscillator_save(struct oscillator *oscillator)
{
	info("%s(%p)\n", __func__, oscillator);

	return 0;
}

static int dummy_oscillator_get_temp(struct oscillator *oscillator,
		uint16_t *temp)
{
	*temp = (rand() % (55 - 10)) + 10;

	info("%s(%p, %u)\n", __func__, oscillator, *temp);

	return 0;
}

static struct oscillator *dummy_oscillator_new(struct config *config)
{
	struct oscillator *oscillator;

	oscillator = calloc(1, sizeof(*oscillator));
	if (oscillator == NULL)
		return NULL;

	snprintf(oscillator->name, OSCILLATOR_NAME_LENGTH, FACTORY_NAME "-%d",
			dummy_oscillator_index);
	dummy_oscillator_index++;
	oscillator->set_dac = dummy_oscillator_set_dac;
	oscillator->get_dac = dummy_oscillator_get_dac;
	oscillator->save = dummy_oscillator_save;
	oscillator->get_temp = dummy_oscillator_get_temp;
	oscillator->factory_name = FACTORY_NAME;

	return oscillator;
}

static void dummy_oscillator_destroy(struct oscillator **oscillator)
{
	memset(*oscillator, 0, sizeof(**oscillator));
	free(*oscillator);
	*oscillator = NULL;
}

static const struct oscillator_factory dummy_oscillator_factory = {
	.name = FACTORY_NAME,
	.new = dummy_oscillator_new,
	.destroy = dummy_oscillator_destroy,
};

static void __attribute__((constructor)) dummy_oscillator_constructor(void)
{
	int ret;

	ret = oscillator_factory_register(&dummy_oscillator_factory);
	if (ret < 0)
		perr("oscillator_factory_register", ret);
}
