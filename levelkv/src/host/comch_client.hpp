#pragma once

#include <functional>
#include <string>

#include <doca_comch.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_pe.h>

#include "comch_common.hpp"
#include "host.hpp"

class ComchClient : public Comch {
public:
  ComchClient(const std::string &pcie_addr, Host *host);
  std::function<void(ComchMsg *msg, Host &host)> client_recv_callback_;
  Host *host_;

private:
  doca_comch_client *comch_client_;
};