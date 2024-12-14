#pragma once

#include <cstddef>
#include <stdlib.h>

#define ENSURE(expr, message)                                                  \
  if (!(expr)) {                                                               \
    std::cerr << "ERROR: " << (message) << std::endl;                          \
    std::terminate();                                                          \
  }

inline void *alignedmalloc(size_t size) {
  void *ret;
  posix_memalign(&ret, 64, size);
  return ret;
}
