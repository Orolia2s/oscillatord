/**
 * @file config.c
 * @brief Header for function handling parsing of configuration file
 */
#include "config.h"

#include "log.h"
#include "utils.h"

#include <argz.h>
#include <envz.h>

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

volatile int loop = true;

static int   read_file(const char* path, char** argz, size_t* argz_len)
{
	char __attribute__((cleanup(string_cleanup)))* string = NULL;
	FILE __attribute__((cleanup(file_cleanup)))* f        = NULL;
	long   size;
	int    ret;
	size_t sret;
	char*  entry = NULL;

	f = fopen(path, "rbe");
	if (f == NULL)
		return -errno;

	/* compute the size of the file */
	ret = fseek(f, 0, SEEK_END);
	if (ret == -1)
		return -errno;
	size = ftell(f);
	if (size == -1)
		return -errno;
	ret = fseek(f, 0, SEEK_SET);
	if (ret == -1)
		return -errno;

	/* read all */
	string = calloc(size, 1);
	if (string == NULL)
		return -errno;

	sret = fread(string, 1, size, f);
	if (sret < (size_t)size)
		return feof(f) ? -EIO : ret;

	ret = -argz_create_sep(string, '\n', argz, argz_len);
	if (ret)
		return ret;

	while ((entry = argz_next(*argz, *argz_len, entry)))
	{
		if (entry[0] == '#')
		{
			argz_delete(argz, argz_len, entry);
			entry = NULL;
		}
	}

	return 0;
}

int config_init(struct config* config, const char* path)
{
	int ret;

	memset(config, 0, sizeof(*config));

	config->path = strdup(path);
	if (config->path == NULL)
		return -errno;

	ret = read_file(path, &config->argz, &config->len);
	return ret;
}

const char* config_get(const struct config* config, const char* key)
{
	return envz_get(config->argz, config->len, key);
}

const char* config_get_default(const struct config* config, const char* key, const char* default_value)
{
	const char* value;

	value = config_get(config, key);

	return value == NULL ? default_value : value;
}

bool config_get_bool_default(const struct config* config, const char* key, bool default_value)
{
	const char* value;

	value = config_get(config, key);
	if (value == NULL)
	{
		log_warn("value not found for %s!", key);
		return default_value;
	}
	if (strcmp(value, "true") == 0)
		return true;
	if (strcmp(value, "false") == 0)
		return false;

	log_error("invalid value for %s!", key);
	return default_value;
}

int config_set(struct config* config, const char* key, const char* value)
{
	return -envz_add(&config->argz, &config->len, key, value);
}

/* Get a number between 0 and (2**31)-1 */
long config_get_unsigned_number(const struct config* config, const char* key)
{
	const char*   str_value;
	char*         endptr;
	unsigned long value;

	str_value = config_get(config, key);
	if (str_value == NULL)
		return errno != 0 ? -errno : -ESRCH;

	value = strtoul(str_value, &endptr, 0);
	if (*str_value == '\0' || *endptr != '\0')
		return -EINVAL;

	if (value > LONG_MAX)
		return -ERANGE;

	return value;
}

/* Get a signed number between INT16_MIN and INT16_MAX */
int config_get_int16_t(const struct config* config, const char* key, int16_t* val)
{
	const char* str_value;
	char*       endptr;
	long        value;

	str_value = config_get(config, key);
	if (str_value == NULL)
		return errno != 0 ? -errno : -ESRCH;

	value = strtol(str_value, &endptr, 0);
	if (*str_value == '\0' || *endptr != '\0')
		return -EINVAL;

	if (value > INT16_MAX || value < INT16_MIN)
		return -ERANGE;

	*val = (int16_t)value;
	return 0;
}

int config_get_uint8_t(const struct config* config, const char* key)
{
	long value;

	value = config_get_unsigned_number(config, key);
	if (value > UINT8_MAX)
		return -ERANGE;

	return value;
}

void config_dump(const struct config* config, char* buf, size_t buf_len)
{
	size_t n;
	size_t slen;
	char*  argz = malloc(config->len);

	memcpy(argz, config->argz, config->len);
	argz_stringify(argz, config->len, '\n');
	slen = strlen(argz) + 1;
	n    = buf_len < slen ? buf_len : slen;
	memcpy(buf, argz, n);
	free(argz);
}

void config_cleanup(struct config* config)
{
	if (config->argz != NULL)
		free(config->argz);
	if (config->path != NULL)
		free(config->path);
	memset(config, 0, sizeof(*config));
}

int config_save(struct config* config, const char* path)
{
	FILE __attribute__((cleanup(file_cleanup)))* f = NULL;
	char data[1024]                                = {0}; // size of eeprom
	config_dump(config, data, sizeof(data));

	f = fopen(path, "w+");
	if (f == NULL)
	{
		log_error("Could not open file %s", path);
		return -EINVAL;
	}
	fwrite(data, 1, strlen(data) + 1, f);
	fwrite("\n", 1, 1, f);
	return 0;
}

static int fill_tty_devices(const char* sysfs_path, struct dirent* entry, struct devices_path* dp)
{
	char           filedir[FILENAME_MAX];
	struct dirent* entry_tty;
	DIR*           tty_dir;
	int            ret = 0;

	snprintf(filedir, FILENAME_MAX, "%s/%s", sysfs_path, entry->d_name);
	tty_dir = opendir(filedir);
	if (tty_dir == NULL)
	{
		return ret;
	}

	while ((entry_tty = readdir(tty_dir)) != NULL && ret < 2)
	{
		if (strcmp(entry_tty->d_name, "ttyGNSS") == 0)
		{
			find_dev_path(filedir, entry_tty, dp->gnss_path);
			log_debug("ttyGPS detected: %s", dp->gnss_path);
			ret++;
		}
		else if (strcmp(entry_tty->d_name, "ttyMAC") == 0)
		{
			find_dev_path(filedir, entry_tty, dp->mac_path);
			log_debug("ttyMAC detected: %s", dp->mac_path);
			ret++;
		}
	}
	closedir(tty_dir);
	return ret;
}

int config_discover_devices(const struct config* config, struct devices_path* devices_path)
{
	const char* sysfs_path;
	DIR*        ocp_dir;
	int         ret = 0;

	sysfs_path = config_get(config, "sysfs-path");
	if (sysfs_path == NULL)
	{
		log_error("No sysfs-path provided in oscillatord config file !");
		return -EINVAL;
	}
	log_info("Scanning sysfs path %s", sysfs_path);

	ocp_dir              = opendir(sysfs_path);
	if (ocp_dir == NULL)
	{
		log_fatal("Failed to open '%s': %s", sysfs_path, strerror(errno));
		return -errno;
	}
	struct dirent* entry = readdir(ocp_dir);
	while (entry != NULL)
	{
		if (strcmp(entry->d_name, "mro50") == 0)
		{
			find_dev_path(sysfs_path, entry, devices_path->mro_path);
			log_debug("mro50 device detected: %s", devices_path->mro_path);
		}
		else if (strcmp(entry->d_name, "ptp") == 0)
		{
			find_dev_path(sysfs_path, entry, devices_path->ptp_path);
			log_debug("ptp clock device detected: %s", devices_path->ptp_path);
		}
		else if (strcmp(entry->d_name, "pps") == 0)
		{
			find_dev_path(sysfs_path, entry, devices_path->pps_path);
			log_debug("pps device detected: %s", devices_path->pps_path);
		}
		else if (strcmp(entry->d_name, "ttyGNSS") == 0)
		{
			find_dev_path(sysfs_path, entry, devices_path->gnss_path);
			log_debug("ttyGPS detected: %s", devices_path->gnss_path);
		}
		else if (strcmp(entry->d_name, "ttyMAC") == 0)
		{
			find_dev_path(sysfs_path, entry, devices_path->mac_path);
			log_debug("ttyMAC detected: %s", devices_path->mac_path);
		}
		else if (strlen(entry->d_name) == 3 && entry->d_type == DT_DIR && strcmp(entry->d_name, "tty") == 0)
		{
			ret = fill_tty_devices(sysfs_path, entry, devices_path);
			if (ret != 2)
			{
				log_error("Not all tty devices detected, exiting");
				ret = -EINVAL;
				break;
			}
			ret = 0;
		}
		else if (strcmp(entry->d_name, "disciplining_config") == 0)
		{
			find_file((char*)sysfs_path, "disciplining_config", devices_path->disciplining_config_path);
			log_debug("disciplining_config detected: %s", devices_path->disciplining_config_path);
		}
		else if (strcmp(entry->d_name, "temperature_table") == 0)
		{
			find_file((char*)sysfs_path, "temperature_table", devices_path->temperature_table_path);
			log_debug("temperature_table detected: %s", devices_path->temperature_table_path);
		}

		entry = readdir(ocp_dir);
	}
	closedir(ocp_dir);
	return ret;
}
