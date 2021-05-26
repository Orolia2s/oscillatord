#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>

#include <spi2c.h>

#include "config.h"
#include "log.h"
#include "utils.h"

#include "../oscillator.h"
#include "../oscillator_factory.h"

#define FACTORY_NAME "rakon"
#define RAKON_CMD_READ_TEMP 0x3e
#define RAKON_CMD_GET_DAC 0x41
#define RAKON_CMD_PROD_ID 0x50
#define RAKON_CMD_READ_FW_REV 0x51
#define RAKON_CMD_SET_DAC 0xA0
#define RAKON_CMD_SAVE 0xc2
#define RAKON_SETPOINT_MIN 0
#define RAKON_SETPOINT_MAX 1000000

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

struct rakon_oscillator {
	struct oscillator oscillator;
	struct i2cdev *i2c;
};

struct rakon_payload {
	uint8_t cmd;
	uint32_t value;
} __packed;

static unsigned int rakon_oscillator_index;

static int rakon_oscillator_set_dac(struct oscillator *oscillator,
		uint32_t value)
{
	struct rakon_oscillator *rakon;
	struct rakon_payload buf = {0};
	int ret;
	uint32_t val_be;

	rakon = container_of(oscillator, struct rakon_oscillator, oscillator);

	log_debug("%s(%s, %" PRIu32 ")\n", __func__, oscillator->name, value);

	buf.cmd = RAKON_CMD_SET_DAC;
	val_be = htobe32(value);
	memcpy(&buf.value, &val_be, sizeof(buf.value));
	ret = i2c_transfer(rakon->i2c, (uint8_t *)&buf, sizeof(buf), NULL, 0);
	if (ret != 0) {
		ret = -errno;
		log_error("failed i2c transfer : %m");
		return ret;
	}

	return 0;
}

static int rakon_oscillator_get_dac(struct oscillator *oscillator,
		uint32_t *value)
{
	uint8_t tx_val;
	struct rakon_oscillator *rakon;
	int ret;

	rakon = container_of(oscillator, struct rakon_oscillator, oscillator);

	*value = 0;
	tx_val = RAKON_CMD_GET_DAC;
	ret = i2c_transfer(rakon->i2c, &tx_val, sizeof(tx_val),
			(uint8_t *)&value, sizeof(value));
	if (ret != 0) {
		ret = -errno;
		log_error("failed i2c transfer : %m");
		return ret;
	}

	*value = be32toh(*value) & 0xfffff;
	log_debug("%s(%s) = %" PRIu32 "\n", __func__, oscillator->name, *value);

	return 0;
}

static int rakon_oscillator_get_ctrl(struct oscillator *oscillator,
		struct oscillator_ctrl *ctrl)
{
	return rakon_oscillator_get_dac(oscillator, &ctrl->dac);
}

static int rakon_oscillator_save(struct oscillator *oscillator)
{
	uint8_t tx_val;
	struct rakon_oscillator *rakon;
	int ret;

	log_debug("%s(%s)\n", __func__, oscillator->name);

	rakon = container_of(oscillator, struct rakon_oscillator, oscillator);

	tx_val = RAKON_CMD_SAVE;
	ret = i2c_transfer(rakon->i2c, &tx_val, sizeof(tx_val), NULL, 0);
	if (ret != 0) {
		ret = -errno;
		log_error("failed i2c transfer : %m");
		return ret;
	}

	return 0;
}

static int rakon_oscillator_get_temp(struct oscillator *oscillator,
		uint16_t *temp)
{
	uint8_t tx_val;
	struct rakon_oscillator *rakon;
	int ret;

	rakon = container_of(oscillator, struct rakon_oscillator, oscillator);

	tx_val = RAKON_CMD_READ_TEMP;
	*temp = 0;
	ret = i2c_transfer(rakon->i2c, &tx_val, sizeof(tx_val),
			   (uint8_t *)temp, sizeof(*temp));
	if (ret != 0) {
		ret = -errno;
		log_error("failed i2c transfer : %m");
		return ret;
	}

	*temp = be16toh(*temp);
	log_debug("%s(%s) = %"PRIu16"\n", __func__, oscillator->name, *temp);

	return 0;
}

static int rakon_oscillator_apply_output(struct oscillator *oscillator, struct od_output *output) {
		return rakon_oscillator_set_dac(oscillator, output->setpoint);
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

static struct oscillator *rakon_oscillator_new(struct config *config)
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
		log_error("rakon-i2c-num config key must be provided");
		goto error;
	}
	i2c_num = ret;
	ret = config_get_uint8_t(config, "rakon-i2c-addr");
	if (ret < 0) {
		log_error("rakon-i2c-addr config key must be provided");
		goto error;
	}
	i2c_addr = ret;

	rakon->i2c = i2c_new(i2c_num, i2c_addr);
	if (rakon->i2c == NULL) {
		ret = -errno;
		log_error("i2c_new: %m");
		goto error;
	}

	oscillator_factory_init(FACTORY_NAME, oscillator, FACTORY_NAME "-%d",
			rakon_oscillator_index);
	rakon_oscillator_index++;

	log_info("instantiated " FACTORY_NAME " oscillator on i2c number %" PRIu8
			" and i2c address %#" PRIx8, i2c_num, i2c_addr);

	return oscillator;
error:
	rakon_oscillator_destroy(&oscillator);
	errno = -ret;
	return NULL;
}

static const struct oscillator_factory rakon_oscillator_factory = {
	.class = {
			.name = FACTORY_NAME,
			.get_ctrl = rakon_oscillator_get_ctrl,
			.save = rakon_oscillator_save,
			.get_temp = rakon_oscillator_get_temp,
			.apply_output = rakon_oscillator_apply_output,
			.dac_min = RAKON_SETPOINT_MIN,
			.dac_max = RAKON_SETPOINT_MAX,
	},
	.new = rakon_oscillator_new,
	.destroy = rakon_oscillator_destroy,
};

static void __attribute__((constructor)) rakon_oscillator_constructor(void)
{
	int ret;

	ret = oscillator_factory_register(&rakon_oscillator_factory);
	if (ret < 0)
		log_error("oscillator_factory_register", ret);
}
