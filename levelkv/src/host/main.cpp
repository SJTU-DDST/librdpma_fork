#include "comch_common.hpp"
#include "dma_common.hpp"
#include "host.hpp"

int main() {
  init_log_backend();
  Host host("b5:00.0", 3);
  // Comch host_comch(true, "Comch", "b5:00.0", "", comch_client_recv_callback,
  //                 comch_send_completion, comch_send_completion_err, nullptr,
  //                 nullptr, nullptr);
  host.Run();
  // auto cfg = comch_init("Server", "b5:00.0", "", nullptr);
  // while (true) {
    // host_comch.Progress();
    // doca_pe_progress(cfg->pe_);
  // }
  std::cout << "host exit\n" << std::endl;
  return 0;
}