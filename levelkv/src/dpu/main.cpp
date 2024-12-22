#include <iostream>
#include <iomanip>

#include "dpu.hpp"

void exampleCallBack(std::optional<std::string> s) {
  if (s) {
    std::cout << "Found: " << s.value() << std::endl;
  } else {
    std::cout << "KV not found\n";
  }
}

int main() {
  Dpu dpu("03:00.0");
  dpu.Search(5, &exampleCallBack);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  dpu.DebugPrintCache();
  while (true) {
    char c = getchar();
    if (c == 'q' || c == 'Q')
      break;
  }
  return 0;
}