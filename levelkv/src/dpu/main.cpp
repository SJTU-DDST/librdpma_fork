#include <iomanip>
#include <iostream>

#include "comch_common.hpp"
#include "dma_common.hpp"
#include "dpu.hpp"

void exampleCallBack(std::optional<std::string> s) {
  // if (s) {
  //   std::cout << "Found: " << s.value() << std::endl;
  // } else {
  //   std::cout << "KV not found\n";
  // }
}

int main() {
  // init_log_backend();
  Dpu dpu("03:00.0", "b5:00.0");

  // Comch dpu_comch(true, "Comch", "03:00.0", "b5:00.0",
  //                 comch_server_recv_callback, comch_send_completion,
  //                 comch_send_completion_err, server_connection_cb,
  //                 server_disconnection_cb, nullptr);
  // char msg[1024];
  // memset(msg, 'K', 1023);
  // dpu_comch.Send(msg, 1023);
  dpu.Start();
  for (size_t i = 1; i <= 10000; i++) {
    dpu.Insert(i, i);
  }
  for (size_t i = 1; i <= 10000; i++) {
    dpu.Search(i, exampleCallBack);
  }
  dpu.End();
  // dpu.Search(1, exampleCallBack);
  // std::this_thread::sleep_for(std::chrono::seconds(1));
  // dpu.Expand();
  // std::this_thread::sleep_for(std::chrono::seconds(1));
  // for (size_t i = 9; i > 0; i--) {
  //   dpu.Search(i, &exampleCallBack);
  // }
  // dpu.FlushAll();
  while (!dpu.stop_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  std::cout << "dpu exit\n" << std::endl;
  return 0;
}