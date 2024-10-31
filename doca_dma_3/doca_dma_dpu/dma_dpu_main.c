#include "dma_common.h"
#include <stdio.h>

DOCA_LOG_REGISTER(DMA_DPU::MAIN)

doca_error_t dma_copy_dpu();

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

#ifndef DOCA_ARCH_DPU
  DOCA_LOG_ERR("This program can run only on the DPU");
  goto exit;
#endif

  EXIT_ON_FAIL(dma_copy_dpu());
  exit_status = EXIT_SUCCESS;

exit:
  if (exit_status == EXIT_SUCCESS)
    DOCA_LOG_INFO("Sample finished successfully");
  else
    DOCA_LOG_INFO("Sample finished with errors");
  return exit_status;
}

doca_error_t dma_copy_dpu() {
  struct dma_resources resources = {0};
  struct dma_state state = resources.state;
  state.buffer_size = 4096;
  state.buf_inv_size = 4096;
  resources.num_buf_pairs = 2048;
  resources.num_tasks = 64;
  char pcie_addr[16] = "03:00.0\0";
  struct doca_mmap *remote_mmap;
  char *exported_desc = NULL;
  size_t exported_desc_len;
  char *remote_addr = NULL;
  size_t remote_addr_len;

  resources.src_bufs = (struct doca_buf **)malloc(resources.num_buf_pairs *
                                                  sizeof(struct doca_buf *));
  resources.dst_bufs = (struct doca_buf **)malloc(resources.num_buf_pairs *
                                                  sizeof(struct doca_buf *));
  resources.tasks = (struct doca_dma_task_memcpy **)malloc(
      resources.num_tasks * sizeof(struct doca_dma_task_memcpy *));

  EXIT_ON_FAIL(allocate_buffer(&state));
  EXIT_ON_FAIL(create_dma_dpu_resources(pcie_addr, &resources));
  EXIT_ON_FAIL(import_mmap_to_config("export_desc.txt", "buffer_info.txt",
                                     exported_desc, &exported_desc_len,
                                     &remote_addr, &remote_addr_len));
  EXIT_ON_FAIL(doca_mmap_create_from_export(
      NULL, exported_desc, exported_desc_len, state.dev, &remote_mmap));
  EXIT_ON_FAIL(allocate_doca_bufs(&state, remote_mmap, remote_addr,
                                  resources.num_buf_pairs, resources.src_bufs,
                                  resources.dst_bufs));
  EXIT_ON_FAIL(allocate_dma_tasks(&resources, remote_mmap, remote_addr,
                                  resources.num_tasks));

  EXIT_ON_FAIL(submit_dma_tasks(resources.num_tasks, resources.tasks));
  EXIT_ON_FAIL(poll_for_completion(&state, resources.num_buf_pairs));

  free(resources.src_bufs);
  free(resources.dst_bufs);
  free(resources.tasks);
  // if (remote_mmap != NULL) {
  //   doca_mmap_stop(remote_mmap);
  //   doca_mmap_destroy(remote_mmap);
  // }
  dma_state_cleanup(&state);
  return DOCA_SUCCESS;
}