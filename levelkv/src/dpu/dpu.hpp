#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include <doca_buf_inventory.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_dma.h>
#include <doca_mmap.h>
#include <doca_pe.h>

#include "level_hashing.hpp"

#define CACHE_SIZE 64

class Dpu {
public:
  using bucket_id_t = int64_t;
  using frame_id_t = int64_t;

  Dpu(const std::string &pcie_addr);

  ~Dpu();

private:
  std::string pcie_addr_;
  doca_dev *dev_;
  std::vector<doca_mmap *> tl_mmaps_;
  std::vector<doca_mmap *> bl_mmaps_;
  std::vector<doca_mmap *> remote_tl_mmaps_;
  std::vector<doca_mmap *> remote_bl_mmaps_;
  std::vector<doca_buf_inventory *> invs_;
  std::vector<doca_pe *> pes_;
  std::vector<doca_dma *> doca_dmas_;
  std::vector<doca_ctx *> doca_ctxs_;

  std::array<LevelBucket, CACHE_SIZE> cache_;
  std::unordered_map<bucket_id_t, frame_id_t> bucket_table_;
};