#pragma once

#include <memory>
#include <string>
#include <vector>

#include <doca_dev.h>
#include <doca_mmap.h>

#include "level_hashing.hpp"
#include "dma_server.hpp"

class Host {
public:
  Host(const std::string &pcie_addr, uint64_t level);

  ~Host();

  void Run();
private:
  uint64_t GetNextServerId();
private:
  std::unique_ptr<FixedHashTable> level_ht_;
  std::vector<std::unique_ptr<DmaServer>> tl_dma_server_;
  std::vector<std::unique_ptr<DmaServer>> bl_dma_server_;
  uint64_t next_server_id_;
};