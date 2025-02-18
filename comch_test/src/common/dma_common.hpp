#pragma once

#include <future>
#include <iostream>
#include <memory>
#include <string>

#include <doca_buf.h>
#include <doca_buf_inventory.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_dma.h>
#include <doca_log.h>
#include <doca_mmap.h>
#include <doca_pe.h>

typedef doca_error_t (*tasks_check)(
    doca_devinfo *); // Function to check if a given device is capable of
                     // executing some task

doca_error_t open_doca_device_with_pci(const char *pci_addr, tasks_check func,
                                       doca_dev **retval);

doca_error_t open_doca_device_rep_with_pci(struct doca_dev *local,
                                           const char *pci_addr,
                                           struct doca_dev_rep **retval);

doca_dev *open_device(const std::string &pcie_addr);

doca_dev_rep *open_device_rep(const std::string &pcie_rep_addr,
                              doca_dev *local);

