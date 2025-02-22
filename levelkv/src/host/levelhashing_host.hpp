#pragma once

#include <memory>

#include "comch_common.hpp"
#include "level_hashing.h"

#define LEVEL_START 5

class LevelHashingHost {
public:
  LevelHashingHost(const std::string &pcie_addr);

  ~LevelHashingHost();

  void Run();
  void Stop();

  level_hash *level_hashtable_;
  std::unique_ptr<Comch> host_comch_;
  bool in_rehash_;
  bool stop_;
};