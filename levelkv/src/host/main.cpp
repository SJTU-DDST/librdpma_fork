#include "host.hpp"

int main() {
  Host host("b5:00.0", 3);
  host.DebugPrint();
  host.Run();
  
  return 0;
}