#pragma once

#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <stdlib.h>
#include <ostream>
#include <fstream>

#define TOTAL_ROUNDS 1000000
#define WARMUP_ROUNDS 100000
#define PAYLOAD 1

inline void print_time() {
  // Get current time
  auto now = std::chrono::system_clock::now();

  // Convert to time_t (for human-readable formatting)
  std::time_t now_c = std::chrono::system_clock::to_time_t(now);
  std::tm *localTime = std::localtime(&now_c);

  // Get microseconds separately
  auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    now.time_since_epoch()) %
                1000000;

  // Print formatted time with microsecond precision
  std::cout << "Current time: "
            << std::put_time(localTime,
                             "%Y-%m-%d %H:%M:%S") // Print YYYY-MM-DD HH:MM:SS
            << "." << std::setw(6) << std::setfill('0')
            << now_us.count() // Print microseconds
            << std::endl;
}

inline std::chrono::microseconds get_time_us() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch());
}