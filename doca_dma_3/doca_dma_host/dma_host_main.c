#include <stdio.h>

#include "dma_common.h"

DOCA_LOG_REGISTER(DMA_HOST::MAIN)

doca_error_t dma_copy_host(struct dma_cfg *cfg);

int main(int argc, char **argv) {
#ifndef DOCA_ARCH_HOST
  DOCA_LOG_ERR("This program can run only on the Host");
  return EXIT_FAILURE;
#endif
  doca_error_t result;
  struct dma_cfg cfg = {0};
  init_log_backend();
  init_argp("doca_dma_host", &cfg, argc, argv, false);

  result = dma_copy_host(&cfg);
  if (result == DOCA_SUCCESS)
    DOCA_LOG_INFO("Sample finished successfully");
  else
    DOCA_LOG_INFO("Sample finished with errors");
  return EXIT_SUCCESS;
}

doca_error_t dma_copy_host(struct dma_cfg *cfg) {
  uint32_t num_threads = 16;
  struct dma_state *state =
      (struct dma_state *)calloc(num_threads, sizeof(struct dma_state));

  char export_desc_file_name[32];
  char buffer_info_file_name[32];

  for (uint32_t t = 0; t < num_threads; t++) {
    sprintf(export_desc_file_name, "export_desc_%d.txt", t);
    sprintf(buffer_info_file_name, "buffer_info_%d.txt", t);
    state[t].buffer_size = cfg->payload;
    EXIT_ON_FAIL(allocate_buffer(&state[t]));
    EXIT_ON_FAIL(create_dma_host_state(cfg->local_pcie_addr, &state[t]));
    EXIT_ON_FAIL(export_mmap_to_files(
        state[t].local_mmap, state[t].dev, state[t].buffer,
        state[t].buffer_size, export_desc_file_name, buffer_info_file_name));
  }

  DOCA_LOG_INFO("Wait till the DPU has finished and press enter");
  int enter = 0;
  while (enter != '\r' && enter != '\n')
    enter = getchar();

  for (uint32_t t = 0; t < num_threads; t++) {
    dma_state_cleanup(&state[t]);
  }
  free(state);
  return DOCA_SUCCESS;
}