#ifndef CONFIG_H_
#define CONFIG_H_
#include <stddef.h>

struct config {
	char *argz;
	size_t len;
	char *path;
};

int config_init(struct config *config, const char *path);
const char *config_get(const struct config *config, const char *key);

#endif /* CONFIG_H_ */
