#include "comch_common.hpp"
#include "utils.hpp"

static void comch_host_recv_callback(doca_comch_event_msg_recv *event,
                                    uint8_t *recv_buffer, uint32_t msg_len,
                                    doca_comch_connection *comch_connection) {
  auto ctx_user_data = doca_comch_connection_get_user_data(comch_connection);
  Comch *comch = reinterpret_cast<Comch *>(ctx_user_data.ptr);
  char msg[payload];
  comch->Send(msg, payload);
}

int main() {
  Comch host_comch(true, "Comch", "03:00.0", "b5:00.0", comch_host_recv_callback,
                  comch_send_completion, comch_send_completion_err,
                  nullptr, nullptr, nullptr);
  while (true) {
    host_comch.Progress();
  }
  return 0;
}