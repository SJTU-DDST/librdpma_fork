#include <cstring>

#include "dma_common.hpp"
#include "host.hpp"

Host::Host(const std::string &pcie_addr, uint64_t level)
    : level_ht_(std::make_unique<FixedHashTable>(level)), next_server_id_(0) {
  memset(level_ht_->buckets_[0], 0,
         sizeof(FixedBucket) * level_ht_->addr_capacity_);
  memset(level_ht_->buckets_[1], 0,
         sizeof(FixedBucket) * level_ht_->bl_capacity_);
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

  // for (size_t i = 0; i < level_ht_->addr_capacity_; i++) {
  //   level_ht_->buckets_[0][i].SetSlot(0, i, i * 10);
  //   level_ht_->buckets_[0][i].SetSlot(1, i, i * 100);
  //   level_ht_->buckets_[0][i].SetSlot(2, i, i * 1000);
  //   level_ht_->buckets_[0][i].SetSlot(3, i, i * 10000);
  // }
  // for (size_t i = 0; i < level_ht_->bl_capacity_; i++) {
  //   level_ht_->buckets_[1][i].SetSlot(0, i, i * 10);
  //   level_ht_->buckets_[1][i].SetSlot(1, i, i * 100);
  //   level_ht_->buckets_[1][i].SetSlot(2, i, i * 1000);
  //   level_ht_->buckets_[1][i].SetSlot(3, i, i * 10000);
  // }
  comch_cfg_ = comch_init("Comch Client", "b5:00.0", "", this);
}

Host::~Host() {}

void Host::Rehash() { std::cout << "Rehash is called \n"; }

void Host::DebugPrint() const {
  std::cout << "************************************************\nTL: \n";
  for (size_t i = 0; i < level_ht_->addr_capacity_; i++) {
    level_ht_->buckets_[0][i].DebugPrint();
  }
  std::cout << "\nBL: \n";
  for (size_t i = 0; i < level_ht_->bl_capacity_; i++) {
    level_ht_->buckets_[1][i].DebugPrint();
  }
  std::cout << "************************************************\n\n";
}

void Host::Run() {
  while (true) {
    auto c = getchar();
    if (c == 'q' || c == 'Q')
      return;
    else if (c == 'p' || c == 'P')
      DebugPrint();
  }
}

uint64_t Host::GetNextServerId() {
  auto ret = next_server_id_;
  next_server_id_++;
  return ret;
}
