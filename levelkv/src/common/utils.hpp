#pragma once

#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <stdlib.h>

#define SLEEP_IN_NANOS (10 * 1000)

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

inline void print_time() {
  // Get the current time
  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();

  // Break down the duration into seconds and milliseconds
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
  auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(duration - seconds);

  // Convert to time_t to get the human-readable time
  std::time_t current_time = std::chrono::system_clock::to_time_t(now);
  std::tm *time_info = std::localtime(&current_time);

  // Print the time in the format H:M:S.MS
  std::cout << std::put_time(time_info, "%H:%M:%S") << '.' << std::setfill('0')
            << std::setw(3) << milliseconds.count() << " ms" << '\n';
}
