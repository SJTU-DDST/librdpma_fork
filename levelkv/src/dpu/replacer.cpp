#include "replacer.hpp"

void Replacer::RecordAccess(frame_id_t frame) {
  if (auto it = map_.find(frame); it != map_.end()) {
    lru_.erase(it->second);
  }
  lru_.push_front(frame);
  map_[frame] = lru_.begin();
}

void Replacer::Evict(frame_id_t* frame) {
  if (lru_.empty()) {
    *frame = -1;
    return;
  }
  auto evict = lru_.back();
  lru_.pop_back();
  map_.erase(evict);
  *frame = evict;
}
