#pragma once

#include <future>
#include <iostream>
#include <memory>
#include <string>

#include <doca_buf.h>
#include <doca_buf_inventory.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_dma.h>
#include <doca_mmap.h>
#include <doca_pe.h>

#define THREADS 8
#define TASK_FINISH_SUCCESS 1
#define TASK_FINISH_ERROR 2

typedef doca_error_t (*tasks_check)(
    doca_devinfo *); // Function to check if a given device is capable of
                     // executing some task

const std::string export_desc_file_name_base = "export_desc_";
const std::string buffer_info_file_name_base = "buffer_info_";

struct DmaRequest {
  bool is_write_;
  size_t src_offset_;
  size_t dst_offset_;
  size_t len_;
  std::shared_ptr<std::promise<bool>> promise_;
};

doca_error_t open_doca_device_with_pci(const char *pci_addr, tasks_check func,
                                       doca_dev **retval);

doca_dev *open_device(const std::string &pcie_addr);

doca_mmap *create_mmap(void *addr, size_t len, doca_dev *dev);

doca_buf_inventory *create_inv();

doca_pe *create_pe();

doca_dma *create_dma(doca_dev *dev);

doca_ctx *create_ctx(doca_dma *dma, doca_pe *pe, char *ctx_user_data_ptr);

doca_buf *create_buf_by_data(doca_buf_inventory *inv, doca_mmap *mmap,
                             void *addr, size_t len);

doca_buf *create_buf_by_addr(doca_buf_inventory *inv, doca_mmap *mmap,
                             void *addr, size_t len);

doca_dma_task_memcpy *create_dma_task(doca_dma *dma, doca_buf *src,
                                      doca_buf *dst, uint64_t *finish);

void free_dma_task_buf(struct doca_dma_task_memcpy *task);

doca_error_t export_mmap_to_files(doca_mmap *mmap, const doca_dev *dev,
                                  const char *src_buffer,
                                  size_t src_buffer_size,
                                  const std::string &export_desc_file_path,
                                  const std::string &buffer_info_file_path);