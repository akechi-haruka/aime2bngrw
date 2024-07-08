#pragma once

#include <stddef.h>

#ifndef NDEBUG
void dump(const void *ptr, size_t nbytes);
#else
#define dump(ptr, nbytes)
#endif
