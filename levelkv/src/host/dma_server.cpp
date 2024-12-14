#include "dma_server.hpp"
#include "dma_common.hpp"

DmaServer::DmaServer(uint64_t dma_server_id, const std::string &pcie_addr,
                     char *addr, size_t len)
    : dma_server_id_(dma_server_id), pcie_addr_(pcie_addr), addr_(addr),
      len_(len) {
  dev_ = open_device(pcie_addr);
  mmap_ = create_mmap(addr, len, dev_);
}

DmaServer::~DmaServer() {
  if (mmap_) {
    doca_mmap_stop(mmap_);
    doca_mmap_destroy(mmap_);
  }
  if (dev_)
    doca_dev_close(dev_);
}

void DmaServer::ExportMmapToFile() {
  export_mmap_to_files(
      mmap_, dev_, addr_, len_,
      export_desc_file_name_base + std::to_string(dma_server_id_) + ".txt",
      buffer_info_file_name_base + std::to_string(dma_server_id_) + ".txt");
}
