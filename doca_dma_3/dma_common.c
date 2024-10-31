#include "dma_common.h"

DOCA_LOG_REGISTER(DMA::COMMON);

doca_error_t allocate_buffer(struct dma_state *state) {
  state->buffer = (char *)malloc(state->buffer_size);
  if (state->buffer == NULL)
    return DOCA_ERROR_NO_MEMORY;
  return DOCA_SUCCESS;
}

doca_error_t
free_dma_memcpy_task_buffers(struct doca_dma_task_memcpy *dma_task) {
  struct doca_buf *src = doca_dma_task_memcpy_get_src(dma_task);
  struct doca_buf *dst = doca_dma_task_memcpy_get_dst(dma_task);

  EXIT_ON_FAIL(doca_buf_dec_refcount(src, NULL));
  EXIT_ON_FAIL(doca_buf_dec_refcount(dst, NULL));

  return DOCA_SUCCESS;
}

doca_error_t dma_task_free(struct doca_dma_task_memcpy *dma_task) {
  EXIT_ON_FAIL(free_dma_memcpy_task_buffers(dma_task));
  doca_task_free(doca_dma_task_memcpy_as_task(dma_task));

  return DOCA_SUCCESS;
}

doca_error_t allocate_dma_tasks(struct dma_resources *resources,
                                struct doca_mmap *remote_mmap,
                                void *remote_addr, uint32_t num_tasks,
                                struct doca_dma_task_memcpy **tasks) {
  uint32_t task_id = 0;
  struct dma_state state = resources->state;

  DOCA_LOG_INFO("Allocating tasks");

  for (task_id = 0; task_id < num_tasks; task_id++) {
    struct doca_buf *source = NULL;
    struct doca_buf *destination = NULL;
    union doca_data user_data = {0};

    user_data.u64 = task_id;

    EXIT_ON_FAIL(doca_buf_inventory_buf_get_by_data(
        state.buf_inv, state.local_mmap, state.buffer, state.buffer_size,
        &source));

    EXIT_ON_FAIL(doca_buf_inventory_buf_get_by_addr(
        state.buf_inv, remote_mmap, remote_addr, state.buffer_size,
        &destination));

    EXIT_ON_FAIL(doca_dma_task_memcpy_alloc_init(
        resources->dma_ctx, source, destination, user_data, &tasks[task_id]));
  }

  return DOCA_SUCCESS;
}

doca_error_t submit_dma_tasks(uint32_t num_tasks,
                              struct doca_dma_task_memcpy **tasks) {
  uint32_t task_id = 0;

  DOCA_LOG_INFO("Submitting tasks");

  for (task_id = 0; task_id < num_tasks; task_id++)
    EXIT_ON_FAIL(
        doca_task_submit(doca_dma_task_memcpy_as_task(tasks[task_id])));

  return DOCA_SUCCESS;
}

static doca_error_t check_dev_dma_capable(struct doca_devinfo *devinfo) {
  doca_error_t status = doca_dma_cap_task_memcpy_is_supported(devinfo);

  if (status != DOCA_SUCCESS)
    return status;

  return DOCA_SUCCESS;
}

doca_error_t open_doca_device_with_pci(const char *pci_addr, tasks_check func,
                                       struct doca_dev **retval) {
  struct doca_devinfo **dev_list;
  uint32_t nb_devs;
  uint8_t is_addr_equal = 0;
  int res;
  size_t i;

  *retval = NULL;

  res = doca_devinfo_create_list(&dev_list, &nb_devs);
  if (res != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to load doca devices list: %s",
                 doca_error_get_descr(res));
    return res;
  }

  for (i = 0; i < nb_devs; i++) {
    res = doca_devinfo_is_equal_pci_addr(dev_list[i], pci_addr, &is_addr_equal);
    if (res == DOCA_SUCCESS && is_addr_equal) {
      if (func != NULL && func(dev_list[i]) != DOCA_SUCCESS)
        continue;

      /* if device can be opened */
      res = doca_dev_open(dev_list[i], retval);
      if (res == DOCA_SUCCESS) {
        doca_devinfo_destroy_list(dev_list);
        return res;
      }
    }
  }

  DOCA_LOG_WARN("Matching device not found");
  res = DOCA_ERROR_NOT_FOUND;

  doca_devinfo_destroy_list(dev_list);
  return res;
}

doca_error_t open_device(const char *pcie_addr, struct dma_state *state) {
  EXIT_ON_FAIL(
      open_doca_device_with_pci(pcie_addr, check_dev_dma_capable, &state->dev));
  return DOCA_SUCCESS;
}

doca_error_t create_pe(struct dma_state *state) {
  EXIT_ON_FAIL(doca_pe_create(&state->pe));
  return DOCA_SUCCESS;
}

doca_error_t create_mmap(struct dma_state *state,
                         enum doca_access_flag access_flag) {
  EXIT_ON_FAIL(doca_mmap_create(&state->local_mmap));
  EXIT_ON_FAIL(doca_mmap_set_memrange(state->local_mmap, state->buffer,
                                      state->buffer_size));
  EXIT_ON_FAIL(doca_mmap_add_dev(state->local_mmap, state->dev));
  EXIT_ON_FAIL(doca_mmap_set_permissions(state->local_mmap, access_flag));
  EXIT_ON_FAIL(doca_mmap_start(state->local_mmap));
  return DOCA_SUCCESS;
}

doca_error_t create_buf_inventory(struct dma_state *state) {
  EXIT_ON_FAIL(doca_buf_inventory_create(state->buf_inv_size, &state->buf_inv));
  EXIT_ON_FAIL(doca_buf_inventory_start(state->buf_inv));
  return DOCA_SUCCESS;
}

doca_error_t create_dma_state(const char *pcie_addr, struct dma_state *state) {
  EXIT_ON_FAIL(open_device(pcie_addr, state));
  EXIT_ON_FAIL(create_mmap(state, DOCA_ACCESS_FLAG_PCI_READ_ONLY));
  EXIT_ON_FAIL(create_buf_inventory(state));
  EXIT_ON_FAIL(create_pe(state));
  return DOCA_SUCCESS;
}

static void dma_memcpy_completed_callback(struct doca_dma_task_memcpy *dma_task,
                                          union doca_data task_user_data,
                                          union doca_data ctx_user_data) {}

static void dma_memcpy_error_callback(struct doca_dma_task_memcpy *dma_task,
                                      union doca_data task_user_data,
                                      union doca_data ctx_user_data) {}

doca_error_t create_dma_dpu_resources(const char *pcie_addr,
                                      struct dma_resources *resources) {
  memset(resources, 0, sizeof(*resources));
  union doca_data ctx_user_data = {0};
  EXIT_ON_FAIL(create_dma_state(pcie_addr, &resources->state));
  EXIT_ON_FAIL(doca_dma_create(resources->state.dev, &resources->dma_ctx));
  resources->ctx = doca_dma_as_ctx(resources->dma_ctx);
  EXIT_ON_FAIL(doca_dma_task_memcpy_set_conf(
      resources->dma_ctx, dma_memcpy_completed_callback,
      dma_memcpy_error_callback, NUM_DMA_TASKS));
  ctx_user_data.ptr = resources;
  EXIT_ON_FAIL(doca_ctx_set_user_data(resources->ctx, ctx_user_data));
}

doca_error_t create_dma_host_state(const char *pcie_addr,
                                   struct dma_state *state) {
  EXIT_ON_FAIL(open_device(pcie_addr, state));
  EXIT_ON_FAIL(create_mmap(state, DOCA_ACCESS_FLAG_PCI_READ_ONLY));
}

void dma_state_cleanup(struct dma_state *state) {
  if (state->pe != NULL)
    (void)doca_pe_destroy(state->pe);
  if (state->buf_inv != NULL) {
    (void)doca_buf_inventory_stop(state->buf_inv);
    (void)doca_buf_inventory_destroy(state->buf_inv);
  }
  if (state->local_mmap != NULL) {
    (void)doca_mmap_stop(state->local_mmap);
    (void)doca_mmap_destroy(state->local_mmap);
  }
  if (state->dev != NULL)
    (void)doca_dev_close(state->dev);
  if (state->buffer != NULL)
    free(state->buffer);
}

doca_error_t export_mmap_to_files(struct doca_mmap *mmap,
                                  const struct doca_dev *dev,
                                  const char *src_buffer,
                                  size_t src_buffer_size,
                                  const char *export_desc_file_path,
                                  const char *buffer_info_file_path) {
  const void *export_desc;
  size_t export_desc_len;
  EXIT_ON_FAIL(doca_mmap_export_pci(mmap, dev, &export_desc, &export_desc_len));

  FILE *fp;
  uint64_t buffer_addr = (uintptr_t)src_buffer;
  uint64_t buffer_len = (uint64_t)src_buffer_size;

  fp = fopen(export_desc_file_path, "wb");
  if (fp == NULL) {
    DOCA_LOG_ERR("Failed to create the DMA copy file");
    return DOCA_ERROR_IO_FAILED;
  }

  if (fwrite(export_desc, 1, export_desc_len, fp) != export_desc_len) {
    DOCA_LOG_ERR("Failed to write all data into the file");
    fclose(fp);
    return DOCA_ERROR_IO_FAILED;
  }

  fclose(fp);

  fp = fopen(buffer_info_file_path, "w");
  if (fp == NULL) {
    DOCA_LOG_ERR("Failed to create the DMA copy file");
    return DOCA_ERROR_IO_FAILED;
  }

  fprintf(fp, "%" PRIu64 "\n", buffer_addr);
  fprintf(fp, "%" PRIu64 "", buffer_len);

  fclose(fp);

  return DOCA_SUCCESS;
}
