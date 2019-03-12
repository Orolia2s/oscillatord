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

#define FACTORY_NAME "rakon"
#define RAKON_CMD_SET_DAC 0xc0
#define RAKON_CMD_GET_DAC 0xc1
#define RAKON_CMD_SAVE 0xc2
#define RAKON_SETPOINT_MIN 31500
#define RAKON_SETPOINT_MAX 1016052

struct rakon_oscillator {
	struct oscillator oscillator;
	struct i2cdev *i2c;
	unsigned value;
};

static unsigned rakon_oscillator_index;

static int rakon_oscillator_set_dac(struct oscillator *oscillator,
		unsigned value)
{
	uint8_t buf[4];
	struct rakon_oscillator *rakon;
	int ret;
	bool skipped;

	if (value < RAKON_SETPOINT_MIN || value > RAKON_SETPOINT_MAX) {
		warn("dac value %u ignored, not in [%d, %d]\n", value,
				RAKON_SETPOINT_MIN, RAKON_SETPOINT_MAX);
		return 0;
	}

	rakon = container_of(oscillator, struct rakon_oscillator, oscillator);

	skipped = value == rakon->value;
	debug("%s(%s, %u)%s\n", __func__, oscillator->name, value,
			skipped ? " skipped" : "");
	if (skipped)
		return 0;

	buf[0] = RAKON_CMD_SET_DAC;
	buf[1] = (value & 0x0F0000) >> 16;
	buf[2] = (value & 0xFF00) >> 8;
	buf[3] = value & 0xFF;
	ret = i2c_transfer(rakon->i2c, buf, sizeof(buf), NULL, 0);
	if (ret != 0) {
		ret = -errno;
		err("failed i2c transfer : %m\n");
		return ret;
	}
	rakon->value = value;

	return 0;
}

static int rakon_oscillator_get_dac(struct oscillator *oscillator,
		unsigned *value)
{
	uint8_t tx_val;
	uint8_t buf[3];
	struct rakon_oscillator *rakon;
	int ret;

	rakon = container_of(oscillator, struct rakon_oscillator, oscillator);

	tx_val = RAKON_CMD_GET_DAC;
	ret = i2c_transfer(rakon->i2c, &tx_val, sizeof(tx_val), buf,
			sizeof(buf));
	if (ret != 0) {
		ret = -errno;
		err("failed i2c transfer : %m\n");
		return ret;
	}

	*value = (buf[0] & 0x0F) << 16 | buf[1] << 8 | buf[2];

	debug("%s(%s) = %u\n", __func__, oscillator->name, *value);

	return 0;
}

static int rakon_oscillator_save(struct oscillator *oscillator)
{
	uint8_t tx_val;
	struct rakon_oscillator *rakon;
	int ret;

	debug("%s(%s)\n", __func__, oscillator->name);

	rakon = container_of(oscillator, struct rakon_oscillator, oscillator);

	tx_val = RAKON_CMD_SAVE;
	ret = i2c_transfer(rakon->i2c, &tx_val, sizeof(tx_val), NULL, 0);
	if (ret != 0) {
		ret = -errno;
		err("failed i2c transfer : %m\n");
		return ret;
	}

	return 0;
}

static int rakon_oscillator_get_temp(struct oscillator *oscillator,
		uint16_t *temp)
{
	struct rakon_oscillator *rakon;
	int ret;

	rakon = container_of(oscillator, struct rakon_oscillator, oscillator);

	*temp = 0;
	ret = i2c_transfer(rakon->i2c, NULL, 0, (uint8_t *)temp, sizeof(*temp));
	if (ret != 0) {
		ret = -errno;
		err("failed i2c transfer : %m\n");
		return ret;
	}

	*temp = be16toh(*temp);
	debug("%s(%s) = %"PRIu16"\n", __func__, oscillator->name, *temp);

	return 0;
}

static void rakon_oscillator_destroy(struct oscillator **oscillator)
{
	struct oscillator *o;
	struct rakon_oscillator *r;

	if (oscillator == NULL || *oscillator == NULL)
		return;

	o = *oscillator;
	r = container_of(o, struct rakon_oscillator, oscillator);
	i2c_delete(r->i2c);
	memset(o, 0, sizeof(*o));
	free(o);
	*oscillator = NULL;
}

static struct oscillator *rakon_oscillator_new(const struct config *config)
{
	struct rakon_oscillator *rakon;
	int ret;
	uint8_t i2c_num;
	uint8_t i2c_addr;
	struct oscillator *oscillator;

	rakon = calloc(1, sizeof(*rakon));
	if (rakon == NULL)
		return NULL;
	oscillator = &rakon->oscillator;

	ret = config_get_uint8_t(config, "rakon-i2c-num");
	if (ret < 0) {
		err("rakon-i2c-num config key must be provided\n");
		goto error;
	}
	i2c_num = ret;
	ret = config_get_uint8_t(config, "rakon-i2c-addr");
	if (ret < 0) {
		err("rakon-i2c-addr config key must be provided\n");
		goto error;
	}
	i2c_addr = ret;

	rakon->i2c = i2c_new(i2c_num, i2c_addr);
	if (rakon->i2c == NULL) {
		ret = -errno;
		err("i2c_new: %m\n");
		goto error;
	}

	snprintf(oscillator->name, OSCILLATOR_NAME_LENGTH, FACTORY_NAME "-%d",
			rakon_oscillator_index);
	oscillator->set_dac = rakon_oscillator_set_dac;
	oscillator->get_dac = rakon_oscillator_get_dac;
	oscillator->save = rakon_oscillator_save;
	oscillator->get_temp = rakon_oscillator_get_temp;
	oscillator->factory_name = FACTORY_NAME;

	info("instantiated " FACTORY_NAME " oscillator on i2c number %" PRIu8
			" and i2c address %#" PRIx8 "\n", i2c_num, i2c_addr);

	return oscillator;
error:
	rakon_oscillator_destroy(&oscillator);
	errno = -ret;
	return NULL;
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
