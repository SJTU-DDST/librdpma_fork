#include "levelhashing_dpu.hpp"

int i = 0;

static void comch_dpu_recv_callback(doca_comch_event_msg_recv *event,
                                    uint8_t *recv_buffer, uint32_t msg_len,
                                    doca_comch_connection *comch_connection) {
  auto ctx_user_data = doca_comch_connection_get_user_data(comch_connection);
  Comch *comch = reinterpret_cast<Comch *>(ctx_user_data.ptr);
  ENSURE(comch->is_server_, "Comch->is_server_ is not 1 on dpu");
  auto comch_user_data = comch->user_data_;
  LevelHashingDpu *dpu = reinterpret_cast<LevelHashingDpu *>(comch_user_data);
  ComchMsg *msg = reinterpret_cast<ComchMsg *>(recv_buffer);
  switch (msg->msg_type_) {
  case ComchMsgType::COMCH_MSG_OPERATION_RESULT: {
    if (msg->op_result_msg_.success) {
      memcpy(dpu->searched_value_, msg->op_result_msg_.search_result_,
             VALUE_LEN);
      dpu->success_ = true;
    } else {
      dpu->success_ = false;
    }
    dpu->waiting_ = false;
    break;
  }

  default:
    ENSURE(0, "Dpu received unsupported comch msg type");
  }
}

LevelHashingDpu::LevelHashingDpu(const std::string &pcie_addr,
                                 const std::string &pcie_rep_addr)
    : dpu_comch_(std::make_unique<Comch>(
          true, "Comch", pcie_addr, pcie_rep_addr, comch_dpu_recv_callback,
          comch_send_completion, comch_send_completion_err,
          server_connection_cb, server_disconnection_cb, this)),
      waiting_(false), success_(false) {
  memset(searched_value_, 0, VALUE_LEN);
}

void LevelHashingDpu::Search(
    const FixedKey &key,
    std::function<void(std::optional<std::string>)> callback) {
  ComchMsg request_msg;
  request_msg.msg_type_ = ComchMsgType::COMCH_MSG_OPERATION;
  request_msg.op_msg_.op_type_ = OprationType::SEARCH;
  memcpy(request_msg.op_msg_.key_, (void *)&key, KEY_LEN);
  waiting_ = true;
  dpu_comch_->Send((void *)&request_msg, sizeof(request_msg));
  while (waiting_) {
    dpu_comch_->Progress();
  }
  if (success_) {
    std::string res(reinterpret_cast<const char *>(searched_value_), VALUE_LEN);
    callback(std::make_optional(res));
  } else {
    callback(std::nullopt);
  }
  memset(searched_value_, 0, VALUE_LEN);
  success_ = false;
}

void LevelHashingDpu::Insert(const FixedKey &key, const FixedValue &value) {
  ComchMsg request_msg;
  request_msg.msg_type_ = ComchMsgType::COMCH_MSG_OPERATION;
  request_msg.op_msg_.op_type_ = OprationType::INSERT;
  memcpy(request_msg.op_msg_.key_, (void *)&key, KEY_LEN);
  memcpy(request_msg.op_msg_.value_, (void *)&value, KEY_LEN);
  waiting_ = true;
  dpu_comch_->Send((void *)&request_msg, sizeof(request_msg));
  while (waiting_) {
    dpu_comch_->Progress();
  }
  std::cout << "insertion " << ++i << " " << success_ << std::endl;
  success_ = false;
}
