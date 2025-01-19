#include "dma_server.hpp"
#include "dma_common.hpp"
#include "utils.hpp"

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

ComchMsg DmaServer::ExportMmap() {
  ComchMsg msg;
  msg.msg_type_ = ComchMsgType::COMCH_MSG_EXPORT_DESCRIPTOR;
  msg.exp_msg_.host_addr_ = (uint64_t)addr_;
  const void * exported_desc;
  size_t exported_desc_len = 0;
  doca_mmap_export_pci(mmap_, dev_, &exported_desc, &exported_desc_len);
  ENSURE(exported_desc && exported_desc_len > 0 && exported_desc_len < MAX_EXPORTED_DESC_LEN, "Mmap export failed");
  msg.exp_msg_.exported_desc_len_ = exported_desc_len;
  memcpy(msg.exp_msg_.exported_mmap_, exported_desc, exported_desc_len);
  return msg;
}

void DmaServer::ExportMmapToFile() {
  export_mmap_to_files(
      mmap_, dev_, addr_, len_,
      export_desc_file_name_base + std::to_string(dma_server_id_) + ".txt",
      buffer_info_file_name_base + std::to_string(dma_server_id_) + ".txt");
}
