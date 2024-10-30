#ifndef BENCHMARK_H_
#define BENCHMARK_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define NANO 1000000000

double timespec_to_ms(struct timespec time);

double timespec_to_us(struct timespec time);

struct timespec get_duration(const struct timespec start,
                             const struct timespec end);

void timespec_add(struct timespec *lhs, const struct timespec rhs);

void timespec_sub(struct timespec *lhs, const struct timespec rhs);

double average(const double *duration_vec, int len);

struct statistics {
  uint64_t ops;
  uint64_t file_size;
  struct timespec total_time;
  double avg_setup_lt;
  double avg_task_lt;
  double avg_cleanup_lt;
};

void print_statistics(const struct statistics *stat);

void write_statistics_to_file(const struct statistics *stat,
                              const char *filename);

#endif // BENCHMARK_H_