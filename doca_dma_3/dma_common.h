#ifndef DMA_COMMON_H_
#define DMA_COMMON_H_

#include <doca_buf_inventory.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_dma.h>
#include <doca_log.h>
#include <doca_mmap.h>
#include <doca_pe.h>

#define NUM_DMA_TASKS 256

#define EXIT_ON_FAIL(_expression_)                                             \
  {                                                                            \
    doca_error_t _status_ = _expression_;                                      \
                                                                               \
    if (_status_ != DOCA_SUCCESS) {                                            \
      DOCA_LOG_ERR("%s failed with status %s", __func__,                       \
                   doca_error_get_descr(_status_));                            \
      return _status_;                                                         \
    }                                                                          \
  }

/* Function to check if a given device is capable of executing some task */
typedef doca_error_t (*tasks_check)(struct doca_devinfo *);

struct dma_state {
  struct doca_dev *dev;
  struct doca_mmap *local_mmap;
  struct doca_buf_inventory *buf_inv;
  struct doca_pe *pe;

  char *buffer;
  size_t buffer_size;

  size_t buf_inv_size;

  size_t num_completed_tasks;
};

struct dma_resources {
  struct dma_state state;
  struct doca_ctx *ctx;
  struct doca_dma *dma_ctx;
};

doca_error_t allocate_buffer(struct dma_state *state);

doca_error_t
free_dma_memcpy_task_buffers(struct doca_dma_task_memcpy *dma_task);

doca_error_t dma_task_free(struct doca_dma_task_memcpy *dma_task);

doca_error_t allocate_dma_tasks(struct dma_resources *resources,
                                struct doca_mmap *remote_mmap,
                                void *remote_addr, uint32_t num_tasks,
                                struct doca_dma_task_memcpy **tasks);

doca_error_t submit_dma_tasks(uint32_t num_tasks,
                              struct doca_dma_task_memcpy **tasks);

doca_error_t open_doca_device_with_pci(const char *pci_addr, tasks_check func,
                                       struct doca_dev **retval);

doca_error_t open_device(const char *pcie_addr, struct dma_state *state);

doca_error_t create_pe(struct dma_state *state);

doca_error_t create_mmap(struct dma_state *state,
                         enum doca_access_flag access_flag);

doca_error_t create_buf_inventory(struct dma_state *state);

doca_error_t create_dma_state(const char *pcie_addr, struct dma_state *state);

doca_error_t create_dma_dpu_resources(const char *pcie_addr,
                                      struct dma_resources *resources);

doca_error_t create_dma_host_state(const char *pcie_addr,
                                   struct dma_state *state);

void dma_state_cleanup(struct dma_state *state);

doca_error_t export_mmap_to_files(struct doca_mmap *mmap,
                                  const struct doca_dev *dev,
                                  const char *src_buffer,
                                  size_t src_buffer_size,
                                  const char *export_desc_file_path,
                                  const char *buffer_info_file_path);

#endif
