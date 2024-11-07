#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "benchmark.h"
#include "dma_common.h"

DOCA_LOG_REGISTER(DMA_DPU::MAIN)

doca_error_t dma_copy_dpu(struct dma_cfg *cfg);

void *dma_copy_dpu_thread(void *arg);
void signal_host();

int main(int argc, char **argv) {
#ifndef DOCA_ARCH_DPU
  DOCA_LOG_ERR("This program can run only on the DPU");
  return EXIT_FAILURE;
#endif
  doca_error_t result;
  struct dma_cfg cfg = {0};
  init_log_backend();
  init_argp("doca_dma_dpu", &cfg, argc, argv, true);

  result = dma_copy_dpu(&cfg);
  if (result == DOCA_SUCCESS)
    DOCA_LOG_INFO("Sample finished successfully");
  else
    DOCA_LOG_INFO("Sample finished with errors");
  return EXIT_SUCCESS;
}

doca_error_t dma_copy_dpu(struct dma_cfg *cfg) {
  printf("Start initializing... \n");
  struct timespec start = {0};
  struct timespec end = {0};
  uint32_t num_threads = cfg->num_threads == 0 ? 1 : cfg->num_threads;
  DOCA_LOG_INFO("Starting dpu with threads: %u, num of working tasks: %u",
                num_threads, cfg->num_working_tasks);
  struct dma_resources *resources_array =
      (struct dma_resources *)calloc(num_threads, sizeof(struct dma_resources));

  char export_desc_file_name[32];
  char buffer_info_file_name[32];

  for (uint32_t t = 0; t < num_threads; t++) {
    char exported_desc[1024] = {0};
    size_t exported_desc_len;
    char *remote_addr = NULL;
    size_t remote_addr_len;
    sprintf(export_desc_file_name, "export_desc_%d.txt", t);
    sprintf(buffer_info_file_name, "buffer_info_%d.txt", t);
    EXIT_ON_FAIL(import_mmap_to_config(
        export_desc_file_name, buffer_info_file_name, exported_desc,
        &exported_desc_len, &remote_addr, &remote_addr_len));

    resources_array[t].state =
        (struct dma_state *)calloc(1, sizeof(struct dma_state));
    resources_array[t].state->num_ctxs = 1;
    resources_array[t].thread_idx = t;
    resources_array[t].state->buffer_size = cfg->payload;
    resources_array[t].num_buf_pairs = cfg->ops / num_threads;
    resources_array[t].state->buf_inv_size =
        resources_array[t].num_buf_pairs * 2;
    resources_array[t].num_tasks = cfg->num_working_tasks;

    resources_array[t].src_bufs = (struct doca_buf **)malloc(
        resources_array[t].num_buf_pairs * sizeof(struct doca_buf *));
    resources_array[t].dst_bufs = (struct doca_buf **)malloc(
        resources_array[t].num_buf_pairs * sizeof(struct doca_buf *));
    resources_array[t].tasks = (struct doca_dma_task_memcpy **)malloc(
        resources_array[t].num_tasks * sizeof(struct doca_dma_task_memcpy *));
    EXIT_ON_FAIL(allocate_buffer(resources_array[t].state));
    EXIT_ON_FAIL(
        create_dma_dpu_resources(cfg->local_pcie_addr, &resources_array[t]));
    EXIT_ON_FAIL(doca_mmap_create_from_export(
        NULL, exported_desc, exported_desc_len,
        resources_array[t].state->dev[0], &resources_array[t].remote_mmap));
    EXIT_ON_FAIL(allocate_doca_bufs(
        resources_array[t].state, resources_array[t].remote_mmap, remote_addr,
        resources_array[t].num_buf_pairs, resources_array[t].src_bufs,
        resources_array[t].dst_bufs));
    EXIT_ON_FAIL(allocate_dma_tasks(&resources_array[t],
                                    resources_array[t].remote_mmap, remote_addr,
                                    resources_array[t].num_tasks));
  }

  pthread_t *threads = (pthread_t *)calloc(num_threads, sizeof(pthread_t));
  GET_TIME(&start);
  printf("Start running... \n");
  for (uint32_t t = 0; t < num_threads; t++) {
    pthread_create(&threads[t], NULL, dma_copy_dpu_thread,
                   (void *)&resources_array[t]);
  }

  for (uint32_t t = 0; t < num_threads; t++) {
    void *ret;
    pthread_join(threads[t], &ret);
    doca_error_t result = (doca_error_t)ret;
    if (result != DOCA_SUCCESS) {
      DOCA_LOG_ERR("Thead execution failed");
    }
  }
  printf("Running finished... \n");
  GET_TIME(&end);
  timespec_sub(&end, start);

  double latency = 0.F;
#ifdef LATENCY_BENCHMARK
  struct timespec total_latency = {0};
  for (uint32_t t = 0; t < num_threads; t++) {
    timespec_add(&total_latency, resources_array[t].total_time);
  }
  latency = timespec_to_us(total_latency) / (double)cfg->ops;
#endif
  write_statistics_to_file(cfg, &end, latency, "result.json");

  free(threads);
  for (uint32_t t = 0; t < num_threads; t++) {
    if (resources_array[t].remote_mmap != NULL) {
      doca_mmap_stop(resources_array[t].remote_mmap);
      doca_mmap_destroy(resources_array[t].remote_mmap);
    }
    dma_resources_cleanup(&resources_array[t]);
    free(resources_array[t].src_bufs);
    free(resources_array[t].dst_bufs);
    free(resources_array[t].tasks);
  }
  free(resources_array);
  signal_host();
  return DOCA_SUCCESS;
}

void *dma_copy_dpu_thread(void *arg) {
  doca_error_t result = DOCA_SUCCESS;
  struct dma_resources *resources = (struct dma_resources *)arg;
  result = submit_dma_tasks(resources->num_tasks, resources->tasks,
                            resources->timer);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to submit dma tasks in thread %u",
                 resources->thread_idx);
    return (void *)result;
  }
  result = poll_for_completion(resources->state, resources->num_buf_pairs);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to run dma tasks in thread %u", resources->thread_idx);
    return (void *)result;
  }
  return (void *)DOCA_SUCCESS;
}

void signal_host() {
  int sock;
  struct sockaddr_in server_addr;
  sock = socket(AF_INET, SOCK_STREAM, 0);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(8080);
  inet_pton(AF_INET, "192.168.98.75", &server_addr.sin_addr);
  connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
  close(sock);
}