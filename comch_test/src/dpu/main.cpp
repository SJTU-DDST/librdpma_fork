#include "comch_common.hpp"
#include "utils.hpp"
#include <chrono>
#include <thread>
#include <vector>

std::chrono::microseconds start_time;
std::chrono::microseconds end_time;
int rounds = TOTAL_ROUNDS;
bool finished = false;

static void comch_dpu_recv_callback(doca_comch_event_msg_recv *event,
                                    uint8_t *recv_buffer, uint32_t msg_len,
                                    doca_comch_connection *comch_connection) {
  if (rounds == TOTAL_ROUNDS - WARMUP_ROUNDS) {
    start_time = get_time_us();
  }
  auto ctx_user_data = doca_comch_connection_get_user_data(comch_connection);
  Comch *comch = reinterpret_cast<Comch *>(ctx_user_data.ptr);
  if (rounds > 0) {
    char msg[PAYLOAD];
    comch->Send(msg, PAYLOAD);
    rounds--;
  } else {
    end_time = get_time_us();
    std::cout << (end_time - start_time).count() /
                     double(TOTAL_ROUNDS - WARMUP_ROUNDS)
              << std::endl;
    finished = true;
    char exit = 'E';
    comch->Send(&exit, 1);
  }
}

int main() {
  Comch dpu_comch(true, "Comch", "03:00.0", "b5:00.0", comch_dpu_recv_callback,
                  comch_send_completion, comch_send_completion_err,
                  server_connection_cb, server_disconnection_cb, nullptr);
  char msg[PAYLOAD];
  dpu_comch.Send(msg, PAYLOAD);
  while (!finished) {
    dpu_comch.Progress();
  }
  return 0;
}