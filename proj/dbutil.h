#pragma once

#ifndef __unused
#define __unused __attribute__((unused))
#endif

#define __initializer __attribute__((constructor))

#define __packed __attribute__((packed))

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define DEBUG(fmt,...) \
	fprintf(stderr, fmt, ##__VA_ARGS__)
