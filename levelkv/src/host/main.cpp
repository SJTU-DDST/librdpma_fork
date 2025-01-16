#include "comch_common.hpp"
#include "dma_common.hpp"
#include "host.hpp"

int main() {
  init_log_backend();
  Host host("b5:00.0", 3);
  // host.DebugPrint();
  // host.Run();
  // auto cfg = comch_init("Server", "b5:00.0", "", nullptr);
  while (true) {
    // doca_pe_progress(cfg->pe_);
  }
  return 0;
}