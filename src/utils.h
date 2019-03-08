#ifndef UTILS_H_
#define UTILS_H_
#include <stddef.h>
#include <inttypes.h>

#ifndef container_of
#define container_of(ptr, type, member) ({ \
	const typeof(((type *)0)->member)*__mptr = (ptr); \
	(type *)((uintptr_t)__mptr - offsetof(type, member)); })
#endif /* ut_container_of */

void file_cleanup(FILE **f);
void string_cleanup(char **s);
void fd_cleanup(int *fd);

#endif /* UTILS_H_ */
