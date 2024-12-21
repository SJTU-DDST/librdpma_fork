#include "dpu.hpp"
#include "dma_common.hpp"
#include "utils.hpp"

Dpu::Dpu(const std::string &pcie_addr, uint64_t level)
    : level_(level), next_client_id_(0) {
  ENSURE(level >= 3, "Starting level should be at least 3");
  bl_start_bucket_id_ = (1 << (level - 1)) - 4;
  tl_start_bucket_id_ = (1 << level) - 4;
  cache_size_ = cache_.size() * sizeof(FixedBucket);
  std::cout << "Bucket size: " << sizeof(FixedBucket) << std::endl;
  memset((void *)cache_.data(), 0, cache_size_);
  dma_client_.resize(THREADS);
  for (size_t i = 0; i < THREADS; i++) {
    auto id = GenNextClientId();
    dma_client_[i] = std::make_unique<DmaClient>(
        id, pcie_addr, (char *)cache_.data(), cache_size_);
    dma_client_[i]->ImportFromFile();
  }
}

Dpu::~Dpu() {}

std::future<frame_id_t> Dpu::FetchBucket(bucket_id_t bucket) {
  frame_id_t frame = 0;
  auto [client, offset] = GetClientBucket(bucket);
  auto future_bool = dma_client_[client]->ScheduleReadWrite(
      false, offset, frame * sizeof(FixedBucket), sizeof(FixedBucket));
  return std::async(std::launch::deferred, [frame, &future_bool]() {
    bool res = future_bool.get();
    return res ? frame : (frame_id_t)-1;
  });
}

uint64_t Dpu::GenNextClientId() {
  auto ret = next_client_id_;
  next_client_id_++;
  return ret;
}

std::pair<size_t, size_t> Dpu::GetClientBucket(bucket_id_t bucket) const {
  size_t client = 0;
  size_t offset = 0;
  if (bucket <= (1 << level_) - 4) {
    // Bucket is in bottom level
    size_t k = bucket - bl_start_bucket_id_;
    size_t g = (tl_start_bucket_id_ - bl_start_bucket_id_) / (THREADS / 2);
    client = k / g;
    offset = (k % g) * sizeof(FixedBucket);
  } else {
    // Bucket is in top level
    size_t k = bucket - tl_start_bucket_id_;
    size_t g = (tl_start_bucket_id_ - bl_start_bucket_id_) / (THREADS / 2) * 2;
    client = k / g + THREADS / 2;
    offset = (k % g) * sizeof(FixedBucket);
  }
  return std::make_pair(client, offset);
}
