#include "comch_common.hpp"
#include "utils.hpp"

static void comch_dpu_recv_callback(doca_comch_event_msg_recv *event,
                                    uint8_t *recv_buffer, uint32_t msg_len,
                                    doca_comch_connection *comch_connection) {
  print_time();
  std::cout << "Dpu recv callback called... \n";
  std::cout.write((char *)recv_buffer, msg_len);
  std::cout << std::endl;
}

int main() {
  Comch dpu_comch(true, "Comch", "03:00.0", "b5:00.0", comch_dpu_recv_callback,
                  comch_send_completion, comch_send_completion_err,
                  server_connection_cb, server_disconnection_cb, nullptr);
  char msg[4000];
  memset(msg, 'A', 4000);
  print_time();
  std::cout << "Start to send.\n";
  dpu_comch.Send(msg, 4000);
  return 0;
}