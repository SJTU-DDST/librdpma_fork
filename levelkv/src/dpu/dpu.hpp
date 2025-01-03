#pragma once

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

#include <doca_buf_inventory.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_dma.h>
#include <doca_mmap.h>
#include <doca_pe.h>

#include "dma_client.hpp"
#include "level_hashing.hpp"
#include "replacer.hpp"
#include "utils.hpp"

#define CACHE_SIZE 4

enum class RequestType {
  SEARCH,
  DELETE,
  INSERT,
};

struct Request {
  RequestType request_type_;
  FixedKey key_;
  FixedValue value_;
  uint64_t hash1_;
  uint64_t hash2_;
  std::function<void(std::optional<std::string>)> callback_;
};

class Dpu {
public:
  Dpu(const std::string &pcie_addr, uint64_t level = DEFAULT_START_LEVEL);

  ~Dpu();

  void Search(const FixedKey &key, std::function<void(std::optional<std::string>)> callback);
  void Delete(const FixedKey &key);
  void Insert(const FixedKey &key, const FixedValue &value);
  
  std::future<bool> FlushBucket(frame_id_t frame);
  std::future<std::pair<bool, frame_id_t>> FetchBucket(bucket_id_t bucket);
  void FlushAll();
  void DebugPrintCache() const;

private:
  bool ProcessSearch(const Request &request, FixedValue &result);
  bool ProcessDelete(const Request &request);
  bool ProcessInsert(const Request &request);
  uint64_t GenNextClientId();
  std::pair<size_t, size_t> GetClientBucket(bucket_id_t bucket) const;
  void Run();
  void GenSeeds();
  std::array<bucket_id_t, 4> Get4Buckets(uint64_t hash1, uint64_t hash2);

public:
  std::vector<std::unique_ptr<DmaClient>> dma_client_;
  std::thread worker_;
  std::atomic<bool> stop_;
  std::queue<Request> request_queue_;
  std::mutex request_queue_mtx_;
  std::condition_variable cv_;

  std::array<FixedBucket, CACHE_SIZE> cache_;
  std::array<bool, CACHE_SIZE> dirty_flag_;
  size_t cache_size_;
  std::unordered_map<bucket_id_t, frame_id_t> bucket_table_;
  bucket_id_t bl_start_bucket_id_;
  bucket_id_t tl_start_bucket_id_;
  uint64_t level_;
  uint64_t next_client_id_;

  uint64_t f_seed_;
  uint64_t s_seed_;
  size_t addr_capacity_;
  size_t bl_capacity_;

  std::unique_ptr<Replacer> replacer_;
  std::list<frame_id_t> free_list_;
  std::list<frame_id_t> dirty_list_;
  std::array<bucket_id_t, CACHE_SIZE> bucket_ids_;
};