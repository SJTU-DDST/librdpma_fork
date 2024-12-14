#include <cassert>
#include <cmath>
#include <iostream>

#include "level_hashing.hpp"
#include "utils.hpp"

LevelHashTable::LevelHashTable(uint64_t level) : level_(level) {
  assert(level >= 3);
  addr_capacity_ = 1 << level;
  bl_capacity_ = 1 << (level - 1);
  total_capacity_ = addr_capacity_ + bl_capacity_;
  buckets_[0] = (LevelBucket *)alignedmalloc(addr_capacity_ * sizeof(LevelBucket));
  buckets_[1] = (LevelBucket *)alignedmalloc(bl_capacity_ * sizeof(LevelBucket));
  ENSURE(buckets_[0] && buckets_[1], "Level hash table initialization failed");
  num_level_items_[0] = 0;
  num_level_items_[1] = 0;
}

LevelHashTable::~LevelHashTable() {
  free(buckets_[0]);
  free(buckets_[1]);
}

void LevelHashTable::GenSeeds() {
  srand(time(NULL));
  do {
    f_seed = rand();
    s_seed = rand();
    f_seed = f_seed << (rand() % 63);
    s_seed = s_seed << (rand() % 63);
  } while (f_seed == s_seed);
}