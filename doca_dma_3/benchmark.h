#ifndef BENCHMARK_H_
#define BENCHMARK_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "dma_common.h"

#define NANO 1000000000

double timespec_to_ms(struct timespec time);

double timespec_to_us(struct timespec time);

struct timespec get_duration(const struct timespec start,
                             const struct timespec end);

void timespec_add(struct timespec *lhs, const struct timespec rhs);

void timespec_sub(struct timespec *lhs, const struct timespec rhs);

double average(const double *duration_vec, int len);

void write_statistics_to_file(const struct dma_cfg *cfg,
                              const struct timespec *total_time,
                              double latency_in_us, const char *filename);

#endif // BENCHMARK_H_