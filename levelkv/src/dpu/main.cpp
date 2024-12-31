#include <iostream>
#include <iomanip>

#include "dpu.hpp"
#include "dma_common.hpp"

void exampleCallBack(std::optional<std::string> s) {
  if (s) {
    std::cout << "Found: " << s.value() << std::endl;
  } else {
    std::cout << "KV not found\n";
  }
}

int main() {
  init_log_backend();
  Dpu dpu("03:00.0");
  for (size_t i = 1; i < 10; i++) {
    dpu.Insert(i, i * 10);
  }
  for (size_t i = 1; i < 10; i++) {
    dpu.Search(i, &exampleCallBack);
  }
  std::this_thread::sleep_for(std::chrono::seconds(3));
  dpu.FlushAll();
  // dpu.DebugPrintCache();
  while (true) {
    char c = getchar();
    if (c == 'q' || c == 'Q')
      break;
  }
  return 0;
}