#include "host.hpp"
#include "comch_common.hpp"
#include "dma_common.hpp"

int main() {
  // Host host("b5:00.0", 3);
  // host.DebugPrint();
  // host.Run();
  init_log_backend();
  auto cfg = comch_init("Client", "b5:00.0", "", nullptr);
  
  return 0;
}