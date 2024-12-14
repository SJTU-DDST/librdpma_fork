#pragma once

#include <string>

#include <doca_dev.h>
#include <doca_mmap.h>

class DmaServer{
public:
  DmaServer(uint64_t dma_server_id, const std::string &pcie_addr, char *addr, size_t len);

  ~DmaServer();

  void ExportMmapToFile();
private:
  uint64_t dma_server_id_;
  std::string pcie_addr_;
  char *addr_;
  size_t len_;

  doca_dev *dev_;
  doca_mmap *mmap_;
};