#include <iomanip>
#include <iostream>

#include "comch_common.hpp"
#include "dma_common.hpp"
#include "dpu.hpp"

void exampleCallBack(std::optional<std::string> s) {
  if (s) {
    std::cout << "Found: " << s.value() << std::endl;
  } else {
    std::cout << "KV not found\n";
  }
}

int main() {
  init_log_backend();
  Dpu dpu("03:00.0", "b5:00.0");
  // for (size_t i = 9; i > 0; i--) {
  //   dpu.Search(i, &exampleCallBack);
  // }
  // std::this_thread::sleep_for(std::chrono::seconds(3));
  // dpu.FlushAll();
  // auto cfg = comch_init("Server", "03:00.0", "b5:00.0", nullptr);
  // std::string msg = "Hello this is the comch testing message!";
  // comch_send(cfg->active_connection_, msg.c_str(), msg.length());
  while (true) {
    char c = getchar();
    if (c == 'q' || c == 'Q')
      break;
  }
  return 0;
}