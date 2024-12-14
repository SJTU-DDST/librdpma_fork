#pragma once

#include <array>
#include <cstddef>
#include <memory>

#define ASSOC_NUM (4)  // The number of slots in a bucket
#define KEY_LEN (16)   // The maximum length of a key
#define VALUE_LEN (15) // The maximum length of a value
#define DEFAULT_START_LEVEL (3)

struct Entry { // A slot storing a key-value item
  uint8_t key[KEY_LEN];
  uint8_t value[VALUE_LEN];
} __attribute__((packed));

struct LevelBucket {
  uint8_t token[ASSOC_NUM];
  Entry slot[ASSOC_NUM];
} __attribute__((packed));

struct LevelHashTable {
  std::array<LevelBucket *, 2> buckets_;
  std::array<uint64_t, 2> num_level_items_;
  uint64_t addr_capacity_;
  uint64_t bl_capacity_;
  uint64_t total_capacity_;
  uint64_t level_;

  uint64_t f_seed;
  uint64_t s_seed;

  explicit LevelHashTable(uint64_t level = DEFAULT_START_LEVEL);
  ~LevelHashTable();
  void GenSeeds();
};
