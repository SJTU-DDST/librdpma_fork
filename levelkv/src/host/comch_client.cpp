#include "comch_client.hpp"

static void message_recv_callback(doca_comch_event_msg_recv *event,
                           uint8_t *recv_buffer, uint32_t msg_len,
                           doca_comch_connection *comch_connection) {
  auto ctx = doca_comch_server_get_server_ctx(comch_connection);
  doca_data user_data;
  doca_ctx_get_user_data(doca_comch_server_as_ctx(ctx), &user_data);
  auto server = static_cast<ComchClient *>(user_data.ptr);
  ENSURE(server->recv_callback_, "Receive callback is empty!");
  server->client_recv_callback_((ComchMsg *)recv_buffer, *server->host_);
}

ComchClient::ComchClient(const std::string &pcie_addr, Host *host)
    : Comch(pcie_addr), host_(host) {
  doca_comch_client_create(dev_, "Comch Client", &comch_client_);
  ctx_ = doca_comch_client_as_ctx(comch_client_);
  doca_pe_connect_ctx(pe_, ctx_);
  doca_comch_client_event_msg_recv_register(comch_client_,
                                            message_recv_callback);
}
