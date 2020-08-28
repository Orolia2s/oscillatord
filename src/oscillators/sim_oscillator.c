#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>

#include <spi2c.h>

#include "sim_oscillator.h"

#include "config.h"
#include "log.h"
#include "utils.h"

#include "../oscillator.h"
#include "../oscillator_factory.h"

#define FACTORY_NAME "sim"
#define SIM_SETPOINT_MIN 0
#define SIM_SETPOINT_MAX 1000000
#define SIM_MAX_PTS_PATH_LEN 0x400

struct sim_oscillator {
	struct oscillator oscillator;
	FILE *simulator_process;
	int control_fifo;
	char pps_pts[SIM_MAX_PTS_PATH_LEN];
	uint32_t value;
};

static unsigned sim_oscillator_index;

static int sim_oscillator_set_dac(struct oscillator *oscillator,
		uint32_t value)
{
	struct sim_oscillator *sim;
	ssize_t sret;

	sim = container_of(oscillator, struct sim_oscillator, oscillator);

	debug("%s(%s, %" PRIu32 ")\n", __func__, oscillator->name, value);

	sret = write(sim->control_fifo, &value, sizeof(value));
	if (sret == -1)
		return -errno;
	sim->value = value;

	return 0;
}

static int sim_oscillator_get_dac(struct oscillator *oscillator,
		uint32_t *value)
{
	struct sim_oscillator *sim;

	sim = container_of(oscillator, struct sim_oscillator, oscillator);

	*value = sim->value;
	debug("%s(%s) = %" PRIu32 "\n", __func__, oscillator->name, *value);

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
	struct sim_oscillator *s;

	if (oscillator == NULL || *oscillator == NULL)
		return;

	o = *oscillator;
	s = container_of(o, struct sim_oscillator, oscillator);
	fd_cleanup(&s->control_fifo);
	if (s->simulator_process != NULL) {
		pclose(s->simulator_process);
		s->simulator_process = NULL;
	}
	memset(o, 0, sizeof(*o));
	free(o);
	*oscillator = NULL;
}

static struct oscillator *sim_oscillator_new(struct config *config)
{
	struct sim_oscillator *sim;
	int ret;
	struct oscillator *oscillator;
	const char *cret;

	__attribute__((cleanup(string_cleanup))) char *simulator_command = NULL;

	ret = asprintf(&simulator_command, "oscillator_sim %s", config->path);
	if (ret < 0) {
		err("asprintf error\n");
		errno = ENOMEM;
		return NULL;
	}

	sim = calloc(1, sizeof(*sim));
	if (sim == NULL)
		return NULL;
	oscillator = &sim->oscillator;
	sim->control_fifo = -1;

	info("launching the simulator process\n");
	unlink(CONTROL_FIFO_PATH);
	sim->simulator_process = popen(simulator_command, "re");
	if (sim->simulator_process == NULL) {
		ret = -errno;
		err("popen: %m\n");
		goto error;
	}
	info("opening fifo\n");
	do {
		sim->control_fifo = open(CONTROL_FIFO_PATH, O_WRONLY);
	} while (sim->control_fifo == -1 && errno == ENOENT);
	if (sim->control_fifo == -1) {
		ret = -errno;
		err("open(" CONTROL_FIFO_PATH "): %m\n");
		goto error;
	}
	oscillator_factory_init(FACTORY_NAME, oscillator, FACTORY_NAME "-%d",
			sim_oscillator_index);


	debug("reading pts name\n");
	cret = fgets(sim->pps_pts, SIM_MAX_PTS_PATH_LEN,
			sim->simulator_process);
	if (cret == NULL) {
		ret = -EIO;
		err("fgets error\n");
		goto error;
	}
	debug("pts name is %s\n", sim->pps_pts);

	ret = config_set(config, "pps-device", sim->pps_pts);
	if (ret < 0) {
		err("config_set: %s\n", strerror(-ret));
		goto error;
	}

	info("instantiated " FACTORY_NAME " oscillator\n");

	return oscillator;
error:
	sim_oscillator_destroy(&oscillator);
	errno = -ret;
	return NULL;
}

static const struct oscillator_factory sim_oscillator_factory = {
	.class = {
			.name = FACTORY_NAME,
			.set_dac = sim_oscillator_set_dac,
			.get_dac = sim_oscillator_get_dac,
			.save = sim_oscillator_save,
			.get_temp = sim_oscillator_get_temp,
			.dac_min = SIM_SETPOINT_MIN,
			.dac_max = SIM_SETPOINT_MAX,
	},
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
