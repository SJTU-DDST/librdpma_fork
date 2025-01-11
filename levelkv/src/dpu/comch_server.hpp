#pragma once

#include <string>
#include <functional>

#include <doca_comch.h>
#include <doca_pe.h>
#include <doca_dev.h>
#include <doca_dev.h>
#include <doca_ctx.h>

#include "dpu.hpp"
#include "comch_common.hpp"

class ComchServer : public Comch {
public:
  ComchServer(const std::string &pcie_addr, const std::string &pcie_rep_addr, Dpu *dpu);
  void SendExpand();

private:
  std::string pcie_rep_addr_;
  doca_dev_rep *dev_rep_;
  doca_comch_server *comch_server_;
  Dpu *dpu_;
};