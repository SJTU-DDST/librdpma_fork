#pragma once

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <doca_buf_inventory.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_dma.h>
#include <doca_mmap.h>
#include <doca_pe.h>

#include "dma_client.hpp"
#include "level_hashing.hpp"

#define CACHE_SIZE 64

using bucket_id_t = int64_t;
using frame_id_t = int64_t;

class Dpu {
public:
  Dpu(const std::string &pcie_addr, uint64_t level = DEFAULT_START_LEVEL);

  ~Dpu();
  
  std::future<bool> FlushBucket(bucket_id_t bucket);
  std::future<frame_id_t> FetchBucket(bucket_id_t bucket);

private:
  uint64_t GenNextClientId();
  std::pair<size_t, size_t> GetClientBucket(bucket_id_t bucket) const;

public:
  std::vector<std::unique_ptr<DmaClient>> dma_client_;

  std::array<FixedBucket, CACHE_SIZE> cache_;
  size_t cache_size_;
  std::unordered_map<bucket_id_t, frame_id_t> bucket_table_;
  bucket_id_t bl_start_bucket_id_;
  bucket_id_t tl_start_bucket_id_;
  uint64_t level_;
  uint64_t next_client_id_;
};