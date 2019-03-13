#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>
#include <fcntl.h>

#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>

#include <spi2c.h>

#include "../oscillator.h"
#include "../oscillator_factory.h"
#include "../config.h"
#include "../log.h"
#include "../utils.h"

#define FACTORY_NAME "sim"
#define SIM_CMD_SET_DAC 0xc0
#define SIM_CMD_GET_DAC 0xc1
#define SIM_CMD_SAVE 0xc2
#define SIM_SETPOINT_MIN 31500
#define SIM_SETPOINT_MAX 1016052

struct sim_oscillator {
	struct oscillator oscillator;
	int control_fifo;
	unsigned value;
};

static unsigned sim_oscillator_index;

static int sim_oscillator_set_dac(struct oscillator *oscillator,
		unsigned value)
{
	struct sim_oscillator *sim;
	ssize_t sret;
	bool skipped;

	if (value < SIM_SETPOINT_MIN || value > SIM_SETPOINT_MAX) {
		warn("dac value %u ignored, not in [%d, %d]\n", value,
				SIM_SETPOINT_MIN, SIM_SETPOINT_MAX);
		return 0;
	}

	sim = container_of(oscillator, struct sim_oscillator, oscillator);

	skipped = value == sim->value;
	debug("%s(%s, %u)%s\n", __func__, oscillator->name, value,
			skipped ? " skipped" : "");
	if (skipped)
		return 0;

	sret = write(sim->control_fifo, &value, sizeof(value));
	if (sret == -1)
		return -errno;
	sim->value = value;


	return 0;
}

static int sim_oscillator_get_dac(struct oscillator *oscillator,
		unsigned *value)
{
	struct sim_oscillator *sim;

	sim = container_of(oscillator, struct sim_oscillator, oscillator);

	*value = sim->value;
	debug("%s(%s) = %u\n", __func__, oscillator->name, *value);

	return 0;
}

static int sim_oscillator_save(struct oscillator *oscillator)
{
	debug("%s(%s)\n", __func__, oscillator->name);

	return -ENOSYS;
}

static int sim_oscillator_get_temp(struct oscillator *oscillator,
		uint16_t *temp)
{
	*temp = (rand() % (55 - 10)) + 10;

	info("%s(%p, %u)\n", __func__, oscillator, *temp);

	return 0;
}

static void sim_oscillator_destroy(struct oscillator **oscillator)
{
	struct oscillator *o;
	struct sim_oscillator *r;

	if (oscillator == NULL || *oscillator == NULL)
		return;

	o = *oscillator;
	r = container_of(o, struct sim_oscillator, oscillator);
	fd_cleanup(&r->control_fifo);
	memset(o, 0, sizeof(*o));
	free(o);
	*oscillator = NULL;
}

static struct oscillator *sim_oscillator_new(struct config *config)
{
	struct sim_oscillator *sim;
	int ret;
	struct oscillator *oscillator;
	const char *control_fifo;

	sim = calloc(1, sizeof(*sim));
	if (sim == NULL)
		return NULL;
	oscillator = &sim->oscillator;
	sim->control_fifo = -1;

	control_fifo = config_get(config, "control-fifo");
	if (control_fifo == NULL) {
		ret = -errno;
		err("control-fifo config key must be provided\n");
		goto error;
	}
	info("opening fifo %s\n", control_fifo);
	sim->control_fifo = open(control_fifo, O_WRONLY);
	if (sim->control_fifo == -1) {
		ret = -errno;
		err("open(%s): %m", control_fifo);
		goto error;
	}
	snprintf(oscillator->name, OSCILLATOR_NAME_LENGTH, FACTORY_NAME "-%d",
			sim_oscillator_index);
	oscillator->set_dac = sim_oscillator_set_dac;
	oscillator->get_dac = sim_oscillator_get_dac;
	oscillator->save = sim_oscillator_save;
	oscillator->get_temp = sim_oscillator_get_temp;
	oscillator->factory_name = FACTORY_NAME;

	info("instantiated " FACTORY_NAME " oscillator, control fifo: %s\n",
			control_fifo);

	return oscillator;
error:
	sim_oscillator_destroy(&oscillator);
	errno = -ret;
	return NULL;
}

static const struct oscillator_factory sim_oscillator_factory = {
	.name = FACTORY_NAME,
	.new = sim_oscillator_new,
	.destroy = sim_oscillator_destroy,
};

static void __attribute__((constructor)) sim_oscillator_constructor(void)
{
	int ret;

	ret = oscillator_factory_register(&sim_oscillator_factory);
	if (ret < 0)
		perr("oscillator_factory_register", ret);
}
