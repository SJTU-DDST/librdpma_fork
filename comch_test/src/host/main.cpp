#include "comch_common.hpp"
#include "utils.hpp"

static void comch_host_recv_callback(doca_comch_event_msg_recv *event,
                                    uint8_t *recv_buffer, uint32_t msg_len,
                                    doca_comch_connection *comch_connection) {
  print_time();
  std::cout << "Host recv callback called... \n";
  std::cout.write((char *)recv_buffer, msg_len);
  std::cout << std::endl;
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