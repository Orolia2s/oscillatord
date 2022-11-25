#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "utils.h"
#include "log.h"

void file_cleanup(FILE **f)
{
	if (f == NULL || *f == NULL)
		return;

	fclose(*f);
	*f = NULL;
}

void string_cleanup(char **s)
{
	if (s == NULL || *s == NULL)
		return;

	free(*s);
	*s = NULL;
}

void fd_cleanup(int *fd)
{
	if (fd == NULL)
		return;

	close(*fd);
	*fd = -1;
}

// Formula to compute mRO50 temperature
double compute_temp(uint32_t reg)
{
	double x = (double) reg / 4095.0;
	if (x == 1.0) {
		log_warn("Cannot compute temperature\n");
		// Return absurd value so that user know it is not possible
		return DUMMY_TEMPERATURE_VALUE;
	}
	double resistance = 47000.0 * x / (1.0 - x);
	double temperature = (
		4100.0 * 298.15
		/ (298.15 * log(pow(10, -5) * resistance) + 4100.0)
		) - 273.14;
	return temperature;
}

/* find device path in /dev from symlink in sysfs */
void find_dev_path(const char *dirname, struct dirent *dir, char *dev_path)
{
    char dev_repository[1024];   /* should always be big enough */
    sprintf(dev_repository, "%s/%s", dirname, dir->d_name );
    char dev_name[100];
    char * token = strtok(realpath(dev_repository, NULL), "/");
    while(token != NULL) {
        strncpy(dev_name, token, sizeof(dev_name));
        token = strtok(NULL, "/");
    }
    sprintf(dev_path, "%s/%s", "/dev", dev_name );
}

/* Find file by name recursively in a directory */
bool find_file(char * path , char * name, char * file_path)
{
    DIR * directory;
    struct dirent * dp;
    bool found = false;
    if(!(directory = opendir(path)))
        return false;

    while ((dp = readdir(directory)) != NULL) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
            continue;

        if (dp->d_type == DT_DIR) {
            char subpath[1024];
            sprintf(subpath, "%s/%s", path, dp->d_name);
            found = find_file(subpath, name, file_path);
            if (found)
                break;
        } else if (!strcmp(dp->d_name, name)) {
            if (file_path != NULL)
                sprintf(file_path, "%s/%s", path, dp->d_name);
            found = true;
            break;
        }
    }
    closedir(directory);
    return found;
}
