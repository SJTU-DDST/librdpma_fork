#include "dma_common.hpp"

static doca_error_t check_dev_dma_capable(doca_devinfo *devinfo) {
  doca_error_t status = doca_dma_cap_task_memcpy_is_supported(devinfo);
  if (status != DOCA_SUCCESS)
    return status;
  return DOCA_SUCCESS;
}

doca_error_t open_doca_device_with_pci(const char *pci_addr, tasks_check func,
                                       doca_dev **retval) {
  doca_devinfo **dev_list;
  uint32_t nb_devs;
  uint8_t is_addr_equal = 0;
  doca_error_t res;
  size_t i;
  *retval = NULL;
  res = doca_devinfo_create_list(&dev_list, &nb_devs);
  if (res != DOCA_SUCCESS)
    return res;

  for (i = 0; i < nb_devs; i++) {
    res = doca_devinfo_is_equal_pci_addr(dev_list[i], pci_addr, &is_addr_equal);
    if (res == DOCA_SUCCESS && is_addr_equal) {
      if (func != NULL && func(dev_list[i]) != DOCA_SUCCESS)
        continue;
      res = doca_dev_open(dev_list[i], retval);
      if (res == DOCA_SUCCESS) {
        doca_devinfo_destroy_list(dev_list);
        return res;
      }
    }
  }
  doca_devinfo_destroy_list(dev_list);
  return DOCA_ERROR_NOT_FOUND;
}

doca_error_t open_doca_device_rep_with_pci(struct doca_dev *local,
                                           const char *pci_addr,
                                           struct doca_dev_rep **retval) {
  uint32_t nb_rdevs = 0;
  struct doca_devinfo_rep **rep_dev_list = NULL;
  uint8_t is_addr_equal = 0;
  doca_error_t result;
  size_t i;
  *retval = NULL;

  result = doca_devinfo_rep_create_list(local, DOCA_DEVINFO_REP_FILTER_NET,
                                        &rep_dev_list, &nb_rdevs);
  if (result != DOCA_SUCCESS)
    return result;

  for (i = 0; i < nb_rdevs; i++) {
    result = doca_devinfo_rep_is_equal_pci_addr(rep_dev_list[i], pci_addr,
                                                &is_addr_equal);
    if (result == DOCA_SUCCESS && is_addr_equal &&
        doca_dev_rep_open(rep_dev_list[i], retval) == DOCA_SUCCESS) {
      doca_devinfo_rep_destroy_list(rep_dev_list);
      return DOCA_SUCCESS;
    }
  }
  doca_devinfo_rep_destroy_list(rep_dev_list);
  return DOCA_ERROR_NOT_FOUND;
}

doca_dev *open_device(const std::string &pcie_addr) {
  doca_dev *dev;
  open_doca_device_with_pci(pcie_addr.c_str(), check_dev_dma_capable, &dev);
  return dev;
}

doca_dev_rep *open_device_rep(const std::string &pcie_rep_addr, doca_dev *local) {
  doca_dev_rep *dev_rep;
  open_doca_device_rep_with_pci(local, pcie_rep_addr.c_str(), &dev_rep);
  return dev_rep;
}
