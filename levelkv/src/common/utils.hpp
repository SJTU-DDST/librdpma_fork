#pragma once

#include <cstddef>
#include <stdlib.h>

#define ENSURE(expr, message)                                                  \
  if (!(expr)) {                                                               \
    std::cerr << "ERROR: " << (message) << std::endl;                          \
    std::terminate();                                                          \
  }

using frame_id_t = int64_t;
using bucket_id_t = int64_t;

inline void *alignedmalloc(size_t size) {
  void *ret;
  posix_memalign(&ret, 64, size);
  return ret;
}
