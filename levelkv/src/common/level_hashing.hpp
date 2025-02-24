#pragma once

#include <array>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>

#include "type.hpp"
#include "utils.hpp"

#define ASSOC_NUM (4) // The number of slots in a bucket
#define DEFAULT_START_LEVEL (7)

template <typename K, typename V> struct Entry {
  K key_;
  V value_;
} __attribute__((packed));

template <typename K, typename V, size_t AssocNum> class LevelBucket {
public:
  std::array<uint8_t, AssocNum> token_;
  std::array<Entry<K, V>, AssocNum> slots_;

  LevelBucket() { token_.fill(0); }

  void SetSlot(size_t slot_number, K key, V value) {
    assert(slot_number < AssocNum);
    slots_[slot_number].key_ = key;
    slots_[slot_number].value_ = value;
    token_[slot_number] = 1;
  }

  bool SearchSlot(const K &key, V *value) const {
    for (size_t i = 0; i < AssocNum; i++) {
      if (token_[i] == 1 && slots_[i].key_ == key) {
        *value = slots_[i].value_;
        return true;
      }
    }
    return false;
  }

  bool DeleteSlot(const K &key, bool &dirty_flag) {
    for (size_t i = 0; i < AssocNum; i++) {
      if (token_[i] == 1 && slots_[i].key_ == key) {
        token_[i] = 0;
        dirty_flag = true;
        return true;
      }
    }
    return false;
  }

  bool InsertSlot(const K &key, const V &value, bool &dirty_flag) {
    for (size_t i = 0; i < AssocNum; i++) {
      if (token_[i] == 0) {
        slots_[i].key_ = key;
        slots_[i].value_ = value;
        token_[i] = 1;
        dirty_flag = true;
        return true;
      }
    }
    return false;
  }

  bool UpdateSlot(const K &key, const V &value, bool &dirty_flag) {
    for (size_t i = 0; i < AssocNum; i++) {
      if (token_[i] == 1 && slots_[i].key_ == key) {
        if (slots_[i].value_ != value) {
          slots_[i].value_ = value;
          dirty_flag = true;
        }
        return true;
      }
    }
    return false;
  }

  size_t Count() const {
    size_t c = 0;
    for (size_t i = 0; i < AssocNum; i++) {
      if (token_[i] == 1)
        c++;
    }
    return c;
  }

  void DebugPrint(bucket_id_t bucket = -1) const {
    std::cout << "------ Bucket " << bucket << " ------\n";
    for (size_t i = 0; i < AssocNum; i++) {
      std::cout << "Slot " << i << ": " << (token_[i] == 0 ? 0 : 1) << " "
                << slots_[i].key_ << " " << slots_[i].value_ << "   ";
    }
    std::cout << std::endl;
  }
};

template <typename K, typename V, size_t AssocNum = ASSOC_NUM>
class LevelHashTable {
public:
  using BucketType = LevelBucket<K, V, AssocNum>;

  LevelHashTable(uint64_t level) : level_(level) {
    assert(level >= 3);
    addr_capacity_ = 1 << level;
    bl_capacity_ = 1 << (level - 1);
    total_capacity_ = addr_capacity_ + bl_capacity_;
    buckets_[0] =
        (BucketType *)alignedmalloc(addr_capacity_ * sizeof(BucketType));
    buckets_[1] =
        (BucketType *)alignedmalloc(bl_capacity_ * sizeof(BucketType));
    ENSURE(buckets_[0] && buckets_[1],
           "Level hash table initialization failed");
    num_level_items_[0] = 0;
    num_level_items_[1] = 0;
    GenSeeds();
  }

  ~LevelHashTable() {
    free(buckets_[0]);
    free(buckets_[1]);
  }

  void GenSeeds() {
    srand(time(NULL));
    do {
      f_seed_ = rand();
      s_seed_ = rand();
      f_seed_ = f_seed_ << (rand() % 63);
      s_seed_ = s_seed_ << (rand() % 63);
    } while (f_seed_ == s_seed_);
  }

  void ExpandParameters() {
    addr_capacity_ *= 2;
    bl_capacity_ *= 2;
    total_capacity_ = addr_capacity_ + bl_capacity_;
    level_++;
  }

  std::array<BucketType *, 2> buckets_;
  std::array<uint64_t, 2> num_level_items_;
  uint64_t addr_capacity_;
  uint64_t bl_capacity_;
  uint64_t total_capacity_;
  uint64_t level_;

  uint64_t f_seed_;
  uint64_t s_seed_;
};

using FixedBucket = LevelBucket<FixedKey, FixedValue, ASSOC_NUM>;
using FixedHashTable = LevelHashTable<FixedKey, FixedValue, ASSOC_NUM>;
