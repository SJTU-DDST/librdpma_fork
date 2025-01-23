#include "dpu.hpp"
#include "dma_common.hpp"
#include "hash.hpp"
#include "utils.hpp"

static void comch_dpu_recv_callback(doca_comch_event_msg_recv *event,
                                    uint8_t *recv_buffer, uint32_t msg_len,
                                    doca_comch_connection *comch_connection) {
  // print_time();
  std::cout << "Dpu recv callback called... \n";
  // std::cout.write((char *)recv_buffer, msg_len);
  // std::cout << std::endl;

  auto ctx_user_data = doca_comch_connection_get_user_data(comch_connection);
  Comch *comch = reinterpret_cast<Comch *>(ctx_user_data.ptr);
  ENSURE(comch->is_server_, "Comch->is_server_ is not 1 on dpu");
  auto comch_user_data = comch->user_data_;
  Dpu *dpu = reinterpret_cast<Dpu *>(comch_user_data);
  ComchMsg *msg = reinterpret_cast<ComchMsg *>(recv_buffer);
  switch (msg->msg_type_) {
  case ComchMsgType::COMCH_MSG_EXPORT_DESCRIPTOR: {
    ENSURE(dpu->mmap_to_recv_ > 0,
           "New remote mmap received on dpu after its mmap_to_recv_ is zero");
    if (dpu->in_init_) {
      auto i = THREADS - dpu->mmap_to_recv_;
      dpu->dma_client_[i]->Import(msg->exp_msg_);
      dpu->mmap_to_recv_--;
      // std::cout << "Dpu successfully imported a remote mmap\n";
      if (dpu->mmap_to_recv_ == 0)
        dpu->in_init_ = false;
    } else if (dpu->in_rehash_) {
      auto i = THREADS + THREADS / 2 - dpu->mmap_to_recv_;
      dpu->dma_client_[i]->Import(msg->exp_msg_);
      dpu->mmap_to_recv_--;
    } else {
      ENSURE(0, "Dpu received export desc msg not during initialization and "
                "rehashing");
    }
    break;
  }
  case ComchMsgType::COMCH_MSG_EXPORT_SEEDS: {
    ENSURE(dpu->seed_recved_ == false, "Dpu received multiple seed msg");
    dpu->f_seed_ = msg->seed_msg_.f_seed_;
    dpu->s_seed_ = msg->seed_msg_.s_seed_;
    dpu->seed_recved_ = true;
    break;
  }

  default:
    ENSURE(0, "Dpu received unsupported comch msg type");
  }
}

Dpu::Dpu(const std::string &pcie_addr, const std::string &pcie_rep_addr,
         uint64_t level)
    : stop_(false), level_(level), next_client_id_(0), max_load_factor_(0.7),
      replacer_(std::make_unique<Replacer>()),
      dpu_comch_(std::make_unique<Comch>(
          true, "Comch", pcie_addr, pcie_rep_addr, comch_dpu_recv_callback,
          comch_send_completion, comch_send_completion_err,
          server_connection_cb, server_disconnection_cb, this)),
      seed_recved_(false), in_init_(true), in_rehash_(false), size_(0) {
  ENSURE(level >= 3, "Starting level should be at least 3");
  addr_capacity_ = 1 << level;
  bl_capacity_ = 1 << (level - 1);
  bl_start_bucket_id_ = (1 << (level - 1)) - 4;
  tl_start_bucket_id_ = (1 << level) - 4;
  cache_size_ = cache_.size() * sizeof(FixedBucket);
  memset((void *)cache_.data(), 0, cache_size_);
  dma_client_.resize(THREADS);
  for (size_t i = 0; i < THREADS; i++) {
    auto id = GenNextClientId();
    dma_client_[i] = std::make_unique<DmaClient>(
        id, pcie_addr, (char *)cache_.data(), cache_size_);
  }
  GenSeeds();
  dirty_flag_.fill(0);
  worker_ = std::thread(&Dpu::Run, this);
  bucket_ids_.fill(-1);
  for (frame_id_t i = 0; i < CACHE_SIZE; i++) {
    free_list_.push_back(i);
  }
  mmap_to_recv_ = THREADS;
  while (mmap_to_recv_ > 0 || seed_recved_ == false) {
    dpu_comch_->Progress();
  }
  std::cout << "Dpu initialization completed!\n";
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

void Dpu::Update(const FixedKey &key, const FixedValue &value) {
  auto hash1 = key.Hash(f_seed_);
  auto hash2 = key.Hash(s_seed_);
  Request request = {RequestType::UPDATE, key, value, hash1, hash2, nullptr};
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
  auto [client, offset] = GetClientBucket(bucket);
  return dma_client_[client]->ScheduleReadWrite(
      true, frame * sizeof(FixedBucket), offset, sizeof(FixedBucket));
}

std::future<std::pair<bool, frame_id_t>> Dpu::FetchBucket(bucket_id_t bucket) {
  frame_id_t target_frame = -1;
  bool need_flush = false;
  std::future<bool> flush_future;

  // Determine target frame
  if (free_list_.empty()) {
    replacer_->Evict(&target_frame);
    // std::cout << "Evict frame: " << target_frame << std::endl;
    if (dirty_flag_[target_frame]) {
      // std::cout << "Flush frame: " << target_frame << std::endl;
      flush_future = FlushBucket(target_frame);
      dirty_flag_[target_frame] = false;
      need_flush = true;
    }
  } else {
    target_frame = free_list_.front();
    free_list_.pop_front();
  }

  ENSURE(target_frame >= 0 && target_frame < CACHE_SIZE,
         "FetchBucket: target frame error");

  bucket_table_.erase(bucket_ids_[target_frame]);
  bucket_ids_[target_frame] = bucket;
  replacer_->RecordAccess(target_frame);

  auto [client, offset] = GetClientBucket(bucket);
  if (need_flush) {
    flush_future.wait();
  }

  // Schedule DMA read
  auto dma_future = dma_client_[client]->ScheduleReadWrite(
      false, offset, target_frame * sizeof(FixedBucket), sizeof(FixedBucket));
  return std::async(std::launch::deferred, [dma_future = std::move(dma_future),
                                            target_frame]() mutable {
    bool dma_success = dma_future.get();
    return std::make_pair(dma_success, target_frame);
  });
}

frame_id_t Dpu::NewBucket(bucket_id_t bucket) {
  frame_id_t target_frame = -1;
  bool need_flush = false;
  std::future<bool> flush_future;

  // Determine target frame
  if (free_list_.empty()) {
    replacer_->Evict(&target_frame);
    // std::cout << "Evict frame: " << target_frame << std::endl;
    if (dirty_flag_[target_frame]) {
      // std::cout << "Flush frame: " << target_frame << std::endl;
      flush_future = FlushBucket(target_frame);
      dirty_flag_[target_frame] = false;
      need_flush = true;
    }
  } else {
    target_frame = free_list_.front();
    free_list_.pop_front();
  }

  ENSURE(target_frame >= 0 && target_frame < CACHE_SIZE,
         "FetchBucket: target frame error");

  bucket_table_.erase(bucket_ids_[target_frame]);
  bucket_ids_[target_frame] = bucket;
  replacer_->RecordAccess(target_frame);

  auto [client, offset] = GetClientBucket(bucket);
  if (need_flush) {
    flush_future.wait();
  }

  return target_frame;
}

void Dpu::FlushAll() {
  std::array<std::future<bool>, CACHE_SIZE> futures;
  for (frame_id_t i = 0; i < CACHE_SIZE; i++) {
    futures[i] = FlushBucket(i);
  }
  for (auto &f : futures) {
    f.wait();
  }
}

void Dpu::DebugPrintCache() const {
  std::cout << "************************************\n";
  for (size_t i = 0; i < CACHE_SIZE; i++) {
    cache_[i].DebugPrint(bucket_ids_[i]);
  }
  std::cout << "************************************\n\n";
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
  // std::cout << "Process Search...\n";

  auto bs = Get4Buckets(request.hash1_, request.hash2_);
  // std::cout << "Hash to 4 buckets: ";
  // for (auto bucket : bs) {
  //   std::cout << bucket << " ";
  // }
  // std::cout << std::endl;

  // Check buckets already in cache
  std::vector<std::future<std::pair<bool, frame_id_t>>> futures;
  std::vector<bucket_id_t> to_fetch;

  for (auto b : bs) {
    if (auto it = bucket_table_.find(b); it != bucket_table_.end()) {
      // std::cout << "Bucket " << b << " found in cache\n";
      replacer_->RecordAccess(it->second);
      if (cache_[it->second].SearchSlot(request.key_, &result)) {
        return true;
      }
    } else {
      // std::cout << "Bucket " << b << " not in cache, fetching...\n";
      to_fetch.push_back(b);
    }
  }

  // Fetch buckets not in cache
  std::vector<frame_id_t> fetched_frames(to_fetch.size(), -1);
  for (auto b : to_fetch) {
    futures.push_back(FetchBucket(b));
  }

  // Process fetched buckets and search
  bool searched = false;
  for (size_t i = 0; i < to_fetch.size(); ++i) {
    auto [success, frame] = futures[i].get();
    if (success) {
      bucket_table_[to_fetch[i]] = frame;
      if (!searched && cache_[frame].SearchSlot(request.key_, &result)) {
        request.callback_(std::make_optional(result.ToString()));
        searched = true;
      }
    } else {
      std::cerr << "Failed to fetch bucket: " << to_fetch[i] << std::endl;
    }
  }

  return false;
}

bool Dpu::ProcessDelete(const Request &request) {
  auto bs = Get4Buckets(request.hash1_, request.hash2_);

  // Check buckets already in cache
  std::vector<std::future<std::pair<bool, frame_id_t>>> futures;
  std::vector<bucket_id_t> to_fetch;

  for (auto b : bs) {
    if (auto it = bucket_table_.find(b); it != bucket_table_.end()) {
      replacer_->RecordAccess(it->second);
      if (cache_[it->second].DeleteSlot(request.key_,
                                        dirty_flag_[it->second])) {
        return true;
      }
    } else {
      to_fetch.push_back(b);
    }
  }

  // Fetch buckets not in cache
  std::vector<frame_id_t> fetched_frames(to_fetch.size(), -1);
  for (auto b : to_fetch) {
    futures.push_back(FetchBucket(b));
  }

  // Process fetched buckets and search
  for (size_t i = 0; i < to_fetch.size(); ++i) {
    auto [success, frame] = futures[i].get();
    if (success) {
      bucket_table_[to_fetch[i]] = frame;
      if (cache_[frame].DeleteSlot(request.key_, dirty_flag_[frame])) {
        return true;
      }
    } else {
      std::cerr << "Failed to fetch bucket: " << to_fetch[i] << std::endl;
    }
  }
  return false;
}

bool Dpu::ProcessInsert(const Request &request) {
  // std::cout << "Process Insert...\n";
  // if (GetCurrentLoadFactor() >= 0.7)
  //   Expand();

  auto bs = Get4Buckets(request.hash1_, request.hash2_);
  // std::cout << "Hash to 4 buckets: ";
  // for (auto bucket : bs) {
  //   std::cout << bucket << " ";
  // }
  // std::cout << std::endl;

  auto tryInsertInto2Buckets = [&](size_t start_idx) -> bool {
    std::vector<std::pair<size_t, std::future<std::pair<bool, frame_id_t>>>>
        futures;
    std::array<frame_id_t, 2> frames;

    for (size_t i = 0; i < 2; ++i) {
      auto b = bs[start_idx + i];
      if (bucket_table_.count(b) == 0) {
        // Bucket not in cache, fetch asynchronously
        futures.push_back(std::make_pair(i, FetchBucket(b)));
      } else {
        // Bucket is in cache, record access
        frames[i] = bucket_table_[b];
        replacer_->RecordAccess(frames[i]);
      }
    }

    // Process futures for fetched buckets
    for (size_t i = 0; i < futures.size(); ++i) {
      auto [success, frame] = futures[i].second.get();
      if (success) {
        frames[futures[i].first] = frame;
        bucket_table_[bs[start_idx + futures[i].first]] = frame;
      } else {
        std::cerr << "Failed to fetch bucket: " << bs[start_idx + i]
                  << std::endl;
        return false;
      }
    }

    // Insert into buckets
    if (cache_[frames[0]].Count() > cache_[frames[1]].Count()) {
      if (cache_[frames[1]].InsertSlot(request.key_, request.value_,
                                       dirty_flag_[frames[1]])) {
        std::cout << "Inserted to bucket: " << bs[start_idx + 1]
                  << " frame: " << frames[1] << std::endl;
        return true;
      }
    } else {
      if (cache_[frames[0]].InsertSlot(request.key_, request.value_,
                                       dirty_flag_[frames[0]])) {
        std::cout << "Inserted to bucket: " << bs[start_idx]
                  << " frame: " << frames[0] << std::endl;
        return true;
      }
    }
    std::cout << "Cannot insert into bucket " << bs[start_idx] << ", "
              << bs[start_idx + 1] << std::endl;
    return false;
  };

  // Try inserting into the first two buckets
  if (tryInsertInto2Buckets(0)) {
    return true;
  }

  // Try inserting into the last two buckets
  return tryInsertInto2Buckets(2);
}

bool Dpu::ProcessUpdate(const Request &request) {
  auto bs = Get4Buckets(request.hash1_, request.hash2_);

  // Check buckets already in cache
  std::vector<std::future<std::pair<bool, frame_id_t>>> futures;
  std::vector<bucket_id_t> to_fetch;

  for (auto b : bs) {
    if (auto it = bucket_table_.find(b); it != bucket_table_.end()) {
      // std::cout << "Bucket " << b << " found in cache\n";
      replacer_->RecordAccess(it->second);
      if (cache_[it->second].UpdateSlot(request.key_, request.value_,
                                        dirty_flag_[it->second])) {
        return true;
      }
    } else {
      // std::cout << "Bucket " << b << " not in cache, fetching...\n";
      to_fetch.push_back(b);
    }
  }

  // Fetch buckets not in cache
  std::vector<frame_id_t> fetched_frames(to_fetch.size(), -1);
  for (auto b : to_fetch) {
    futures.push_back(FetchBucket(b));
  }

  // Process fetched buckets and search
  for (size_t i = 0; i < to_fetch.size(); ++i) {
    auto [success, frame] = futures[i].get();
    if (success) {
      bucket_table_[to_fetch[i]] = frame;
      if (cache_[frame].UpdateSlot(request.key_, request.value_,
                                   dirty_flag_[frame])) {
        return true;
      }
    } else {
      std::cerr << "Failed to fetch bucket: " << to_fetch[i] << std::endl;
    }
  }

  return false;
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
      if (!result)
        request.callback_(std::nullopt);
      // request.callback_(result ? std::make_optional(result_value.ToString())
      //                          : std::nullopt);
      break;
    }
    case RequestType::INSERT: {
      auto result = ProcessInsert(request);
      if (result)
        size_++;
      break;
    }
    case RequestType::DELETE: {
      ProcessDelete(request);
      break;
    }
    case RequestType::UPDATE: {

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

std::array<bucket_id_t, 4> Dpu::Get4Buckets(uint64_t hash1, uint64_t hash2) {
  size_t idx1 = hash1 % (addr_capacity_ / 2);
  size_t idx2 = addr_capacity_ / 2 + hash2 % (addr_capacity_ / 2);
  size_t idx3 = hash1 % (addr_capacity_ / 4);
  size_t idx4 = addr_capacity_ / 4 + hash2 % (addr_capacity_ / 4);
  return {addr_capacity_ - 4 + idx1, addr_capacity_ - 4 + idx2,
          bl_capacity_ - 4 + idx3, bl_capacity_ - 4 + idx4};
}

void Dpu::Expand() {
  std::cout << "Dpu begin to expand\n";
  in_rehash_ = true;
  mmap_to_recv_ = THREADS / 2;
  ComchMsg msg;
  msg.msg_type_ = ComchMsgType::COMCH_MSG_CONTROL;
  msg.ctl_msg_.control_signal_ = ControlSignal::EXPAND;
  dpu_comch_->Send((void *)&msg, sizeof(msg));
  for (size_t i = 0; i < THREADS / 2; i++) {
    dma_client_.emplace_back(std::make_unique<DmaClient>(
        GenNextClientId(), "03:00.0", (char *)cache_.data(), cache_size_));
  }
  while (mmap_to_recv_) {
    dpu_comch_->Progress();
  }
  std::cout << "Dpu expand received new mmap\n";
  std::vector<std::future<std::pair<bool, frame_id_t>>> uncached_buckets_fetch;
  for (size_t b = bl_start_bucket_id_; b < tl_start_bucket_id_; b++) {
    if (auto it = bucket_table_.find(b); it != bucket_table_.end()) {
      auto nbs = GetExpandBucketIds(b);
      std::array<frame_id_t, 4> nfs;
      for (size_t n = 0; n < 4; n++) {
        nfs[n] = NewBucket(nbs[n]);
      }
    } else {
      uncached_buckets_fetch.emplace_back(FetchBucket(b));
    }

    for (size_t slot = 0; slot < ASSOC_NUM; slot++) {
    }
  }
}

std::array<bucket_id_t, 4> Dpu::GetExpandBucketIds(bucket_id_t bucket) {
  ENSURE(bucket >= bl_start_bucket_id_ && bucket < tl_start_bucket_id_,
         "Only bl buckets need to be expanded");
  std::array<bucket_id_t, 4> ret;
  bucket_id_t start;
  if (bucket < bl_start_bucket_id_ + bl_capacity_ / 2) {
    start =
        tl_start_bucket_id_ + addr_capacity_ + (bucket - bl_start_bucket_id_);
  } else {
    start = tl_start_bucket_id_ + addr_capacity_ * 2 +
            (bucket - bl_capacity_ / 2 - bl_start_bucket_id_);
  }
  for (size_t i = 0; i < 4; i++) {
    ret[i] = start;
    start += bl_capacity_ / 2;
  }
  return ret;
}

float Dpu::GetCurrentLoadFactor() {
  return (float)ASSOC_NUM * (bl_capacity_ + addr_capacity_);
}
