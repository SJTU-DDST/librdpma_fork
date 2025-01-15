#include "comch_common.hpp"
#include "dma_common.hpp"
#include "host.hpp"

int main() {
  // Host host("b5:00.0", 3);
  // host.DebugPrint();
  // host.Run();
  init_log_backend();
  auto cfg = comch_init("Server", "b5:00.0", "", nullptr);
  while (true) {
  }
  return 0;
}