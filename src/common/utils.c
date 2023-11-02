#include "utils.h"

#include "log.h"

#include <sys/stat.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Formula to compute mRO50 temperature
double compute_temp(uint32_t reg)
{
	double x = (double)reg / 4095.0;
	if (x == 1.0)
	{
		log_warn("Cannot compute temperature\n");
		// Return absurd value so that user know it is not possible
		return DUMMY_TEMPERATURE_VALUE;
	}
	double resistance  = 47000.0 * x / (1.0 - x);
	double temperature = (4100.0 * 298.15 / (298.15 * log(pow(10, -5) * resistance) + 4100.0)) - 273.14;
	return temperature;
}

/* find device path in /dev from symlink in sysfs */
void find_dev_path(const char* dirname, struct dirent* dir, char* dev_path)
{
	struct stat p_statbuf;
	char        dev_repository[1024]; /* should always be big enough */
	char        dev_name[100] = {};
	char*       dev_realpath;

	snprintf(dev_repository, 1024, "%s/%s", dirname, dir->d_name);
	/* should be fine because callers provides real files only */
	lstat(dev_repository, &p_statbuf);
	if (S_ISLNK(p_statbuf.st_mode) == 1)
	{
		dev_realpath = realpath(dev_repository, NULL);
		char* token  = strtok(dev_realpath, "/");
		while (token != NULL)
		{
			strncpy(dev_name, token, sizeof(dev_name));
			token = strtok(NULL, "/");
		}
		free(dev_realpath);
	}
	else
	{
		/* this is real file, read it's content to get the device */
		FILE* fp = fopen(dev_repository, "r");
		fread(dev_name, 1, sizeof(dev_name), fp);
		fclose(fp);
	}
	sprintf(dev_path, "%s/%s", "/dev", dev_name);
}

/* Find file by name recursively in a directory */
bool find_file(char* path, char* name, char* file_path)
{
	DIR*           directory;
	struct dirent* dp;
	bool           found = false;
	if (!(directory = opendir(path)))
		return false;

	while ((dp = readdir(directory)) != NULL)
	{
		if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;

		if (dp->d_type == DT_DIR)
		{
			char subpath[1024];
			sprintf(subpath, "%s/%s", path, dp->d_name);
			found = find_file(subpath, name, file_path);
			if (found)
				break;
		}
		else if (!strcmp(dp->d_name, name))
		{
			if (file_path != NULL)
				sprintf(file_path, "%s/%s", path, dp->d_name);
			found = true;
			break;
		}
	}
	closedir(directory);
	return found;
}

/**
 * Extracts version information from a string.
 *
 * The expected format is `W X.Y (D)` where:
 *  - W is a word made of one or more non-space characters
 *  - X is an integer
 *  - Y is an integer
 *  - D can be anything
 * the numerical value of X will be stored in @a major,
 * that of Y will be stored in @a minor.
 * @param textToCheck A null-terminated character string
 * @param major will receive the `X` in `X.Y`
 * @param minor will receive the `Y` in `X.Y`
 * @return
 *  - `true` if the parsing was successful
 *  - `false` otherwise
 */
bool parse_receiver_version(char* textToCheck, int* major, int* minor)
{
	return textToCheck && sscanf(textToCheck, "%*s %i.%i", major, minor) == 2;
}

#ifdef UNIT_TESTS

#	include <assert.h>

#	define TEST(STRING, MAJOR, MINOR) \
		{ \
			int  major = 0, minor = 0; \
			bool ret = parse_receiver_version(STRING, &major, &minor); \
			printf("%s: %i.%i\n", (ret ? "OK" : "fail"), major, minor); \
			assert(ret&& major == MAJOR && minor == MINOR); \
		}

//	assert(parse_receiver_version(STRING, &major, &minor) && major == MAJOR && minor == MINOR)

int main()
{
	TEST("f9d 2.01 (smth)", 2, 1);
	TEST("f9d 2.20 (whtv)", 2, 20);
	TEST("some_name    4.73  (Some description)", 4, 73);
	TEST("  nm   1.2(mmh)", 1, 2);
	TEST(" K 3.04 (abcd)", 3, 4);
	TEST("K 5.006", 5, 6);
	TEST("fd 7.8 (36W7vCCffR6Gv83)", 7, 8);
	TEST("some-name  \t\t  9.10", 9, 10);
	int major, minor;
	assert(!parse_receiver_version(NULL, &major, &minor));
	assert(!parse_receiver_version("", &major, &minor));
	assert(!parse_receiver_version("2.1", &major, &minor));
	assert(!parse_receiver_version(" wdw 320", &major, &minor));
}

#endif
