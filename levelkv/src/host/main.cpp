#include "comch_common.hpp"
#include "dma_common.hpp"
#include "host.hpp"
#include "levelhashing_host.hpp"

int main() {
  // init_log_backend();
  // Host host("b5:00.0", DEFAULT_START_LEVEL);
  LevelHashingHost host("b5:00.0");
  host.Run();
  std::cout << "host exit\n" << std::endl;
  return 0;
}