#include "host.hpp"
#include "comch_common.hpp"

int main() {
  // Host host("b5:00.0", 3);
  // host.DebugPrint();
  // host.Run();
  auto cfg = comch_init("Client", "b5:00.0", "", nullptr);
  
  return 0;
}