#include "dpu.hpp"
#include "dma_common.hpp"
#include "hash.hpp"
#include "utils.hpp"

Dpu::Dpu(const std::string &pcie_addr, uint64_t level)
    : stop_(false), level_(level), next_client_id_(0),
      replacer_(std::make_unique<Replacer>()) {
  ENSURE(level >= 3, "Starting level should be at least 3");
  addr_capacity_ = 1 << level;
  bl_capacity_ = 1 << (level - 1);
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
  GenSeeds();
  dirty_flag_.fill(0);
  worker_ = std::thread(&Dpu::Run, this);
  bucket_ids_.fill(-1);
  for (frame_id_t i = 0; i < CACHE_SIZE; i++) {
    free_list_.push_back(i);
  }
}

Dpu::~Dpu() {
  stop_ = true;
  worker_.join();
}

void Dpu::Search(const FixedKey &key,
                 std::function<void(std::optional<std::string>)> callback) {
  auto hash1 = key.Hash(f_seed_);
  auto hash2 = key.Hash(s_seed_);
  Request request = {RequestType::SEARCH, key, "", hash1, hash2, callback};
  {
    std::unique_lock<std::mutex> lock(request_queue_mtx_);
    request_queue_.push(request);
  }
  cv_.notify_one();
}

void Dpu::Insert(const FixedKey &key, const FixedValue &value) {
  auto hash1 = key.Hash(f_seed_);
  auto hash2 = key.Hash(s_seed_);
  Request request = {RequestType::INSERT, key, value, hash1, hash2, nullptr};
  {
    std::unique_lock<std::mutex> lock(request_queue_mtx_);
    request_queue_.push(request);
  }
  cv_.notify_one();
}

void Dpu::Delete(const FixedKey &key) {
  auto hash1 = key.Hash(f_seed_);
  auto hash2 = key.Hash(s_seed_);
  Request request = {RequestType::DELETE, key, "", hash1, hash2, nullptr};
  {
    std::unique_lock<std::mutex> lock(request_queue_mtx_);
    request_queue_.push(request);
  }
  cv_.notify_one();
}

std::future<bool> Dpu::FlushBucket(frame_id_t frame) {
  auto bucket = bucket_ids_[frame];
  ENSURE(bucket > 0, "FlushBucket: Frame id must >= 0");
  auto [client, offset] = GetClientBucket(bucket);
  return dma_client_[client]->ScheduleReadWrite(
      true, offset, frame * sizeof(FixedBucket), sizeof(FixedBucket));
}

std::future<bool> Dpu::FetchBucket(bucket_id_t bucket, frame_id_t *frame) {
  frame_id_t target_frame = -1;
  std::future<bool> f;
  bool need_flush = false;
  if (free_list_.empty()) {
    replacer_->Evict(&target_frame);
    if (dirty_flag_[target_frame]) {
      f = FlushBucket(target_frame);
      dirty_flag_[target_frame] = 0;
      need_flush = true;
    }
  } else {
    target_frame = free_list_.front();
    free_list_.pop_front();
  }
  ENSURE(target_frame >= 0, "FetchBucket: Frame id must >= 0");
  bucket_ids_[target_frame] = bucket;
  replacer_->RecordAccess(target_frame);
  auto [client, offset] = GetClientBucket(bucket);
  if (need_flush)
    f.wait();
  *frame = target_frame;
  return dma_client_[client]->ScheduleReadWrite(
      false, offset, target_frame * sizeof(FixedBucket), sizeof(FixedBucket));
}

void Dpu::DebugPrintCache() const {
  for (size_t i = 0; i < CACHE_SIZE; i++) {
    std::cout << "Cache frame: " << i << std::endl;
    cache_[i].DebugPrint();
    std::cout << std::endl;
  }
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

bool Dpu::ProcessSearch(const Request &request, FixedValue &result) {
  std::cout << "Process Search...\n";

  size_t idx1 = request.hash1_ % (addr_capacity_ / 2);
  size_t idx2 = addr_capacity_ / 2 + request.hash2_ % (addr_capacity_ / 2);
  size_t idx3 = request.hash1_ % (addr_capacity_ / 4);
  size_t idx4 = addr_capacity_ / 4 + request.hash2_ % (addr_capacity_ / 4);

  std::cout << "Four idx: " << idx1 << " " << idx2 << " " << idx3 << " " << idx4
            << std::endl;

  std::array<bucket_id_t, 4> bs = {
      addr_capacity_ - 4 + idx1, addr_capacity_ - 4 + idx2,
      bl_capacity_ - 4 + idx3, bl_capacity_ - 4 + idx4};

  std::vector<bucket_id_t> fetches;
  std::vector<frame_id_t> fetched_frames(bs.size(), -1);
  std::vector<std::future<bool>> futures;

  for (auto b : bs) {
    if (auto it = bucket_table_.find(b); it != bucket_table_.end()) {
      std::cout << b << " found in the cache\n";
      replacer_->RecordAccess(it->second);
      auto found = cache_[it->second].SearchSlot(request.key_, &result);
      if (found) {
        return true;
      }
    } else {
      fetches.push_back(b);
      std::cout << b << " not found in the cache\n";
    }
  }
  for (size_t i = 0; i < fetches.size(); i++) {
    futures.push_back(FetchBucket(fetches[i], &fetched_frames[i]));
  }

  bool found = false;
  for (size_t i = 0; i < fetches.size(); i++) {
    futures[i].wait();
    if (!found) {
      if (fetched_frames[i] != -1) {
        bucket_table_[fetches[i]] = fetched_frames[i];
        found = cache_[fetched_frames[i]].SearchSlot(request.key_, &result);
      }
    }
  }
  return found;
}

bool Dpu::ProcessInsert(const Request &request) {
  std::cout << "Process Insert...\n";
  size_t idx1 = request.hash1_ % (addr_capacity_ / 2);
  size_t idx2 = addr_capacity_ / 2 + request.hash2_ % (addr_capacity_ / 2);
  size_t idx3 = request.hash1_ % (addr_capacity_ / 4);
  size_t idx4 = addr_capacity_ / 4 + request.hash2_ % (addr_capacity_ / 4);
  std::cout << "Four idx: " << idx1 << " " << idx2 << " " << idx3 << " " << idx4
            << std::endl;
  std::array<bucket_id_t, 4> bs = {
      addr_capacity_ - 4 + idx1, addr_capacity_ - 4 + idx2,
      bl_capacity_ - 4 + idx3, bl_capacity_ - 4 + idx4};
  std::vector<std::future<bool>> futures;
  std::array<frame_id_t, 2> frames;
  for (size_t i = 0; i < 2; i++) {
    if (bucket_table_.count(bs[i]) == 0)
      futures.push_back(FetchBucket(bs[i], &frames[i]));
    else
      replacer_->RecordAccess(bucket_table_[bs[i]]);
  }
  futures[0].wait();
  futures[1].wait();
  bucket_table_[bs[0]] = frames[0];
  bucket_table_[bs[1]] = frames[1];
  bool finished = false;
  if (cache_[frames[0]].Count() > cache_[frames[1]].Count()) {
    finished = cache_[frames[1]].InsertSlot(request.key_, request.value_,
                                            dirty_flag_[frames[1]]);
    if (finished) {
      std::cout << "Inserted to bucket: " << bs[1] << "   frame: " << frames[1]
                << std::endl;
      return true;
    }
  } else {
    finished = cache_[frames[0]].InsertSlot(request.key_, request.value_,
                                            dirty_flag_[frames[0]]);
    if (finished) {
      std::cout << "Inserted to bucket: " << bs[0] << "   frame: " << frames[0]
                << std::endl;
      return true;
    }
  }

  // if (finished)
  //   return true;

  futures.clear();
  for (size_t i = 0; i < 2; i++) {
    if (bucket_table_.count(bs[i + 2]) == 0)
      futures.push_back(FetchBucket(bs[i + 2], &frames[i]));
    else
      replacer_->RecordAccess(bucket_table_[bs[i + 2]]);
  }
  futures[0].wait();
  futures[1].wait();
  bucket_table_[bs[3]] = frames[0];
  bucket_table_[bs[4]] = frames[1];
  if (cache_[frames[0]].Count() > cache_[frames[1]].Count())
    finished = cache_[frames[1]].InsertSlot(request.key_, request.value_,
                                            dirty_flag_[frames[1]]);
  else
    finished = cache_[frames[0]].InsertSlot(request.key_, request.value_,
                                            dirty_flag_[frames[0]]);

  return finished;
}

void Dpu::Run() {
  while (!stop_) {
    FixedValue result_value;
    Request request;
    {
      std::unique_lock<std::mutex> lock(request_queue_mtx_);
      cv_.wait(lock, [this] { return !request_queue_.empty() || stop_; });
      if (stop_)
        break;
      request = request_queue_.front();
      request_queue_.pop();
    }
    switch (request.request_type_) {
    case RequestType::SEARCH: {
      auto result = ProcessSearch(request, result_value);
      if (result)
        request.callback_(result ? std::make_optional(result_value.ToString())
                                 : std::nullopt);
      break;
    }
    case RequestType::INSERT: {
      ProcessInsert(request);
      break;
    }
    case RequestType::DELETE: {
      break;
    }
    }
  }
}

void Dpu::GenSeeds() {
  srand(time(NULL));
  do {
    f_seed_ = rand();
    s_seed_ = rand();
    f_seed_ = f_seed_ << (rand() % 63);
    s_seed_ = s_seed_ << (rand() % 63);
  } while (f_seed_ == s_seed_);
}