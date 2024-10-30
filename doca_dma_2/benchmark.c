#include "benchmark.h"

double timespec_to_ms(struct timespec time) {
  return time.tv_sec * 1000.0F + time.tv_nsec / 1000000.0F;
}

double timespec_to_us(struct timespec time) {
  return time.tv_sec * 1000000.0F + time.tv_nsec / 1000.0F;
}

struct timespec get_duration(const struct timespec start,
                             const struct timespec end) {
  struct timespec ret;
  ret.tv_sec = end.tv_sec - start.tv_sec;
  ret.tv_nsec = end.tv_nsec - start.tv_nsec;
  return ret;
}

void timespec_add(struct timespec *lhs, const struct timespec rhs) {
  lhs->tv_nsec += rhs.tv_nsec;
  lhs->tv_sec += rhs.tv_sec;

  if (lhs->tv_nsec >= NANO) {
    lhs->tv_nsec -= NANO;
    lhs->tv_sec += 1;
  }
}

void timespec_sub(struct timespec *lhs, const struct timespec rhs) {
  lhs->tv_nsec -= rhs.tv_nsec;
  lhs->tv_sec -= rhs.tv_sec;

  if (lhs->tv_nsec < 0) {
    lhs->tv_nsec += NANO;
    lhs->tv_sec -= 1;
  }
}

double average(const double *duration_vec, int len) {
  double total = 0.F;
  for (int i = 0; i < len; i++) {
    total += duration_vec[i];
  }
  return total / (double)len;
}

void print_statistics(const struct statistics *stat) {
  printf("Operations: %lu \nFile Size: %lu \nLatency: %lf \n", stat->ops,
         stat->file_size, timespec_to_ms(stat->total_time));
}

void write_statistics_to_file(const struct statistics *stat,
                              const char *filename) {
  double bandwidth =
      stat->file_size * stat->ops / timespec_to_ms(stat->total_time) * 1000;
  FILE *file = fopen(filename, "w+");
  if (file == NULL) {
    perror("Error opening file");
    exit(EXIT_FAILURE);
  }

  fprintf(file, "{\n");
  fprintf(file, "    \"ops\": %lu,\n", stat->ops);
  fprintf(file, "    \"file_size\": %lu,\n", stat->file_size);
  fprintf(file, "    \"latency\": %lf,\n", timespec_to_ms(stat->total_time));
  fprintf(file, "    \"bandwidth\": %lf,\n", bandwidth);
  fprintf(file, "    \"setup_lt\": %lf,\n", stat->avg_setup_lt);
  fprintf(file, "    \"task_lt\": %lf,\n", stat->avg_task_lt);
  fprintf(file, "    \"cleanup_lt\": %lf\n", stat->avg_cleanup_lt);
  fprintf(file, "}\n");

  fclose(file);

  printf("Statistics successfully written to %s\n", filename);
}