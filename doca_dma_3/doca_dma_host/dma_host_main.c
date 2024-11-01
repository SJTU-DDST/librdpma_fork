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
  struct dma_state state = {0};

  state.buffer_size = cfg->payload;

  EXIT_ON_FAIL(allocate_buffer(&state));
  EXIT_ON_FAIL(create_dma_host_state(cfg->local_pcie_addr, &state));
  EXIT_ON_FAIL(export_mmap_to_files(state.local_mmap, state.dev, state.buffer,
                                    state.buffer_size, "export_desc.txt",
                                    "buffer_info.txt"));

  DOCA_LOG_INFO("Wait till the DPU has finished and press enter");
  int enter = 0;
  while (enter != '\r' && enter != '\n')
    enter = getchar();

  dma_state_cleanup(&state);
  return DOCA_SUCCESS;
}