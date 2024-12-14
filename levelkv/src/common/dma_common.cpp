#include "dma_common.hpp"
#include "utils.hpp"

static doca_error_t check_dev_dma_capable(doca_devinfo *devinfo) {
  doca_error_t status = doca_dma_cap_task_memcpy_is_supported(devinfo);
  if (status != DOCA_SUCCESS)
    return status;
  return DOCA_SUCCESS;
}

static void dma_memcpy_completed_callback(struct doca_dma_task_memcpy *dma_task,
                                          union doca_data task_user_data,
                                          union doca_data ctx_user_data) {

  std::cout << "task completed\n";
  uint64_t *finish = (uint64_t *)task_user_data.ptr;
  *finish = TASK_FINISH_SUCCESS;
  free_dma_task_buf(dma_task);
}

static void dma_memcpy_error_callback(struct doca_dma_task_memcpy *dma_task,
                                      union doca_data task_user_data,
                                      union doca_data ctx_user_data) {
  std::cout << "task failed\n";
  uint64_t *finish = (uint64_t *)task_user_data.ptr;
  *finish = TASK_FINISH_ERROR;
  free_dma_task_buf(dma_task);
}

doca_error_t open_doca_device_with_pci(const char *pci_addr, tasks_check func,
                                       doca_dev **retval) {
  doca_devinfo **dev_list;
  uint32_t nb_devs;
  uint8_t is_addr_equal = 0;
  doca_error_t res;
  size_t i;
  *retval = NULL;
  res = doca_devinfo_create_list(&dev_list, &nb_devs);
  if (res != DOCA_SUCCESS)
    return res;

  for (i = 0; i < nb_devs; i++) {
    res = doca_devinfo_is_equal_pci_addr(dev_list[i], pci_addr, &is_addr_equal);
    if (res == DOCA_SUCCESS && is_addr_equal) {
      if (func != NULL && func(dev_list[i]) != DOCA_SUCCESS)
        continue;
      res = doca_dev_open(dev_list[i], retval);
      if (res == DOCA_SUCCESS) {
        doca_devinfo_destroy_list(dev_list);
        return res;
      }
    }
  }
  res = DOCA_ERROR_NOT_FOUND;
  doca_devinfo_destroy_list(dev_list);
  return res;
}

doca_dev *open_device(const std::string &pcie_addr) {
  doca_dev *dev;
  open_doca_device_with_pci(pcie_addr.c_str(), check_dev_dma_capable, &dev);
  ENSURE(dev, "Failed to open doca dev with pci");
  return dev;
}

doca_mmap *create_mmap(void *addr, size_t len, doca_dev *dev) {
  doca_mmap *mmap;
  doca_mmap_create(&mmap);
  ENSURE(mmap, "Failed to create mmap");
  doca_mmap_set_memrange(mmap, addr, len);
  doca_mmap_add_dev(mmap, dev);
  doca_mmap_set_permissions(mmap, DOCA_ACCESS_FLAG_PCI_READ_WRITE);
  doca_mmap_start(mmap);
  return mmap;
}

doca_buf_inventory *create_inv() {
  doca_buf_inventory *inv;
  doca_buf_inventory_create(1024, &inv);
  ENSURE(inv, "Failed to create buf inventory");
  doca_buf_inventory_start(inv);
  return inv;
}

doca_pe *create_pe() {
  doca_pe *pe;
  doca_pe_create(&pe);
  ENSURE(pe, "Failed to create pe");
  return pe;
}

doca_dma *create_dma(doca_dev *dev) {
  doca_dma *dma;
  doca_dma_create(dev, &dma);
  ENSURE(dma, "Failed to create dma");
  doca_dma_task_memcpy_set_conf(dma, dma_memcpy_completed_callback,
                                dma_memcpy_error_callback, 8);
  return dma;
}

doca_ctx *create_ctx(doca_dma *dma, doca_pe *pe, char *ctx_user_data_ptr) {
  doca_data ctx_user_data;
  ctx_user_data.ptr = (void *)ctx_user_data_ptr;
  doca_ctx *ctx = doca_dma_as_ctx(dma);
  doca_pe_connect_ctx(pe, ctx);
  doca_ctx_set_user_data(ctx, ctx_user_data);
  doca_ctx_start(ctx);
}

doca_buf *create_buf_by_data(doca_buf_inventory *inv, doca_mmap *mmap,
                             void *addr, size_t len) {
  doca_buf *buf;
  doca_buf_inventory_buf_get_by_data(inv, mmap, addr, len, &buf);
  ENSURE(buf, "Failed to create buf by data");
  return buf;
}

doca_buf *create_buf_by_addr(doca_buf_inventory *inv, doca_mmap *mmap,
                             void *addr, size_t len) {
  doca_buf *buf;
  doca_buf_inventory_buf_get_by_addr(inv, mmap, addr, len, &buf);
  ENSURE(buf, "Failed to create buf by addr");
  return buf;
}

doca_dma_task_memcpy *create_dma_task(doca_dma *dma, doca_buf *src, doca_buf *dst,
                           uint64_t *finish) {
  doca_dma_task_memcpy *task;
  doca_data task_user_data;
  task_user_data.ptr = (void *)finish;
  doca_dma_task_memcpy_alloc_init(dma, src, dst, task_user_data, &task);
  ENSURE(task, "Failed to create task");
  return task;
}

void free_dma_task_buf(struct doca_dma_task_memcpy *task) {
  const struct doca_buf *src = doca_dma_task_memcpy_get_src(task);
  struct doca_buf *dst = doca_dma_task_memcpy_get_dst(task);
  doca_buf_dec_refcount((doca_buf *)src, NULL);
  doca_buf_dec_refcount(dst, NULL);
}

doca_error_t export_mmap_to_files(doca_mmap *mmap, const doca_dev *dev,
                                  const char *src_buffer,
                                  size_t src_buffer_size,
                                  const std::string &export_desc_file_path,
                                  const std::string &buffer_info_file_path) {
  const void *export_desc;
  size_t export_desc_len;
  doca_mmap_export_pci(mmap, dev, &export_desc, &export_desc_len);
  FILE *fp = fopen(export_desc_file_path.c_str(), "wb");
  uint64_t buffer_addr = (uintptr_t)src_buffer;
  uint64_t buffer_len = (uint64_t)src_buffer_size;
  fwrite(export_desc, 1, export_desc_len, fp);
  fclose(fp);
  fp = fopen(buffer_info_file_path.c_str(), "w");
  fprintf(fp, "%" PRIu64 "\n", buffer_addr);
  fprintf(fp, "%" PRIu64 "", buffer_len);
  fclose(fp);
  return DOCA_SUCCESS;
}
