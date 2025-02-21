#include <cstring>
#include <thread>

#include "dma_common.hpp"
#include "host.hpp"

static void comch_host_recv_callback(doca_comch_event_msg_recv *event,
                                     uint8_t *recv_buffer, uint32_t msg_len,
                                     doca_comch_connection *comch_connection) {
  auto ctx_user_data = doca_comch_connection_get_user_data(comch_connection);
  Comch *comch = reinterpret_cast<Comch *>(ctx_user_data.ptr);
  ENSURE(comch->is_server_ == false, "Comch->is_server_ is not 0 on host");
  auto comch_user_data = comch->user_data_;
  Host *host = reinterpret_cast<Host *>(comch_user_data);
  ComchMsg *msg = reinterpret_cast<ComchMsg *>(recv_buffer);
  switch (msg->msg_type_) {
  case ComchMsgType::COMCH_MSG_CONTROL: {
    if (msg->ctl_msg_.control_signal_ == ControlSignal::EXPAND) {
      host->Expand();
    } else if (msg->ctl_msg_.control_signal_ == ControlSignal::EXPAND_FINISH) {
      host->in_rehash_ = false;
      ComchMsg back;
      back.msg_type_ = ComchMsgType::COMCH_MSG_CONTROL;
      back.ctl_msg_.control_signal_ = ControlSignal::EXPAND_FINISH;
      comch->Send((void *)&back, sizeof(back));
    } else if (msg->ctl_msg_.control_signal_ == ControlSignal::EXIT) {
      host->stop_ = true;
    }
    break;
  }

  default:
    break;
  }
}

Host::Host(const std::string &pcie_addr, uint64_t level)
    : level_ht_(std::make_unique<FixedHashTable>(level)), next_server_id_(0),
      host_comch_(std::make_unique<Comch>(
          false, "Comch", pcie_addr, "", comch_host_recv_callback,
          comch_send_completion, comch_send_completion_err, nullptr, nullptr,
          this)),
      in_rehash_(false), stop_(false) {
  // Send seed
  ComchMsg seed_msg;
  seed_msg.msg_type_ = ComchMsgType::COMCH_MSG_EXPORT_SEEDS;
  seed_msg.seed_msg_.f_seed_ = level_ht_->f_seed_;
  seed_msg.seed_msg_.s_seed_ = level_ht_->s_seed_;
  host_comch_->Send((void *)&seed_msg, sizeof(seed_msg));

  memset(level_ht_->buckets_[0], 0,
         sizeof(FixedBucket) * level_ht_->addr_capacity_);
  memset(level_ht_->buckets_[1], 0,
         sizeof(FixedBucket) * level_ht_->bl_capacity_);
  bl_dma_server_.resize(THREADS / 2);
  tl_dma_server_.resize(THREADS / 2);

  char *ptr = (char *)level_ht_->buckets_[1];
  size_t mmap_size =
      level_ht_->bl_capacity_ / (THREADS / 2) * sizeof(FixedBucket);
  for (size_t i = 0; i < THREADS / 2; i++) {
    auto id = GetNextServerId();
    bl_dma_server_[i] =
        std::make_unique<DmaServer>(id, pcie_addr, ptr, mmap_size);
    bl_dma_server_[i]->ExportMmapToFile();
    ptr += mmap_size;
  }
  ptr = (char *)level_ht_->buckets_[0];
  mmap_size = level_ht_->addr_capacity_ / (THREADS / 2) * sizeof(FixedBucket);
  for (size_t i = 0; i < THREADS / 2; i++) {
    auto id = GetNextServerId();
    tl_dma_server_[i] =
        std::make_unique<DmaServer>(id, pcie_addr, ptr, mmap_size);
    ptr += mmap_size;
  }
  ExportMmapsComch(bl_dma_server_);
  ExportMmapsComch(tl_dma_server_);
}

Host::~Host() {}

void Host::Expand() {
  in_rehash_ = true;
  size_t new_size = level_ht_->addr_capacity_ * 2 * sizeof(FixedBucket);
  size_t new_size_per_server = new_size / (THREADS / 2);
  char *new_mem = (char *)alignedmalloc(new_size);
  memset(new_mem, 0, new_size);
  char *ptr = new_mem;
  std::vector<std::unique_ptr<DmaServer>> new_dma_servers(THREADS / 2);
  for (size_t i = 0; i < THREADS / 2; i++) {
    new_dma_servers[i] = std::make_unique<DmaServer>(
        GetNextServerId(), "b5:00.0", ptr, new_size_per_server);
    ptr += new_size_per_server;
  }
  for (const auto &ns : new_dma_servers) {
    auto new_mmap_msg = ns->ExportMmap();
    host_comch_->Send((void *)&new_mmap_msg, sizeof(new_mmap_msg));
  }
  while (in_rehash_) {
    host_comch_->Progress();
  }
  std::cout << "Host exit rehashing stage\n";
  // Exchange dma servers
  bl_dma_server_ = std::move(tl_dma_server_);
  tl_dma_server_ = std::move(new_dma_servers);
  level_ht_->ExpandParameters();
  free(level_ht_->buckets_[1]);
  level_ht_->buckets_[1] = level_ht_->buckets_[0];
  level_ht_->buckets_[0] = (FixedBucket *)new_mem;
}

void Host::DebugPrint() const {
  std::cout << "************************************************\nTL: \n";
  for (size_t i = 0; i < level_ht_->addr_capacity_; i++) {
    level_ht_->buckets_[0][i].DebugPrint();
  }
  std::cout << "\nBL: \n";
  for (size_t i = 0; i < level_ht_->bl_capacity_; i++) {
    level_ht_->buckets_[1][i].DebugPrint();
  }
  std::cout << "************************************************\n\n";
}

void Host::Run() {
  while (!stop_) {
    host_comch_->Progress();
    // std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

uint64_t Host::GetNextServerId() {
  auto ret = next_server_id_;
  next_server_id_++;
  return ret;
}

void Host::ExportMmapsComch(
    const std::vector<std::unique_ptr<DmaServer>> &dma_servers) {
  for (const auto &s : dma_servers) {
    auto msg = s->ExportMmap();
    host_comch_->Send(&msg, sizeof(msg));
  }
}
