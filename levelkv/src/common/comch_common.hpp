#pragma once

#include <cstdint>
#include <functional>
#include <memory>
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

class Comch {
public:
  Comch(bool is_server, const std::string &name, const std::string &pcie_addr,
        const std::string &pcie_rep_addr,
        doca_comch_event_msg_recv_cb_t recv_callback,
        doca_comch_task_send_completion_cb_t send_complete_callback,
        doca_comch_task_send_completion_cb_t send_err_callback,
        doca_comch_event_connection_status_changed_cb_t connect_callback,
        doca_comch_event_connection_status_changed_cb_t disconnect_callback,
        void *user_data);
  ~Comch();

public:
  bool is_server_;
  uint32_t max_buf_size_;
  doca_comch_connection *connection_;
  doca_dev *dev_;
  doca_dev_rep *dev_rep_;
  doca_pe *pe_;
  doca_ctx *ctx_;
  union {
    doca_comch_server *server_;
    doca_comch_client *client_;
  };

  void *user_data_;
};

// std::unique_ptr<ComchCfg> comch_init(const char *name, const char *pci_addr,
//                                      const char *rep_pci_addr, void
//                                      *user_data);

// doca_error_t comch_send(doca_comch_connection *connection, const void *msg,
//                         uint32_t len);

void comch_server_recv_callback(doca_comch_event_msg_recv *event,
                                uint8_t *recv_buffer, uint32_t msg_len,
                                doca_comch_connection *comch_connection);

void comch_client_recv_callback(doca_comch_event_msg_recv *event,
                                uint8_t *recv_buffer, uint32_t msg_len,
                                doca_comch_connection *comch_connection);

void server_connection_cb(
    struct doca_comch_event_connection_status_changed *event,
    struct doca_comch_connection *comch_connection, uint8_t change_successful);

void server_disconnection_cb(
    struct doca_comch_event_connection_status_changed *event,
    struct doca_comch_connection *comch_connection, uint8_t change_successful);

void comch_send_completion(struct doca_comch_task_send *task,
                           union doca_data task_user_data,
                           union doca_data ctx_user_data);

void comch_send_completion_err(struct doca_comch_task_send *task,
                               union doca_data task_user_data,
                               union doca_data ctx_user_data);