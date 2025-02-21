#include <iomanip>
#include <iostream>

#include "comch_common.hpp"
#include "dma_common.hpp"
#include "dpu.hpp"
#include "levelhashing_dpu.hpp"

void exampleCallBack(std::optional<std::string> s) {
  if (s) {
    std::cout << "Found: " << s.value() << std::endl;
  } else {
    std::cout << "KV not found\n";
  }
}

int main() {
  // Dpu dpu("03:00.0", "b5:00.0");
  // print_time();
  // for (size_t i = 1; i <= 500000; i++) {
  //   dpu.Insert(i, i);
  // }
  // dpu.Start();
  // for (size_t i = 1; i <= 500000; i++) {
  //   dpu.Search(i, exampleCallBack);
  // }
  // dpu.End();
  // print_time();
  // dpu.RunBegin();
  // while (!dpu.stop_) {
  //   std::this_thread::sleep_for(std::chrono::milliseconds(50));
  // }
  LevelHashingDpu dpu("03:00.0", "b5:00.0");
  for (size_t i = 0; i < 100; i++) {
    dpu.Insert(i, i * 10);
  }
  for (size_t i = 0; i < 100; i++) {
    dpu.Search(i, exampleCallBack);
  }

  std::cout << "dpu exit\n" << std::endl;
  return 0;
}