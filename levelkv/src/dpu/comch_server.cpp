#include <iostream>

#include "comch_server.hpp"
#include "dma_common.hpp"

static void message_recv_callback(doca_comch_event_msg_recv *event,
                                  uint8_t *recv_buffer, uint32_t msg_len,
                                  doca_comch_connection *comch_connection) {
  auto ctx = doca_comch_server_get_server_ctx(comch_connection);
  doca_data user_data;
  doca_ctx_get_user_data(doca_comch_server_as_ctx(ctx), &user_data);
  auto server = static_cast<ComchServer *>(user_data.ptr);
  ENSURE(server->recv_callback_, "Receive callback is empty!");
  server->recv_callback_(recv_buffer, msg_len);
}

ComchServer::ComchServer(const std::string &pcie_addr,
                         const std::string &pcie_rep_addr, Dpu *dpu)
    : Comch(pcie_addr), dpu_(dpu) {
  dev_rep_ = open_device_rep(pcie_rep_addr, dev_);
  doca_comch_server_create(dev_, dev_rep_, "Comch Server", &comch_server_);
  ctx_ = doca_comch_server_as_ctx(comch_server_);
  doca_pe_connect_ctx(pe_, ctx_);
  doca_comch_server_event_msg_recv_register(comch_server_,
                                            message_recv_callback);
}

void ComchServer::SendExpand() {
  ComchMsg msg;
  msg.msg_type_ = ComchMsgType::COMCH_MSG_CONTROL;
  msg.ctl_msg_ = ComchMsgControl{ControlSignal::EXPAND};
  doca_comch_task_send *task;
  doca_comch_server_task_send_alloc_init(comch_server_, connection_, (void *)&msg, sizeof(msg), &task);
  doca_task_submit(doca_comch_task_send_as_task(task));
}
