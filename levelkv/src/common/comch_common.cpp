#include <memory>

#include "comch_common.hpp"
#include "dma_common.hpp"
#include "utils.hpp"

// Comch::Comch(const std::string &pcie_addr) {
//   dev_ = open_device(pcie_addr);
//   pe_ = create_pe();
// }

static void
comch_server_recv_callback(doca_comch_event_msg_recv *event,
                           uint8_t *recv_buffer, uint32_t msg_len,
                           doca_comch_connection *comch_connection) {
  std::cout << "Server recv callback called... \n";
}

static void
comch_client_recv_callback(doca_comch_event_msg_recv *event,
                           uint8_t *recv_buffer, uint32_t msg_len,
                           doca_comch_connection *comch_connection) {
  std::cout << "Client recv callback called... \n";
}

static void
server_connection_cb(struct doca_comch_event_connection_status_changed *event,
                     struct doca_comch_connection *comch_connection,
                     uint8_t change_successful) {
  doca_comch_server *server =
      doca_comch_server_get_server_ctx(comch_connection);
  doca_data ctx_user_data = {0};
  ComchCfg *comch_cfg = nullptr;
  ENSURE(change_successful, "Connection event unsuccessful");
  doca_ctx_get_user_data(doca_comch_server_as_ctx(server), &ctx_user_data);
  comch_cfg = reinterpret_cast<ComchCfg *>(ctx_user_data.ptr);
  ENSURE(comch_cfg, "Failed to get configuration from server context");
  ENSURE(comch_cfg->active_connection_ != nullptr,
         "A connection already exists on the server");
  doca_comch_connection_set_user_data(comch_connection, ctx_user_data);
  comch_cfg->active_connection_ = comch_connection;
}

static void server_disconnection_cb(
    struct doca_comch_event_connection_status_changed *event,
    struct doca_comch_connection *comch_connection, uint8_t change_successful) {
  doca_comch_server *server =
      doca_comch_server_get_server_ctx(comch_connection);
  doca_data ctx_user_data = {0};
  ComchCfg *comch_cfg;
  doca_ctx_get_user_data(doca_comch_server_as_ctx(server), &ctx_user_data);
  comch_cfg = reinterpret_cast<ComchCfg *>(ctx_user_data.ptr);
  comch_cfg->active_connection_ = NULL;
}

static void comch_send_completion(struct doca_comch_task_send *task,
                                  union doca_data task_user_data,
                                  union doca_data ctx_user_data) {
  doca_task_free(doca_comch_task_send_as_task(task));
}

static void comch_send_completion_err(struct doca_comch_task_send *task,
                                      union doca_data task_user_data,
                                      union doca_data ctx_user_data) {

  doca_task_free(doca_comch_task_send_as_task(task));
  ENSURE(0, "Comch task failed");
}

std::unique_ptr<ComchCfg> comch_init(const std::string &name,
                                     const std::string &pcie_addr,
                                     const std::string &pcie_rep_addr,
                                     void *user_data) {
  auto cfg = std::make_unique<ComchCfg>();
  cfg->user_data_ = user_data;
  doca_data comch_user_data;
  comch_user_data.ptr = (void *)cfg.get();
  cfg->pe_ = create_pe();
  cfg->dev_ = open_device(pcie_addr);
  doca_comch_cap_get_max_msg_size(doca_dev_as_devinfo(cfg->dev_),
                                  &cfg->max_buf_size_);
  std::cout << "Max buffer size: " << cfg->max_buf_size_ << std::endl;
#ifdef DOCA_ARCH_DPU
  cfg->is_server_ = true;
  cfg->dev_rep_ = open_device_rep(pcie_rep_addr, cfg->dev_);
  doca_comch_server_create(cfg->dev_, cfg->dev_rep_, name.c_str(),
                           &cfg->server_);
  doca_comch_server_task_send_set_conf(cfg->server_, comch_send_completion,
                                       comch_send_completion_err, 1024);
  doca_comch_server_event_msg_recv_register(cfg->server_,
                                            comch_server_recv_callback);
  doca_comch_server_event_connection_status_changed_register(
      cfg->server_, server_connection_cb, server_disconnection_cb);
  cfg->ctx_ = doca_comch_server_as_ctx(cfg->server_);
#else
  auto result =
      doca_comch_cap_client_is_supported(doca_dev_as_devinfo(cfg->dev_));
  ENSURE(result == DOCA_SUCCESS, "Client dev does not support comch");
  cfg->is_server_ = false;
  doca_comch_client_create(cfg->dev_, name.c_str(), &cfg->client_);
  doca_comch_client_set_max_msg_size(cfg->client_, cfg->max_buf_size_);
  cfg->ctx_ = doca_comch_client_as_ctx(cfg->client_);
  doca_comch_client_task_send_set_conf(cfg->client_, comch_send_completion,
                                       comch_send_completion_err, 1024);
  doca_comch_client_event_msg_recv_register(cfg->client_,
                                            comch_client_recv_callback);
#endif
  doca_pe_connect_ctx(cfg->pe_, cfg->ctx_);
  doca_ctx_set_user_data(cfg->ctx_, comch_user_data);
  cfg->active_connection_ = nullptr;
  doca_ctx_start(cfg->ctx_);
  std::cout << "1\n";

#ifdef DOCA_ARCH_DPU
  while (cfg->active_connection_ == nullptr) {
    doca_pe_progress(cfg->pe_);
  }
  std::cout << "Dpu: comch connection established.\n";
#else
  doca_ctx_states state;
  doca_ctx_get_state(cfg->ctx_, &state);
  while (state != DOCA_CTX_STATE_RUNNING) {
    doca_pe_progress(cfg->pe_);
    doca_ctx_get_state(cfg->ctx_, &state);
  }
  doca_comch_client_get_connection(cfg->client_, &cfg->active_connection_);
  doca_comch_connection_set_user_data(cfg->active_connection_, comch_user_data);
  std::cout << "Host: comch connection established.\n";
#endif
  return cfg;
}
