#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include <doca_comch.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_pe.h>

enum class ComchMsgType {
  COMCH_MSG_EXPORT_DESCRIPTOR,
  COMCH_MSG_CONTROL,
};

enum class ControlSignal {
  EXPAND,
};

struct ComchMsgExportMmap {
  uint64_t host_addr_;
  size_t exported_desc_len_;
  uint8_t exported_mmap_[];
};

struct ComchMsgControl {
  ControlSignal control_signal_;
};

struct ComchMsg {
  ComchMsgType msg_type_;
  union {
    ComchMsgExportMmap exp_msg_;
    ComchMsgControl ctl_msg_;
  };
};

enum class ComchState {
  COMCH_NEGOTIATING, /* DMA metadata is being negotiated */
  COMCH_COMPLETE,    /* DMA metadata successfully passed */
  COMCH_ERROR,       /* An error was detected DMA metadata negotiation */
};

struct ComchCfg {
  void *user_data_;      /* User-data supplied by the app */
  struct doca_pe *pe_;   /* Progress engine for comch */
  struct doca_ctx *ctx_; /* Doca context of the client/server */
  union {
    struct doca_comch_server *server_; /* Server context (DPU only) */
    struct doca_comch_client *client_; /* Client context (x86 host only) */
  };
  struct doca_comch_connection
      *active_connection_;       /* Single connection active on the channel */
  struct doca_dev *dev_;         /* Device in use */
  struct doca_dev_rep *dev_rep_; /* Representor in use (DPU only) */
  uint32_t max_buf_size_;        /* Maximum size of message on channel */
  uint8_t is_server_;            /* Indicator of client or server */
};

// class Comch {
// public:
//   Comch(const std::string &pcie_addr);
//   std::function<void(uint8_t *recv_buffer, uint32_t msg_len)> recv_callback_;

// protected:
//   std::string pcie_addr_;

//   doca_comch_connection *connection_;
//   doca_dev *dev_;
//   doca_pe *pe_;
//   doca_ctx *ctx_;
// };

std::unique_ptr<ComchCfg> comch_init(const char *name, const char *pci_addr,
                                     const char *rep_pci_addr, void *user_data);
