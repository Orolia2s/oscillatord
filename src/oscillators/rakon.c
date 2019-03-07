#include <stdlib.h>
#include <errno.h>

#include <spi2c.h>

#include "../oscillator.h"
#include "../oscillator_factory.h"
#include "../config.h"
#include "../log.h"

#define FACTORY_NAME "rakon"

static unsigned rakon_oscillator_index;

static int rakon_oscillator_set_dac(struct oscillator *oscillator,
		unsigned value)
{
	info("%s(%p, %u)\n", __func__, oscillator, value);

	return 0;
}

static int rakon_oscillator_get_dac(struct oscillator *oscillator,
		unsigned *value)
{
	info("%s(%p, %p)\n", __func__, oscillator, value);

	return 0;
}

static int rakon_oscillator_save(struct oscillator *oscillator)
{
	info("%s(%p)\n", __func__, oscillator);

	return 0;
}

static int rakon_oscillator_get_temp(struct oscillator *oscillator,
		uint16_t *temp)
{
	info("%s(%p, %p)\n", __func__, oscillator, temp);

	return 0;
}

static struct oscillator *rakon_oscillator_new(const struct config *config)
{
	struct oscillator *oscillator;

	oscillator = calloc(1, sizeof(*oscillator));
	if (oscillator == NULL)
		return NULL;

	snprintf(oscillator->name, OSCILLATOR_NAME_LENGTH, FACTORY_NAME "-%d",
			rakon_oscillator_index);
	rakon_oscillator_index++;
	oscillator->set_dac = rakon_oscillator_set_dac;
	oscillator->get_dac = rakon_oscillator_get_dac;
	oscillator->save = rakon_oscillator_save;
	oscillator->get_temp = rakon_oscillator_get_temp;
	oscillator->factory_name = FACTORY_NAME;

	return oscillator;
}

static void rakon_oscillator_destroy(struct oscillator **oscillator)
{
	memset(*oscillator, 0, sizeof(**oscillator));
	free(*oscillator);
	*oscillator = NULL;
}

static const struct oscillator_factory rakon_oscillator_factory = {
	.name = FACTORY_NAME,
	.new = rakon_oscillator_new,
	.destroy = rakon_oscillator_destroy,
};

static void __attribute__((constructor)) rakon_oscillator_constructor(void)
{
	int ret;

	ret = oscillator_factory_register(&rakon_oscillator_factory);
	if (ret < 0)
		perr("oscillator_factory_register", ret);
}
