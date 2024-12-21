#include <iostream>
#include <iomanip>

#include "dpu.hpp"

int main() {
  Dpu dpu("03:00.0");
  std::cout << "111\n";
  auto f = dpu.FetchBucket(0);
  std::cout << f.get() << std::endl;
  dpu.cache_[0].DebugPrint();
  while (true) {
    char c = getchar();
    if (c == 'q' || c == 'Q')
      break;
  }
  return 0;
}