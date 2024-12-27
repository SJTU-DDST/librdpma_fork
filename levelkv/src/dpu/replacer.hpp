#pragma once

#include <list>
#include <unordered_map>

#include "utils.hpp"

class Replacer {
public:
  void RecordAccess(frame_id_t frame);
  void Evict(frame_id_t *frame);
private:
  std::list<frame_id_t> lru_;
  std::unordered_map<frame_id_t, typename std::list<frame_id_t>::iterator> map_;
};