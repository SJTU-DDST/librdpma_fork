#include "comch_common.hpp"
#include "utils.hpp"

bool finished = false;

static void comch_host_recv_callback(doca_comch_event_msg_recv *event,
                                    uint8_t *recv_buffer, uint32_t msg_len,
                                    doca_comch_connection *comch_connection) {
  auto ctx_user_data = doca_comch_connection_get_user_data(comch_connection);
  Comch *comch = reinterpret_cast<Comch *>(ctx_user_data.ptr);
  if (msg_len == 1 && *recv_buffer = 'E') {
    std::cout << "exit\n";
    finished = true;
  }
  char msg[msg_len];
  comch->Send(msg, msg_len);
}

int main() {
  Comch host_comch(false, "Comch", "b5:00.0", "", comch_host_recv_callback,
                  comch_send_completion, comch_send_completion_err,
                  nullptr, nullptr, nullptr);
  while (!finished) {
    host_comch.Progress();
  }
  return 0;
}