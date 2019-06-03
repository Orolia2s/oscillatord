#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include "config.h"
#include "log.h"

#include "../oscillator.h"
#include "../oscillator_factory.h"

#define FACTORY_NAME "dummy"
#define DUMMY_SETPOINT_MAX UINT_MAX

static unsigned dummy_oscillator_index;

static int dummy_oscillator_set_dac(struct oscillator *oscillator,
		uint32_t value)
{
	info("%s(%p, %" PRIu32 ")\n", __func__, oscillator, value);

	return 0;
}

static int dummy_oscillator_get_dac(struct oscillator *oscillator,
		uint32_t *value)
{
	*value = (rand() % (OSCILLATOR_DAC_MAX - OSCILLATOR_DAC_MIN)) +
			OSCILLATOR_DAC_MIN;

	info("%s(%p, %" PRIu32 ")\n", __func__, oscillator, *value);

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

	oscillator_factory_init(FACTORY_NAME, oscillator, FACTORY_NAME "-%d",
			dummy_oscillator_index);
	dummy_oscillator_index++;

	return oscillator;
}

static void dummy_oscillator_destroy(struct oscillator **oscillator)
{
	memset(*oscillator, 0, sizeof(**oscillator));
	free(*oscillator);
	*oscillator = NULL;
}

static const struct oscillator_factory dummy_oscillator_factory = {
	.class = {
			.name = FACTORY_NAME,
			.set_dac = dummy_oscillator_set_dac,
			.get_dac = dummy_oscillator_get_dac,
			.save = dummy_oscillator_save,
			.get_temp = dummy_oscillator_get_temp,
			.dac_max = DUMMY_SETPOINT_MAX,
	},
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
