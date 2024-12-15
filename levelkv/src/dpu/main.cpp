#include <iostream>
#include <iomanip>

#include "dpu.hpp"

void printHex(const LevelBucket& bucket) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&bucket);
    size_t size = sizeof(LevelBucket); // Total size of the struct
    
    for (size_t i = 0; i < size; ++i) {
        std::cout << std::setw(2) << std::setfill('0') // Format as two digits
                  << std::hex << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::endl;
}

int main() {
  Dpu dpu("03:00.0");
  std::cout << "111\n";
  auto f = dpu.FetchBucket(0);
  std::cout << f.get() << std::endl;
  printHex(dpu.cache_[0]);
  while (true) {
    char c = getchar();
    if (c == 'q' || c == 'Q')
      break;
  }
  return 0;
}