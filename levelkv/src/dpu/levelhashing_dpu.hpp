#pragma once

#include <functional>
#include <optional>
#include <queue>
#include <string>
#include <memory>

#include "comch_common.hpp"
#include "type.hpp"
#include "dpu.hpp"

class LevelHashingDpu {
public:
  LevelHashingDpu(const std::string &pcie_addr, const std::string &pcie_rep_addr);
  ~LevelHashingDpu() = default;
  void Search(const FixedKey &key,
              std::function<void(std::optional<std::string>)> callback);
  void Insert(const FixedKey &key, const FixedValue &value);

  std::unique_ptr<Comch> dpu_comch_;
  std::queue<Request> request_queue_;
  bool waiting_;
  uint8_t searched_value_[VALUE_LEN];
  bool success_;
};