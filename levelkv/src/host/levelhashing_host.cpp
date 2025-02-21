#include "levelhashing_host.hpp"
#include "type.hpp"

static void comch_host_recv_callback(doca_comch_event_msg_recv *event,
                                     uint8_t *recv_buffer, uint32_t msg_len,
                                     doca_comch_connection *comch_connection) {
  auto ctx_user_data = doca_comch_connection_get_user_data(comch_connection);
  Comch *comch = reinterpret_cast<Comch *>(ctx_user_data.ptr);
  // ENSURE(comch->is_server_ == false, "Comch->is_server_ is not 0 on host");
  auto comch_user_data = comch->user_data_;
  LevelHashingHost *host =
      reinterpret_cast<LevelHashingHost *>(comch_user_data);
  ComchMsg *msg = reinterpret_cast<ComchMsg *>(recv_buffer);
  switch (msg->msg_type_) {
  case ComchMsgType::COMCH_MSG_OPERATION: {
    ComchMsg return_msg;
    return_msg.msg_type_ = ComchMsgType::COMCH_MSG_OPERATION_RESULT;
    if (msg->op_msg_.op_type_ == OprationType::INSERT) {
      uint8_t res =
          level_insert(host->level_hashtable_, (uint8_t *)&msg->op_msg_.key_,
                       (uint8_t *)&msg->op_msg_.value_);
      return_msg.op_result_msg_.success = res;
    } else if (msg->op_msg_.op_type_ == OprationType::SEARCH) {
      uint8_t *res = level_static_query(host->level_hashtable_,
                                        (uint8_t *)&msg->op_msg_.key_);
      if (res) {
        memcpy(return_msg.op_result_msg_.search_result_, res, VALUE_LEN);
        return_msg.op_result_msg_.success = true;
      } else {
        return_msg.op_result_msg_.success = false;
      }
    }
    comch->Send((void *)&return_msg, sizeof(return_msg));
    break;
  }
  case ComchMsgType::COMCH_MSG_CONTROL: {
    if (msg->ctl_msg_.control_signal_ == ControlSignal::EXIT) {
      host->stop_ = true;
    }
    break;
  }
    // case ComchMsgType::COMCH_MSG_CONTROL: {
    //   if (msg->ctl_msg_.control_signal_ == ControlSignal::EXPAND) {
    //     host->Expand();
    //   } else if (msg->ctl_msg_.control_signal_ ==
    //   ControlSignal::EXPAND_FINISH)
    //   {
    //     host->in_rehash_ = false;
    //     ComchMsg back;
    //     back.msg_type_ = ComchMsgType::COMCH_MSG_CONTROL;
    //     back.ctl_msg_.control_signal_ = ControlSignal::EXPAND_FINISH;
    //     comch->Send((void *)&back, sizeof(back));
    //   } else if (msg->ctl_msg_.control_signal_ == ControlSignal::EXIT) {
    //     host->stop_ = true;
    //   }
    //   break;
  }

  // default:
  //   break;
  // }
}

LevelHashingHost::LevelHashingHost(const std::string &pcie_addr, uint64_t level)
    : host_comch_(std::make_unique<Comch>(
          false, "Comch", pcie_addr, "", comch_host_recv_callback,
          comch_send_completion, comch_send_completion_err, nullptr, nullptr,
          this)),
      stop_(false) {
  level_hashtable_ = level_init(LEVEL_START);
}

LevelHashingHost::~LevelHashingHost() { level_destroy(level_hashtable_); }

void LevelHashingHost::Run() {
  while (!stop_) {
    host_comch_->Progress();
  }
}

void LevelHashingHost::Stop() { stop_ = true; }
