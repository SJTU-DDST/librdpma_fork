#include "dma_common.h"
#include <stdio.h>
#include <unistd.h>

DOCA_LOG_REGISTER(DMA_DPU::MAIN)

doca_error_t dma_copy_dpu(struct dma_cfg *cfg);

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
  struct dma_resources resources = {0};
  resources.state = (struct dma_state *)malloc(sizeof(struct dma_state));
  struct dma_state *state = resources.state;

  state->buffer_size = cfg->payload;
  resources.num_buf_pairs = cfg->ops;
  state->buf_inv_size = cfg->ops * 2;
  resources.num_tasks = cfg->num_working_tasks;

  struct doca_mmap *remote_mmap;
  char exported_desc[1024] = {0};
  size_t exported_desc_len;
  char *remote_addr = NULL;
  size_t remote_addr_len;

  resources.src_bufs = (struct doca_buf **)malloc(resources.num_buf_pairs *
                                                  sizeof(struct doca_buf *));
  resources.dst_bufs = (struct doca_buf **)malloc(resources.num_buf_pairs *
                                                  sizeof(struct doca_buf *));
  resources.tasks = (struct doca_dma_task_memcpy **)malloc(
      resources.num_tasks * sizeof(struct doca_dma_task_memcpy *));

  EXIT_ON_FAIL(allocate_buffer(state));
  EXIT_ON_FAIL(create_dma_dpu_resources(cfg->local_pcie_addr, &resources));

  EXIT_ON_FAIL(import_mmap_to_config("export_desc.txt", "buffer_info.txt",
                                     exported_desc, &exported_desc_len,
                                     &remote_addr, &remote_addr_len));
  EXIT_ON_FAIL(doca_mmap_create_from_export(
      NULL, exported_desc, exported_desc_len, state->dev, &remote_mmap));
  EXIT_ON_FAIL(allocate_doca_bufs(state, remote_mmap, remote_addr,
                                  resources.num_buf_pairs, resources.src_bufs,
                                  resources.dst_bufs));
  EXIT_ON_FAIL(allocate_dma_tasks(&resources, remote_mmap, remote_addr,
                                  resources.num_tasks));
  EXIT_ON_FAIL(submit_dma_tasks(resources.num_tasks, resources.tasks));

  EXIT_ON_FAIL(poll_for_completion(state, resources.num_buf_pairs));

  if (remote_mmap != NULL) {
    doca_mmap_stop(remote_mmap);
    doca_mmap_destroy(remote_mmap);
  }
  dma_resources_cleanup(&resources);
  free(resources.src_bufs);
  free(resources.dst_bufs);
  free(resources.tasks);
  return DOCA_SUCCESS;
}