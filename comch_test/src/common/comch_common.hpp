#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <iostream>

#include <doca_comch.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_pe.h>

#define SLEEP_IN_NANOS (10 * 1000)

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

  void Send(const void *msg, uint32_t len);

  void Progress();

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