#include "dma_client.hpp"
#include "dma_common.hpp"
#include "utils.hpp"
#include <climits>

DmaClient::DmaClient(int64_t dma_client_id, const std::string &pcie_addr,
                     char *addr, size_t len)
    : dma_client_id_(dma_client_id), pcie_addr_(pcie_addr), addr_(addr),
      len_(len), stop_(false) {
  dev_ = open_device(pcie_addr);
  mmap_ = create_mmap(addr, len, dev_);
  inv_ = create_inv();
  pe_ = create_pe();
  dma_ = create_dma(dev_);
  ctx_ = create_ctx(dma_, pe_, (char *)this);

  thread_ = std::thread(&DmaClient::ProcessQueue, this);
}

DmaClient::~DmaClient() {
  Stop();
  if (remote_mmap_) {
    doca_mmap_stop(remote_mmap_);
    doca_mmap_destroy(remote_mmap_);
  }
  if (ctx_)
    doca_ctx_stop(ctx_);
  if (dma_)
    doca_dma_destroy(dma_);
  if (pe_)
    doca_pe_destroy(pe_);
  if (inv_)
    doca_buf_inventory_destroy(inv_);
  if (mmap_) {
    doca_mmap_stop(mmap_);
    doca_mmap_destroy(mmap_);
  }
  if (dev_)
    doca_dev_close(dev_);
}

void DmaClient::ImportFromFile() {
  size_t export_desc_len, remote_addr_len;
  void *export_desc;
  void *remote_addr;
  const std::string export_desc_file_path =
      export_desc_file_name_base + std::to_string(dma_client_id_) + ".txt";
  const std::string buffer_info_file_path =
      buffer_info_file_name_base + std::to_string(dma_client_id_) + ".txt";
  FILE *fp;
  long file_size;
  char buffer[RECV_BUF_SIZE];
  size_t convert_value;

  fp = fopen(export_desc_file_path.c_str(), "r");
  ENSURE(fp, "Failed to open exported desc file");

  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    ENSURE(0, "Failed to read exported desc file");
  }

  file_size = ftell(fp);
  if (file_size == -1) {
    fclose(fp);
    ENSURE(0, "Failed to read exported desc file");
  }

  if (file_size > RECV_BUF_SIZE)
    file_size = RECV_BUF_SIZE;

  export_desc_len = file_size;

  if (fseek(fp, 0L, SEEK_SET) != 0) {
    fclose(fp);
    ENSURE(0, "Failed to read exported desc file");
  }

  if (fread(export_desc, 1, file_size, fp) != (size_t)file_size) {
    fclose(fp);
    ENSURE(0, "Failed to read exported desc file");
  }

  fclose(fp);

  /* Read source buffer information from file */
  fp = fopen(buffer_info_file_path.c_str(), "r");
  ENSURE(fp, "Failed to open exported desc file");

  /* Get source buffer address */
  if (fgets(buffer, RECV_BUF_SIZE, fp) == NULL) {
    fclose(fp);
    ENSURE(0, "Failed to read buf info");
  }
  convert_value = strtoull(buffer, NULL, 0);
  if (convert_value == ULLONG_MAX) {
    fclose(fp);
    ENSURE(0, "Failed to read buf info");
  }
  remote_addr = (void *)convert_value;

  memset(buffer, 0, RECV_BUF_SIZE);

  /* Get source buffer length */
  if (fgets(buffer, RECV_BUF_SIZE, fp) == NULL) {
    fclose(fp);
    ENSURE(0, "Failed to read buf info");
  }
  convert_value = strtoull(buffer, NULL, 0);
  if (convert_value == ULLONG_MAX) {
    fclose(fp);
    ENSURE(0, "Failed to read buf info");
  }
  remote_addr_len = convert_value;

  fclose(fp);
  remote_addr_ = (char *)remote_addr;
  remote_len_ = remote_addr_len;
  doca_mmap_create_from_export(nullptr, export_desc, export_desc_len, dev_,
                               &remote_mmap_);
}

std::future<bool> DmaClient::ScheduleFlush(bool is_write, size_t src_offset,
                                           size_t dst_offset, size_t len) {
  auto promise = std::make_shared<std::promise<bool>>();
  DmaRequest request{is_write, src_offset, dst_offset, len, promise};
  {
    std::unique_lock<std::mutex> lock(mtx_);
    queue.push(request);
  }
  cv_.notify_one();
  return promise->get_future();
}

void DmaClient::Stop() {
  stop_ = true;
  cv_.notify_one();
  if (thread_.joinable())
    thread_.join();
}

void DmaClient::ProcessQueue() {
  while (!stop_) {
    DmaRequest request;
    {
      std::unique_lock<std::mutex> lock(mtx_);
      cv_.wait(lock, [this] { return !queue.empty() || stop_; });
      if (stop_)
        break;
      request = queue.front();
      queue.pop();
    }
    uint64_t finish = 0u;
    doca_buf *src_buf;
    doca_buf *dst_buf;
    if (request.is_write_) {
      src_buf = create_buf_by_data(inv_, mmap_, addr_ + request.src_offset_,
                                   request.len_);
      dst_buf = create_buf_by_addr(
          inv_, remote_mmap_, remote_addr_ + request.dst_offset_, request.len_);
    } else {
      src_buf = create_buf_by_data(
          inv_, remote_mmap_, remote_addr_ + request.src_offset_, request.len_);
      dst_buf = create_buf_by_addr(inv_, mmap_, addr_ + request.dst_offset_,
                                   request.len_);
    }
    doca_dma_task_memcpy *task =
        create_dma_task(dma_, src_buf, dst_buf, &finish);
    doca_task_submit(doca_dma_task_memcpy_as_task(task));
    while (!finish)
      doca_pe_progress(pe_);
    if (finish == TASK_FINISH_SUCCESS)
      request.promise_->set_value(true);
    else
      request.promise_->set_value(false);
    doca_task_free(doca_dma_task_memcpy_as_task(task));
  }
}
