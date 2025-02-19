#include <memory>

#include "comch_common.hpp"
#include "dma_common.hpp"
#include "utils.hpp"

void server_connection_cb(
    struct doca_comch_event_connection_status_changed *event,
    struct doca_comch_connection *comch_connection, uint8_t change_successful) {
  doca_comch_server *server =
      doca_comch_server_get_server_ctx(comch_connection);
  doca_data ctx_user_data = {0};
  doca_ctx_get_user_data(doca_comch_server_as_ctx(server), &ctx_user_data);
  Comch *comch = reinterpret_cast<Comch *>(ctx_user_data.ptr);
  doca_comch_connection_set_user_data(comch_connection, ctx_user_data);
  comch->connection_ = comch_connection;
}

void server_disconnection_cb(
    struct doca_comch_event_connection_status_changed *event,
    struct doca_comch_connection *comch_connection, uint8_t change_successful) {
  doca_comch_server *server =
      doca_comch_server_get_server_ctx(comch_connection);
  doca_data ctx_user_data = {0};
  doca_ctx_get_user_data(doca_comch_server_as_ctx(server), &ctx_user_data);
  Comch *comch = reinterpret_cast<Comch *>(ctx_user_data.ptr);
  comch->connection_ = nullptr;
}

void comch_send_completion(struct doca_comch_task_send *task,
                           union doca_data task_user_data,
                           union doca_data ctx_user_data) {
  doca_task_free(doca_comch_task_send_as_task(task));
}

void comch_send_completion_err(struct doca_comch_task_send *task,
                               union doca_data task_user_data,
                               union doca_data ctx_user_data) {

  doca_task_free(doca_comch_task_send_as_task(task));
  std::cerr << "Comch task failed.\n";
}

Comch::Comch(
    bool is_server, const std::string &name, const std::string &pcie_addr,
    const std::string &pcie_rep_addr,
    doca_comch_event_msg_recv_cb_t recv_callback,
    doca_comch_task_send_completion_cb_t send_complete_callback,
    doca_comch_task_send_completion_cb_t send_err_callback,
    doca_comch_event_connection_status_changed_cb_t connect_callback,
    doca_comch_event_connection_status_changed_cb_t disconnect_callback,
    void *user_data) {
  struct timespec ts = {
      .tv_nsec = SLEEP_IN_NANOS,
  };
  union doca_data comch_user_data = {0};
#ifdef DOCA_ARCH_DPU
  is_server_ = true;
#else
  is_server_ = false;
#endif

  user_data_ = user_data;
  comch_user_data.ptr = this;
  doca_pe_create(&pe_);
  dev_ = open_device(pcie_addr);
  doca_comch_cap_get_max_msg_size(doca_dev_as_devinfo(dev_), &max_buf_size_);
  std::cout << max_buf_size_ << std::endl;

  if (is_server_) {
    dev_rep_ = open_device_rep(pcie_rep_addr, dev_);
    doca_comch_server_create(dev_, dev_rep_, name.c_str(), &server_);
    doca_comch_server_set_max_msg_size(server_, max_buf_size_);
    ctx_ = doca_comch_server_as_ctx(server_);
    doca_pe_connect_ctx(pe_, ctx_);
    doca_comch_server_task_send_set_conf(server_, send_complete_callback,
                                         send_err_callback, 1024);
    doca_comch_server_event_msg_recv_register(server_, recv_callback);
    doca_comch_server_event_connection_status_changed_register(
        server_, connect_callback, disconnect_callback);
    doca_ctx_set_user_data(ctx_, comch_user_data);
    connection_ = NULL;
    doca_ctx_start(ctx_);
    std::cout << "Server waiting on a client to connect\n";
    while (connection_ == NULL) {
      (void)doca_pe_progress(pe_);
      nanosleep(&ts, &ts);
    }
    std::cout << "Server connection established\n";
  } else {
    doca_comch_client_create(dev_, name.c_str(), &client_);
    doca_comch_client_set_max_msg_size(client_, max_buf_size_);
    ctx_ = doca_comch_client_as_ctx(client_);
    doca_pe_connect_ctx(pe_, ctx_);
    doca_comch_client_task_send_set_conf(
        client_, send_complete_callback, send_err_callback, 1024);
    doca_comch_client_event_msg_recv_register(client_, recv_callback);
    doca_ctx_set_user_data(ctx_, comch_user_data);
    doca_ctx_start(ctx_);

    /* Wait for client/server handshake to complete */
    doca_ctx_states state;
    (void)doca_ctx_get_state(ctx_, &state);
    while (state != DOCA_CTX_STATE_RUNNING) {
      (void)doca_pe_progress(pe_);
      nanosleep(&ts, &ts);
      (void)doca_ctx_get_state(ctx_, &state);
    }
    (void)doca_comch_client_get_connection(client_, &connection_);
    doca_comch_connection_set_user_data(connection_, comch_user_data);
  }
}

Comch::~Comch() {
  doca_ctx_states ctx_state;
  if (is_server_) {
    /* Wait until the client has closed the connection to end gracefully */
    while (connection_ != nullptr) {
      Progress();
    }
  }

  doca_ctx_stop(ctx_);
  doca_ctx_get_state(ctx_, &ctx_state);
  while (ctx_state != DOCA_CTX_STATE_IDLE) {
    Progress();
    (void)doca_ctx_get_state(ctx_, &ctx_state);
  }

  if (is_server_) {
    doca_comch_server_destroy(server_);
    doca_dev_rep_close(dev_rep_);
  } else {
    doca_comch_client_destroy(client_);
  }
  doca_dev_close(dev_);
  doca_pe_destroy(pe_);
}

void Comch::Send(const void *msg, uint32_t len) {
  doca_data comch_user_data = doca_comch_connection_get_user_data(connection_);
  Comch *comch = reinterpret_cast<Comch *>(comch_user_data.ptr);
  doca_comch_task_send *task;
  if (comch->is_server_)
    doca_comch_server_task_send_alloc_init(comch->server_, connection_, msg,
                                           len, &task);
  else
    doca_comch_client_task_send_alloc_init(comch->client_, connection_, msg,
                                           len, &task);
  doca_task_submit(doca_comch_task_send_as_task(task));
}

void Comch::Progress() { doca_pe_progress(pe_); }
