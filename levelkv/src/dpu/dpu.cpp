#include "dpu.hpp"
#include "dma_common.hpp"
#include "hash.hpp"
#include "utils.hpp"

Dpu::Dpu(const std::string &pcie_addr, uint64_t level)
    : stop_(false), level_(level), next_client_id_(0) {
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
}

Dpu::~Dpu() {
  stop_ = true;
  worker_.join();
}

void Dpu::Search(const FixedKey &key,
                 std::function<void(std::optional<std::string>)> callback) {
  Request request = {SEARCH, key, "", callback};
  {
    std::unique_lock<std::mutex> lock(request_queue_mtx_);
    request_queue_.push(request);
  }
  cv_.notify_one();
}

void Dpu::Update(const FixedKey &key, const FixedValue &value) {
  Request request = {UPDATE, key, value, nullptr};
  {
    std::unique_lock<std::mutex> lock(request_queue_mtx_);
    request_queue_.push(request);
  }
  cv_.notify_one();
}

void Dpu::Delete(const FixedKey &key) {
  Request request = {DELETE, key, "", nullptr};
  {
    std::unique_lock<std::mutex> lock(request_queue_mtx_);
    request_queue_.push(request);
  }
  cv_.notify_one();
}

std::future<frame_id_t> Dpu::FetchBucket(bucket_id_t bucket) {
  frame_id_t frame = tmp;
  auto [client, offset] = GetClientBucket(bucket);
  auto future_bool = dma_client_[client]->ScheduleReadWrite(
      false, offset, frame * sizeof(FixedBucket), sizeof(FixedBucket));
  tmp++;
  return std::async(std::launch::deferred, [frame, &future_bool]() {
    bool res = future_bool.get();
    return res ? frame : (frame_id_t)-1;
  });
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

void Dpu::Run() {
  while (!stop_) {
    FixedValue result_value;
    bool finished = false;
    Request request;
    {
      std::unique_lock<std::mutex> lock(request_queue_mtx_);
      cv_.wait(lock, [this] { return !request_queue_.empty() || stop_; });
      if (stop_)
        break;
      request = request_queue_.front();
      request_queue_.pop();
    }
    std::cout << "Request type: " << request.request_type_ << std::endl;
    auto hash1 = request.key.Hash(f_seed_);
    auto hash2 = request.key.Hash(s_seed_);
    size_t idx1 = hash1 % (addr_capacity_ / 2);
    size_t idx2 = addr_capacity_ / 2 + hash2 % (addr_capacity_ / 2);
    ENSURE(idx1 >= 0 && idx1 < addr_capacity_ && idx2 >= 0 &&
               idx2 < addr_capacity_,
           "Level hashing idx error");
    std::cout << "Hash to idx: " << idx1 << ", " << idx2 << std::endl;
    std::array<bucket_id_t, 4> bs = {
        addr_capacity_ - 4 + idx1, addr_capacity_ - 4 + idx2,
        bl_capacity_ - 4 + idx1 / 2, bl_capacity_ - 4 + idx2 / 2};
    std::cout << "Target buckets: ";
    for (const auto &b : bs) std::cout << b << " ";
    std::cout << std::endl;
    std::vector<std::future<frame_id_t>> futures;
    for (size_t b = 0; b < 4; b++) {
      std::cout << "Looking for bucket: " << bs[b] << std::endl;
      if (auto it = bucket_table_.find(bs[b]); it != bucket_table_.end()) {
        std::cout << "Bucket " << bs[b] << " is cached at frame: " << it->second << std::endl;
        if (request.request_type_ == SEARCH) {
          finished = cache_[it->second].SearchSlot(request.key, &result_value);
        } else if (request.request_type_ == UPDATE) {
          finished = cache_[it->second].UpdateSlot(request.key, request.value);
        } else if (request.request_type_ == DELETE) {
          finished = cache_[it->second].DeleteSlot(request.key);
        } else {
          ENSURE(0, "Unsupported request type!");
        }
        if (finished) {
          if (request.request_type_ == SEARCH)
            request.callback_(result_value.ToString());
          else
            dirty_flag_[it->second] = 1;
          break;
        }
      } else {
        futures.push_back(FetchBucket(bs[b]));
      }
    }
    for (auto &f : futures) {
      auto frame = f.get();
      if (!finished) {
        if (request.request_type_ == SEARCH) {
          finished = cache_[frame].SearchSlot(request.key, &result_value);
        } else if (request.request_type_ == UPDATE) {
          finished = cache_[frame].UpdateSlot(request.key, request.value);
        } else if (request.request_type_ == DELETE) {
          finished = cache_[frame].DeleteSlot(request.key);
        } else {
          ENSURE(0, "Unsupported request type!");
        }
        if (finished) {
          if (request.request_type_ == SEARCH)
            request.callback_(result_value.ToString());
          else
            dirty_flag_[frame] = 1;
        }
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