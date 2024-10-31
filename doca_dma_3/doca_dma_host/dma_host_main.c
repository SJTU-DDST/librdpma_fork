#include "dma_common.h"
#include <stdio.h>

DOCA_LOG_REGISTER(DMA_HOST::MAIN)

doca_error_t dma_copy_host();

int main() {
  doca_error_t result;
  struct doca_log_backend *sdk_log;
  int exit_status = EXIT_FAILURE;

  /* Register a logger backend */
  result = doca_log_backend_create_standard();
  if (result != DOCA_SUCCESS)
    return EXIT_FAILURE;

  /* Register a logger backend for internal SDK errors and warnings */
  result = doca_log_backend_create_with_file_sdk(stderr, &sdk_log);
  if (result != DOCA_SUCCESS)
    goto exit;
  result = doca_log_backend_set_sdk_level(sdk_log, DOCA_LOG_LEVEL_INFO);
  if (result != DOCA_SUCCESS)
    goto exit;

#ifndef DOCA_ARCH_HOST
  DOCA_LOG_ERR("This program can run only on the Host");
  goto exit;
#endif

  EXIT_ON_FAIL(dma_copy_host());
  exit_status = EXIT_SUCCESS;

exit:
  if (exit_status == EXIT_SUCCESS)
    DOCA_LOG_INFO("Sample finished successfully");
  else
    DOCA_LOG_INFO("Sample finished with errors");
  return exit_status;
}

doca_error_t dma_copy_host() {
  struct dma_state state = {0};
  state.buffer_size = 4096;

  EXIT_ON_FAIL(allocate_buffer(&state));
  char pcie_addr[16] = "03:00.0\0";
  EXIT_ON_FAIL(create_dma_host_state(pcie_addr, &state));
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