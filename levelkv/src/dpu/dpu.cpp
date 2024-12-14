#include "dpu.hpp"
#include "dma_common.hpp"

Dpu::Dpu(const std::string &pcie_addr) : pcie_addr_(pcie_addr) {
  memset((void *)cache_.data(), 0, cache_.size() * sizeof(LevelBucket));
  for (size_t i = 0; i < THREADS / 2; i++) {
    dev_ = open_device(pcie_addr);
    bl_mmaps_[i] = create_mmap((void *)cache_.data(),
                               cache_.size() * sizeof(LevelBucket), dev_);
    tl_mmaps_[i] = create_mmap((void *)cache_.data(),
                               cache_.size() * sizeof(LevelBucket), dev_);
  }
  for (size_t i = 0; i < THREADS; i++) {
    invs_[i] = create_inv();
    pes_[i] = create_pe();
  }
}
