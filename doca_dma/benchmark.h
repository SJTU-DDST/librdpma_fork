#ifndef BENCHMARK_H_
#define BENCHMARK_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

double timespec_to_ms(struct timespec time);

struct timespec get_duration(const struct timespec start,
                             const struct timespec end);

void timespec_add(struct timespec *lhs, const struct timespec rhs);

struct statistics {
  uint64_t ops;
  uint64_t file_size;
  struct timespec total_time;
};

void print_statistics(const struct statistics *stat);

void write_statistics_to_file(const struct statistics *stat,
                              const char *filename);

#endif // BENCHMARK_H_