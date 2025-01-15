#include "host.hpp"
#include "comch_common.hpp"
#include "dma_common.hpp"

int main() {
  // Host host("b5:00.0", 3);
  // host.DebugPrint();
  // host.Run();
  init_log_backend();
  int i = 1;
  auto cfg = comch_init("Client", "b5:00.0", "03:00.0", &i);
  
  return 0;
}