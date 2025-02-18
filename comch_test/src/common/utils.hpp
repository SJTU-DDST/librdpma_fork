#pragma once

#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <stdlib.h>

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