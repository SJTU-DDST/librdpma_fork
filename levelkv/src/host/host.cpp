#include "host.hpp"
#include "dma_common.hpp"

Host::Host(const std::string &pcie_addr, uint64_t level)
    : level_ht_(std::make_unique<FixedHashTable>(level)), next_server_id_(0) {
  bl_dma_server_.resize(THREADS / 2);
  tl_dma_server_.resize(THREADS / 2);

  char *ptr = (char *)level_ht_->buckets_[1];
  size_t mmap_size =
      level_ht_->bl_capacity_ / (THREADS / 2) * sizeof(FixedBucket);
  for (size_t i = 0; i < THREADS / 2; i++) {
    auto id = GetNextServerId();
    bl_dma_server_[i] =
        std::make_unique<DmaServer>(id, pcie_addr, ptr, mmap_size);
    bl_dma_server_[i]->ExportMmapToFile();
    ptr += mmap_size;
  }
  ptr = (char *)level_ht_->buckets_[0];
  mmap_size = level_ht_->addr_capacity_ / (THREADS / 2) * sizeof(FixedBucket);
  for (size_t i = 0; i < THREADS / 2; i++) {
    auto id = GetNextServerId();
    tl_dma_server_[i] =
        std::make_unique<DmaServer>(id, pcie_addr, ptr, mmap_size);
    tl_dma_server_[i]->ExportMmapToFile();
    ptr += mmap_size;
  }

  level_ht_->buckets_[0][0].SetSlot(0, FixedKey("ddst"), FixedValue("tsdd"));
  level_ht_->buckets_[0][0].SetSlot(3, FixedKey(1234), FixedValue(-999));
}

Host::~Host() {}

void Host::Run() {
  while (true) {
    if (getchar() == 'q' || getchar() == 'Q')
      return;
  }
}

uint64_t Host::GetNextServerId() {
  auto ret = next_server_id_;
  next_server_id_++;
  return ret;
}
