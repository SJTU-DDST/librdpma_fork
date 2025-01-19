#pragma once

#include <future>
#include <queue>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <doca_buf_inventory.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_dma.h>
#include <doca_mmap.h>
#include <doca_pe.h>

#include "dma_common.hpp"
#include "comch_common.hpp"

#define RECV_BUF_SIZE (512)

class DmaClient {
public:
  DmaClient(int64_t dma_client_id, const std::string &pcie_addr, char *addr,
            size_t len);
  ~DmaClient();
  void ImportFromFile();
  void Import(const ComchMsgExportMmap &msg);
  std::future<bool> ScheduleReadWrite(bool is_write, size_t src_offset,
                                  size_t dst_offset, size_t len);
  void Stop();

private:
  void ProcessQueue();

private:
  int64_t dma_client_id_;
  std::string pcie_addr_;
  char *addr_;
  char *remote_addr_;
  size_t len_;
  size_t remote_len_;

  std::queue<DmaRequest> queue;
  std::thread thread_;
  std::mutex mtx_;
  std::condition_variable cv_;
  std::atomic<bool> stop_;


  doca_dev *dev_;
  doca_mmap *mmap_;
  doca_mmap *remote_mmap_;
  doca_buf_inventory *inv_;
  doca_pe *pe_;
  doca_dma *dma_;
  doca_ctx *ctx_;
};