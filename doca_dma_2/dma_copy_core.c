/*
 * Copyright (c) 2022-2023 NVIDIA CORPORATION AND AFFILIATES.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *     * Neither the name of the NVIDIA CORPORATION nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL NVIDIA CORPORATION BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TOR
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

#include <doca_buf.h>
#include <doca_buf_inventory.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_dma.h>
#include <doca_mmap.h>

#include <samples/common.h>

#include "pack.h"
#include "utils.h"

#include "benchmark.h"
#include "dma_copy_core.h"

#define CC_MAX_QUEUE_SIZE                                                      \
  10                         /* Max number of messages on Comm Channel queue   \
                              */
#define SLEEP_IN_NANOS (10)  /* Sample the task every 10 microseconds */
#define STATUS_SUCCESS true  /* Successful status */
#define STATUS_FAILURE false /* Unsuccessful status */

DOCA_LOG_REGISTER(DMA_COPY_CORE);

/*
 * Get DOCA DMA maximum buffer size allowed
 *
 * @resources [in]: DOCA DMA resources pointer
 * @max_buf_size [out]: Maximum buffer size allowed
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t get_dma_max_buf_size(struct dma_copy_resources *resources,
                                         uint64_t *max_buf_size) {
  struct doca_devinfo *dma_dev_info =
      doca_dev_as_devinfo(resources->state->dev);
  doca_error_t result;

  result =
      doca_dma_cap_task_memcpy_get_max_buf_size(dma_dev_info, max_buf_size);
  if (result != DOCA_SUCCESS)
    DOCA_LOG_ERR("Failed to retrieve maximum buffer size allowed from DOCA DMA "
                 "device: %s",
                 doca_error_get_descr(result));
  else
    DOCA_LOG_INFO("DOCA DMA device supports maximum buffer size of %" PRIu64
                  " bytes",
                  *max_buf_size);

  return result;
}

/*
 * ARGP validation Callback - check if input file exists
 *
 * @config [in]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t args_validation_callback(void *config) {
  // struct dma_copy_cfg *cfg = (struct dma_copy_cfg *)config;

  // if (access(cfg->file_path, F_OK | R_OK) == 0) {
  //   cfg->is_file_found_locally = true;
  //   return validate_file_size(cfg->file_path, &cfg->file_size);
  // }

  return DOCA_SUCCESS;
}

static doca_error_t file_size_callback(void *param, void *config) {
  struct dma_copy_cfg *cfg = (struct dma_copy_cfg *)config;
  int *file_size = (int *)param;

  if (*file_size > MAX_FILE_SIZE || *file_size < 1) {
    DOCA_LOG_INFO("Entered file size number is not within the range of 1 to %d",
                  MAX_FILE_SIZE);
    return DOCA_ERROR_INVALID_VALUE;
  }
  cfg->file_size = *file_size;
  cfg->stat.file_size = *file_size;

  return DOCA_SUCCESS;
}

static doca_error_t num_operations_callback(void *param, void *config) {
  struct dma_copy_cfg *cfg = (struct dma_copy_cfg *)config;
  int *ops = (int *)param;

  if (*ops > MAX_OPS || *ops < 1) {
    DOCA_LOG_INFO("Entered oprations number is not within the range of 1 to %d",
                  MAX_OPS);
    return DOCA_ERROR_INVALID_VALUE;
  }
  cfg->stat.ops = *ops;

  return DOCA_SUCCESS;
}

static doca_error_t num_threads_callback(void *param, void *config) {
  struct dma_copy_cfg *cfg = (struct dma_copy_cfg *)config;
  int *threads = (int *)param;

  if (*threads < 1 || *threads > 1024) {
    DOCA_LOG_INFO(
        "Entered number of threads is not within the range of 1 to %d", 1024);
    return DOCA_ERROR_INVALID_VALUE;
  }
  cfg->num_threads = *threads;

  return DOCA_SUCCESS;
}

/*
 * ARGP Callback - Handle Comm Channel DOCA device PCI address parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t dev_pci_addr_callback(void *param, void *config) {
  struct dma_copy_cfg *cfg = (struct dma_copy_cfg *)config;
  const char *dev_pci_addr = (char *)param;

  if (strnlen(dev_pci_addr, DOCA_DEVINFO_PCI_ADDR_SIZE) ==
      DOCA_DEVINFO_PCI_ADDR_SIZE) {
    DOCA_LOG_ERR("Entered device PCI address exceeding the maximum size of %d",
                 DOCA_DEVINFO_PCI_ADDR_SIZE - 1);
    return DOCA_ERROR_INVALID_VALUE;
  }

  strlcpy(cfg->cc_dev_pci_addr, dev_pci_addr, DOCA_DEVINFO_PCI_ADDR_SIZE);

  return DOCA_SUCCESS;
}

/*
 * ARGP Callback - Handle Comm Channel DOCA device representor PCI address
 * parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t rep_pci_addr_callback(void *param, void *config) {
  struct dma_copy_cfg *cfg = (struct dma_copy_cfg *)config;
  const char *rep_pci_addr = (char *)param;

  if (cfg->mode == DMA_COPY_MODE_DPU) {
    if (strnlen(rep_pci_addr, DOCA_DEVINFO_REP_PCI_ADDR_SIZE) ==
        DOCA_DEVINFO_REP_PCI_ADDR_SIZE) {
      DOCA_LOG_ERR("Entered device representor PCI address exceeding the "
                   "maximum size of %d",
                   DOCA_DEVINFO_REP_PCI_ADDR_SIZE - 1);
      return DOCA_ERROR_INVALID_VALUE;
    }

    strlcpy(cfg->cc_dev_rep_pci_addr, rep_pci_addr,
            DOCA_DEVINFO_REP_PCI_ADDR_SIZE);
  }

  return DOCA_SUCCESS;
}

/*
 * Allocate memory and populate it into the memory map
 *
 * @mmap [in]: DOCA memory map
 * @buffer_len [in]: Allocated buffer length
 * @access_flags [in]: The access permissions of the mmap
 * @buffer [out]: Allocated buffer
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t memory_alloc_and_populate(struct doca_mmap *mmap,
                                              size_t buffer_len,
                                              uint32_t access_flags,
                                              char **buffer) {
  doca_error_t result;

  result = doca_mmap_set_permissions(mmap, access_flags);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Unable to set access permissions of memory map: %s",
                 doca_error_get_descr(result));
    return result;
  }
  *buffer = (char *)memalign(64, buffer_len);
  DOCA_LOG_INFO("Buf_len: %lu", buffer_len);
  if (*buffer == NULL) {
    DOCA_LOG_ERR("Failed to allocate memory for source buffer");
    return DOCA_ERROR_NO_MEMORY;
  }

  result = doca_mmap_set_memrange(mmap, *buffer, buffer_len);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Unable to set memrange of memory map: %s",
                 doca_error_get_descr(result));
    free(*buffer);
    return result;
  }

  /* Populate local buffer into memory map to allow access from DPU side after
   * exporting */
  result = doca_mmap_start(mmap);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Unable to populate memory map: %s",
                 doca_error_get_descr(result));
    free(*buffer);
  }

  return result;
}

/*
 * DPU side function for submitting DMA task into the progress engine, wait for
 * its completion and save it into a file if needed.
 *
 * @cfg [in]: Application configuration
 * @resources [in]: DMA copy resources
 * @bytes_to_copy [in]: Number of bytes to DMA copy
 * @buffer [in]: local DMA buffer
 * @local_doca_buf [in]: local DOCA buffer
 * @remote_doca_buf [in]: remote DOCA buffer
 * @num_remaining_tasks [in]: Number of remaining tasks
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t dpu_submit_dma_task(
    struct dma_copy_cfg *cfg, struct dma_copy_resources *resources,
    size_t bytes_to_copy, char *buffer, struct doca_buf *local_doca_buf,
    struct doca_buf *remote_doca_buf, size_t *num_remaining_tasks,
    int thread_idx, struct timespec *start, struct timespec *end) {
  struct program_core_objects *state = resources->state;
  struct doca_dma_task_memcpy *dma_task;
  struct doca_task *task;
  union doca_data task_user_data = {0};
  void *data;
  struct doca_buf *src_buf;
  struct doca_buf *dst_buf;
  struct timespec ts = {
      .tv_sec = 0,
      .tv_nsec = SLEEP_IN_NANOS,
  };
  doca_error_t result;
  doca_error_t task_result;

  /* Determine DMA copy direction */
  if (cfg->is_sender) {
    src_buf = local_doca_buf;
    dst_buf = remote_doca_buf;
  } else {
    src_buf = remote_doca_buf;
    dst_buf = local_doca_buf;
  }

  /* Set data position in src_buf */
  result = doca_buf_get_data(src_buf, &data);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to get data address from DOCA buffer: %s",
                 doca_error_get_descr(result));
    return result;
  }
  result = doca_buf_set_data(src_buf, data, bytes_to_copy);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to set data for DOCA buffer: %s",
                 doca_error_get_descr(result));
    return result;
  }

  /* Include result in user data of task to be used in the callbacks */
  task_user_data.ptr = &task_result;
  /* Allocate and construct DMA task */
  result =
      doca_dma_task_memcpy_alloc_init(resources->dma_ctx[thread_idx], src_buf,
                                      dst_buf, task_user_data, &dma_task);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to allocate DMA memcpy task: %s",
                 doca_error_get_descr(result));
    return result;
  }

  task = doca_dma_task_memcpy_as_task(dma_task);

  result = doca_task_submit(task);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to submit DMA task: %s", doca_error_get_descr(result));
    result = DOCA_ERROR_UNEXPECTED;
    goto free_task;
  }

  /* Wait for all tasks to be completed */
  while (*num_remaining_tasks > 0) {
    if (doca_pe_progress(state->pe[thread_idx]) == 0)
      nanosleep(&ts, &ts);
  }

  /* Check result of task according to the result we update in the callbacks
   */
  if (task_result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("DMA copy failed: %s", doca_error_get_descr(task_result));
    result = task_result;
    goto free_task;
  }

free_task:
  doca_task_free(task);
  return result;
}

/*
 * Check if DOCA device is DMA capable
 *
 * @devinfo [in]: Device to check
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t check_dev_dma_capable(struct doca_devinfo *devinfo) {
  return doca_dma_cap_task_memcpy_is_supported(devinfo);
}

doca_error_t register_dma_copy_params(void) {
  doca_error_t result;
  struct doca_argp_param *dev_pci_addr_param, *rep_pci_addr_param,
      *file_size_param, *num_ops_param, *num_threads_param;

  /* Create and register file size */
  result = doca_argp_param_create(&file_size_param);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to create ARGP param: %s",
                 doca_error_get_descr(result));
    return result;
  }
  doca_argp_param_set_short_name(file_size_param, "f");
  doca_argp_param_set_long_name(file_size_param, "file size");
  doca_argp_param_set_description(file_size_param,
                                  "Size in bytes of the data to transfer");
  doca_argp_param_set_callback(file_size_param, file_size_callback);
  doca_argp_param_set_type(file_size_param, DOCA_ARGP_TYPE_INT);
  doca_argp_param_set_mandatory(file_size_param);
  result = doca_argp_register_param(file_size_param);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to register program param: %s",
                 doca_error_get_descr(result));
    return result;
  }

  /* Create and register number of operations */
  result = doca_argp_param_create(&num_ops_param);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to create ARGP param: %s",
                 doca_error_get_descr(result));
    return result;
  }
  doca_argp_param_set_short_name(num_ops_param, "o");
  doca_argp_param_set_long_name(num_ops_param, "operations");
  doca_argp_param_set_description(num_ops_param,
                                  "Number of operations to perform");
  doca_argp_param_set_callback(num_ops_param, num_operations_callback);
  doca_argp_param_set_type(num_ops_param, DOCA_ARGP_TYPE_INT);
  doca_argp_param_set_mandatory(num_ops_param);
  result = doca_argp_register_param(num_ops_param);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to register program param: %s",
                 doca_error_get_descr(result));
    return result;
  }

  /* Create and register number of threads */
  result = doca_argp_param_create(&num_threads_param);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to create ARGP param: %s",
                 doca_error_get_descr(result));
    return result;
  }
  doca_argp_param_set_short_name(num_threads_param, "t");
  doca_argp_param_set_long_name(num_threads_param, "threads");
  doca_argp_param_set_description(num_threads_param, "Number of threads");
  doca_argp_param_set_callback(num_threads_param, num_threads_callback);
  doca_argp_param_set_type(num_threads_param, DOCA_ARGP_TYPE_INT);
  result = doca_argp_register_param(num_threads_param);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to register program param: %s",
                 doca_error_get_descr(result));
    return result;
  }

  /* Create and register Comm Channel DOCA device PCI address */
  result = doca_argp_param_create(&dev_pci_addr_param);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to create ARGP param: %s",
                 doca_error_get_descr(result));
    return result;
  }
  doca_argp_param_set_short_name(dev_pci_addr_param, "p");
  doca_argp_param_set_long_name(dev_pci_addr_param, "pci-addr");
  doca_argp_param_set_description(dev_pci_addr_param,
                                  "DOCA Comm Channel device PCI address");
  doca_argp_param_set_callback(dev_pci_addr_param, dev_pci_addr_callback);
  doca_argp_param_set_type(dev_pci_addr_param, DOCA_ARGP_TYPE_STRING);
  doca_argp_param_set_mandatory(dev_pci_addr_param);
  result = doca_argp_register_param(dev_pci_addr_param);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to register program param: %s",
                 doca_error_get_descr(result));
    return result;
  }

  /* Create and register Comm Channel DOCA device representor PCI address */
  result = doca_argp_param_create(&rep_pci_addr_param);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to create ARGP param: %s",
                 doca_error_get_descr(result));
    return result;
  }
  doca_argp_param_set_short_name(rep_pci_addr_param, "r");
  doca_argp_param_set_long_name(rep_pci_addr_param, "rep-pci");
  doca_argp_param_set_description(
      rep_pci_addr_param,
      "DOCA Comm Channel device representor PCI address (needed only on DPU)");
  doca_argp_param_set_callback(rep_pci_addr_param, rep_pci_addr_callback);
  doca_argp_param_set_type(rep_pci_addr_param, DOCA_ARGP_TYPE_STRING);
  result = doca_argp_register_param(rep_pci_addr_param);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to register program param: %s",
                 doca_error_get_descr(result));
    return result;
  }

  /* Register validation callback */
  result = doca_argp_register_validation_callback(args_validation_callback);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to register program validation callback: %s",
                 doca_error_get_descr(result));
    return result;
  }

  /* Register version callback for DOCA SDK & RUNTIME */
  result = doca_argp_register_version_callback(sdk_version_callback);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to register version callback: %s",
                 doca_error_get_descr(result));
    return result;
  }

  return DOCA_SUCCESS;
}

doca_error_t open_dma_device(struct doca_dev **dev) {
  doca_error_t result;

  result = open_doca_device_with_capabilities(check_dev_dma_capable, dev);
  if (result != DOCA_SUCCESS)
    DOCA_LOG_ERR("Failed to open DOCA DMA capable device: %s",
                 doca_error_get_descr(result));

  return result;
}

/*
 * DMA Memcpy task completed callback
 *
 * @dma_task [in]: Completed task
 * @task_user_data [in]: doca_data from the task
 * @ctx_user_data [in]: doca_data from the context
 */
static void dma_memcpy_completed_callback(struct doca_dma_task_memcpy *dma_task,
                                          union doca_data task_user_data,
                                          union doca_data ctx_user_data) {
  size_t *num_remaining_tasks = (size_t *)ctx_user_data.ptr;
  doca_error_t *result = (doca_error_t *)task_user_data.ptr;

  (void)dma_task;
  /* Decrement number of remaining tasks */
  --*num_remaining_tasks;
  /* Assign success to the result */
  *result = DOCA_SUCCESS;
}

/*
 * Memcpy task error callback
 *
 * @dma_task [in]: failed task
 * @task_user_data [in]: doca_data from the task
 * @ctx_user_data [in]: doca_data from the context
 */
static void dma_memcpy_error_callback(struct doca_dma_task_memcpy *dma_task,
                                      union doca_data task_user_data,
                                      union doca_data ctx_user_data) {
  size_t *num_remaining_tasks = (size_t *)ctx_user_data.ptr;
  struct doca_task *task = doca_dma_task_memcpy_as_task(dma_task);
  doca_error_t *result = (doca_error_t *)task_user_data.ptr;

  /* Decrement number of remaining tasks */
  --*num_remaining_tasks;
  /* Get the result of the task */
  *result = doca_task_get_status(task);
}

/*
 * Destroy copy resources
 *
 * @resources [in]: DMA copy resources
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
// static doca_error_t
// destroy_dma_copy_resources(struct dma_copy_resources *resources) {
//   struct program_core_objects *state = resources->state;
//   doca_error_t result = DOCA_SUCCESS, tmp_result;

//   if (resources->dma_ctx != NULL) {
//     tmp_result = doca_dma_destroy(resources->dma_ctx);
//     if (tmp_result != DOCA_SUCCESS) {
//       DOCA_ERROR_PROPAGATE(result, tmp_result);
//       DOCA_LOG_ERR("Failed to destroy DOCA DMA context: %s",
//                    doca_error_get_descr(tmp_result));
//     }
//   }

//   tmp_result = destroy_core_objects(state);
//   if (tmp_result != DOCA_SUCCESS) {
//     DOCA_ERROR_PROPAGATE(result, tmp_result);
//     DOCA_LOG_ERR("Failed to destroy DOCA core objects: %s",
//                  doca_error_get_descr(tmp_result));
//   }

//   free(resources->state);

//   return result;
// }

/*
 * Allocate DMA copy resources
 *
 * @resources [out]: DOCA DMA copy resources
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
allocate_dma_copy_resources(struct dma_copy_resources *resources,
                            int num_threads) {
  struct program_core_objects *state = NULL;
  doca_error_t result, tmp_result;
  /* Two buffers for source and destination */
  uint32_t max_bufs = 6;
  DOCA_LOG_INFO("Dev opened.");
  resources->dma_ctx = malloc(num_threads * sizeof(struct doca_dma *));
  resources->state = malloc(sizeof(struct program_core_objects));
  resources->state->pe = malloc(num_threads * sizeof(struct doca_pe *));
  resources->state->ctx = malloc(num_threads * sizeof(struct doca_ctx *));
  resources->state->buf_inv =
      malloc(num_threads * sizeof(struct doca_buf_inventory *));
  if (!resources->state || !resources->state->pe || !resources->state->ctx ||
      !resources->state->buf_inv) {
    result = DOCA_ERROR_NO_MEMORY;
    DOCA_LOG_ERR("Failed to allocate DOCA program core objects: %s",
                 doca_error_get_descr(result));
    return result;
  }
  state = resources->state;

  /* Open DOCA dma device */
  result = open_dma_device(&state->dev);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to open DMA device: %s", doca_error_get_descr(result));
    // goto free_state;
    return result;
  }
  result = doca_mmap_create(&state->dst_mmap);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Unable to create destination mmap: %s",
                 doca_error_get_descr(result));
    return result;
  }
  result = doca_mmap_add_dev(state->dst_mmap, state->dev);
  // if (result != DOCA_SUCCESS) {
  // 	DOCA_LOG_ERR("Unable to add device to destination mmap: %s",
  // doca_error_get_descr(result)); 	goto destroy_dst_mmap;
  // }

  for (int t = 0; t < num_threads; t++) {
    result = doca_buf_inventory_create(max_bufs, &state->buf_inv[t]);
    result = doca_buf_inventory_start(state->buf_inv[t]);
    result = doca_pe_create(&state->pe[t]);
    result = doca_dma_create(state->dev, &resources->dma_ctx[t]);
    if (result != DOCA_SUCCESS) {
      DOCA_LOG_ERR("Unable to create DOCA DMA context: %s",
                   doca_error_get_descr(result));
    }
    state->ctx[t] = doca_dma_as_ctx(resources->dma_ctx[t]);
    result = doca_pe_connect_ctx(state->pe[t], state->ctx[t]);
    result = doca_dma_task_memcpy_set_conf(resources->dma_ctx[t],
                                           dma_memcpy_completed_callback,
                                           dma_memcpy_error_callback, 1);
  }
  // if (max_bufs != 0) {
  // 	result = doca_buf_inventory_create(max_bufs, &state->buf_inv);
  // 	if (result != DOCA_SUCCESS) {
  // 		DOCA_LOG_ERR("Unable to create buffer inventory: %s",
  // doca_error_get_descr(result)); 		goto destroy_dst_mmap;
  // 	}

  // 	result = doca_buf_inventory_start(state->buf_inv);
  // 	if (result != DOCA_SUCCESS) {
  // 		DOCA_LOG_ERR("Unable to start buffer inventory: %s",
  // doca_error_get_descr(result)); 		goto destroy_buf_inv;
  // 	}
  // }

  // result = doca_pe_create(&state->pe);
  // if (result != DOCA_SUCCESS) {
  // 	DOCA_LOG_ERR("Unable to create progress engine: %s",
  // doca_error_get_descr(res)); 	goto destroy_buf_inv;
  // }

  // result = doca_dma_create(state->dev, &resources->dma_ctx);
  // if (result != DOCA_SUCCESS) {
  //   DOCA_LOG_ERR("Unable to create DOCA DMA context: %s",
  //                doca_error_get_descr(result));
  //   goto destroy_core_objects;
  // }

  // state->ctx = doca_dma_as_ctx(resources->dma_ctx);

  // result = doca_pe_connect_ctx(state->pe, state->ctx);
  // if (result != DOCA_SUCCESS) {
  //   DOCA_LOG_ERR("Unable to set DOCA progress engine to DOCA DMA: %s",
  //                doca_error_get_descr(result));
  //   goto destroy_dma;
  // }

  // result = doca_dma_task_memcpy_set_conf(
  //     resources->dma_ctx, dma_memcpy_completed_callback,
  //     dma_memcpy_error_callback, NUM_DMA_TASKS);
  // if (result != DOCA_SUCCESS) {
  //   DOCA_LOG_ERR("Unable to set configurations for DMA memcpy task: %s",
  //                doca_error_get_descr(result));
  //   goto destroy_dma;
  // }

  return result;

  // destroy_dma:
  //   tmp_result = doca_dma_destroy(resources->dma_ctx);
  //   if (tmp_result != DOCA_SUCCESS) {
  //     DOCA_ERROR_PROPAGATE(result, tmp_result);
  //     DOCA_LOG_ERR("Failed to destroy DOCA DMA context: %s",
  //                  doca_error_get_descr(tmp_result));
  //   }
  // destroy_core_objects:
  //   tmp_result = destroy_core_objects(state);
  //   if (tmp_result != DOCA_SUCCESS) {
  //     DOCA_ERROR_PROPAGATE(result, tmp_result);
  //     DOCA_LOG_ERR("Failed to destroy DOCA core objects: %s",
  //                  doca_error_get_descr(tmp_result));
  //   }
  // free_state:
  //   free(resources->state);

  return result;
}

/*
 * Helper function to send a status message across the comch
 *
 * @comch_connection [in]: comch connection to send the status message across
 * @status [in]: true means the status is success, false means a failure
 */
static void send_status_msg(struct doca_comch_connection *comch_connection,
                            bool status) {
  struct comch_msg_dma_status status_msg = {.type = COMCH_MSG_STATUS};
  doca_error_t result;

  status_msg.is_success = status;

  result = comch_utils_send(comch_connection, &status_msg,
                            sizeof(struct comch_msg_dma_status));
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to send status message: %s",
                 doca_error_get_descr(result));
  }
}

/*
 * Process and respond to a DMA direction negotiation message on the host
 *
 * @cfg [in]: dma copy configuration information
 * @comch_connection [in]: comch connection the message was received on
 * @dir_msg [in]: the direction message received
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t host_process_dma_direction_and_size(
    struct dma_copy_cfg *cfg, struct doca_comch_connection *comch_connection,
    struct comch_msg_dma_direction *dir_msg) {
  struct comch_msg_dma_export_discriptor *exp_msg;
  char export_msg[cfg->max_comch_buffer];
  size_t exp_msg_len;
  const void *export_desc;
  size_t export_desc_len;
  doca_error_t result;

  if (!cfg->is_sender) {
    cfg->file_size = ntohq(dir_msg->file_size);

    /* Allocate a buffer to receive the file data */
    result = memory_alloc_and_populate(cfg->file_mmap, cfg->file_size,
                                       DOCA_ACCESS_FLAG_PCI_READ_WRITE,
                                       &cfg->file_buffer);
    if (result != DOCA_SUCCESS) {
      DOCA_LOG_ERR("Failed to allocate recv buffer: %s",
                   doca_error_get_descr(result));
      return DOCA_ERROR_NO_MEMORY;
    }
  }

  /* Export the host mmap to the DPU to start the DMA */
  exp_msg = (struct comch_msg_dma_export_discriptor *)export_msg;
  exp_msg->type = COMCH_MSG_EXPORT_DESCRIPTOR;
  exp_msg->host_addr = htonq((uintptr_t)cfg->file_buffer);

  result = doca_mmap_export_pci(cfg->file_mmap, cfg->dev, &export_desc,
                                &export_desc_len);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to export DOCA mmap: %s",
                 doca_error_get_descr(result));
    return result;
  }

  exp_msg_len =
      export_desc_len + sizeof(struct comch_msg_dma_export_discriptor);
  if (exp_msg_len > cfg->max_comch_buffer) {
    DOCA_LOG_ERR("Export message exceeds max length of comch. Message len: "
                 "%lu, Max len: %u",
                 exp_msg_len, cfg->max_comch_buffer);
    return DOCA_ERROR_INVALID_VALUE;
  }

  exp_msg->export_desc_len = htonq(export_desc_len);
  memcpy(exp_msg->exported_mmap, export_desc, export_desc_len);

  /* It is assumed there are enough tasks for message to succeed - progress
   * should not be called from callback */
  return comch_utils_send(comch_connection, exp_msg, exp_msg_len);
}

void host_recv_event_cb(struct doca_comch_event_msg_recv *event,
                        uint8_t *recv_buffer, uint32_t msg_len,
                        struct doca_comch_connection *comch_connection) {
  struct dma_copy_cfg *cfg = comch_utils_get_user_data(comch_connection);
  struct comch_msg_dma_status *status;
  struct comch_msg *comch_msg;
  doca_error_t result;

  (void)event;

  if (cfg == NULL) {
    DOCA_LOG_ERR("Cannot get configuration information");
    return;
  }

  /* Message must at least contain a message type */
  if (msg_len < sizeof(enum comch_msg_type)) {
    DOCA_LOG_ERR("Received a message that is too small. Length: %u", msg_len);
    send_status_msg(comch_connection, STATUS_FAILURE);
    cfg->comch_state = COMCH_ERROR;
    return;
  }

  /* All messages should take the format of a comch_msg struct */
  comch_msg = (struct comch_msg *)recv_buffer;

  /*
   * The host will have started the DMA negotiation by sending a dma_direction
   * message. It should receive the same format message back from the DPU
   * containing file size if file in on DPU. The host will allocate space to
   * receive the file or use preallocated memory if the file is on the host. The
   * file location data is exported to the DPU. When DMA has completed, a status
   * message should be received.
   */

  switch (comch_msg->type) {
  case COMCH_MSG_DIRECTION:
    if (msg_len != sizeof(struct comch_msg_dma_direction)) {
      DOCA_LOG_ERR(
          "Direction message has bad length. Length: %u, expected: %lu",
          msg_len, sizeof(struct comch_msg_dma_direction));
      send_status_msg(comch_connection, STATUS_FAILURE);
      cfg->comch_state = COMCH_ERROR;
      return;
    }

    result = host_process_dma_direction_and_size(
        cfg, comch_connection, (struct comch_msg_dma_direction *)recv_buffer);
    if (result != DOCA_SUCCESS) {
      send_status_msg(comch_connection, STATUS_FAILURE);
      cfg->comch_state = COMCH_ERROR;
      return;
    }

    break;
  case COMCH_MSG_STATUS:
    if (msg_len != sizeof(struct comch_msg_dma_status)) {
      DOCA_LOG_ERR("Status message has bad length. Length: %u, expected: %lu",
                   msg_len, sizeof(struct comch_msg_dma_status));
      send_status_msg(comch_connection, STATUS_FAILURE);
      cfg->comch_state = COMCH_ERROR;
      return;
    }

    status = (struct comch_msg_dma_status *)recv_buffer;
    if (status->is_success == STATUS_FAILURE)
      cfg->comch_state = COMCH_ERROR;
    else
      cfg->comch_state = COMCH_COMPLETE;

    break;
  default:
    DOCA_LOG_ERR("Received bad message type. Type: %u", comch_msg->type);
    send_status_msg(comch_connection, STATUS_FAILURE);
    cfg->comch_state = COMCH_ERROR;
    return;
  }
}

/*
 * Helper to send a DMA direction notification request
 *
 * @dma_cfg [in]: dma copy configuration information
 * @comch_cfg [in]: comch object to send message across
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t send_file_direction_request(struct dma_copy_cfg *dma_cfg,
                                                struct comch_cfg *comch_cfg) {
  struct comch_msg_dma_direction dir_msg = {.type = COMCH_MSG_DIRECTION};

  if (dma_cfg->is_sender) {
    DOCA_LOG_INFO("File was found locally, it will be DMA copied to the DPU");
    dir_msg.file_in_host = true;
    dir_msg.file_size = htonq(dma_cfg->file_size);
  } else {
    DOCA_LOG_INFO(
        "File was not found locally, it will be DMA copied from the DPU");
    dir_msg.file_in_host = false;
  }

  return comch_utils_send(comch_util_get_connection(comch_cfg), &dir_msg,
                          sizeof(struct comch_msg_dma_direction));
}

doca_error_t host_start_dma_copy(struct dma_copy_cfg *dma_cfg,
                                 struct comch_cfg *comch_cfg) {
  doca_error_t result, tmp_result;
  struct timespec ts = {
      .tv_nsec = SLEEP_IN_NANOS,
  };

  dma_cfg->max_comch_buffer = comch_utils_get_max_buffer_size(comch_cfg);
  if (dma_cfg->max_comch_buffer == 0) {
    DOCA_LOG_ERR("Comch max buffer length is 0");
    return DOCA_ERROR_INVALID_VALUE;
  }

  /* Open DOCA dma device */
  result = open_dma_device(&dma_cfg->dev);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to open DOCA DMA device: %s",
                 doca_error_get_descr(result));
    return result;
  }

  result = doca_mmap_create(&dma_cfg->file_mmap);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Unable to create mmap: %s", doca_error_get_descr(result));
    goto close_device;
  }

  result = doca_mmap_add_dev(dma_cfg->file_mmap, dma_cfg->dev);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Unable to add device to mmap: %s",
                 doca_error_get_descr(result));
    goto destroy_mmap;
  }

  /*
   * If the file is local, allocate a DMA buffer and populate it now.
   * If file is remote, the buffer can be allocated in the callback when the
   * size if known.
   */
  if (dma_cfg->is_sender == true) {
    result = memory_alloc_and_populate(dma_cfg->file_mmap, dma_cfg->file_size,
                                       DOCA_ACCESS_FLAG_PCI_READ_ONLY,
                                       &dma_cfg->file_buffer);
    if (result != DOCA_SUCCESS)
      goto destroy_mmap;
  }

  dma_cfg->comch_state = COMCH_NEGOTIATING;
  result = send_file_direction_request(dma_cfg, comch_cfg);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to send direction negotiation message: %s",
                 doca_error_get_descr(result));
    goto free_buffer;
  }

  /* Wait for a signal that the DPU has completed the DMA copy */
  while (dma_cfg->comch_state == COMCH_NEGOTIATING) {
    nanosleep(&ts, &ts);
    result =
        comch_utils_progress_connection(comch_util_get_connection(comch_cfg));
    if (result != DOCA_SUCCESS) {
      DOCA_LOG_ERR("Comch connection unexpectedly dropped: %s",
                   doca_error_get_descr(result));
      goto free_buffer;
    }
  }

  if (dma_cfg->comch_state == COMCH_ERROR) {
    DOCA_LOG_ERR("Failure was detected in dma copy");
    goto free_buffer;
  }

  DOCA_LOG_INFO("Final status message was successfully received");

free_buffer:
  free(dma_cfg->file_buffer);
destroy_mmap:
  tmp_result = doca_mmap_destroy(dma_cfg->file_mmap);
  if (tmp_result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to destroy DOCA mmap: %s",
                 doca_error_get_descr(tmp_result));
    DOCA_ERROR_PROPAGATE(result, tmp_result);
  }
close_device:
  tmp_result = doca_dev_close(dma_cfg->dev);
  if (tmp_result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to close DOCA device: %s",
                 doca_error_get_descr(tmp_result));
    DOCA_ERROR_PROPAGATE(result, tmp_result);
  }
  return result;
}

/*
 * Process and respond to a DMA direction negotiation message on the DPU
 *
 * @cfg [in]: dma copy configuration information
 * @comch_connection [in]: comch connection the message was received on
 * @dir_msg [in]: the direction message received
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t dpu_process_dma_direction_and_size(
    struct dma_copy_cfg *cfg, struct doca_comch_connection *comch_connection,
    struct comch_msg_dma_direction *dir_msg) {
  struct comch_msg_dma_direction resp_dir_msg = {.type = COMCH_MSG_DIRECTION};
  doca_error_t result;

  /* Make sure file is located only on one side */
  if (cfg->is_sender && dir_msg->file_in_host == true) {
    DOCA_LOG_ERR("Error - File was found on both Host and DPU");
    return DOCA_ERROR_INVALID_VALUE;

  } else if (!cfg->is_sender) {
    if (!dir_msg->file_in_host) {
      DOCA_LOG_ERR("Error - File was not found on both Host and DPU");
      return DOCA_ERROR_INVALID_VALUE;
    }
    cfg->file_size = ntohq(dir_msg->file_size);
  }

  /* Verify file size against the HW limitation */
  if (cfg->file_size > cfg->max_dma_buf_size) {
    DOCA_LOG_ERR("DMA device maximum allowed file size in bytes is %" PRIu64
                 ", received file size is %" PRIu64 " bytes",
                 cfg->max_dma_buf_size, cfg->file_size);
    return DOCA_ERROR_INVALID_VALUE;
  }

  /* Populate and send response to host */
  if (cfg->is_sender) {
    DOCA_LOG_INFO("File was found locally, it will be DMA copied to the Host");
    resp_dir_msg.file_in_host = false;
    resp_dir_msg.file_size = htonq(cfg->file_size);
  } else {
    DOCA_LOG_INFO(
        "File was not found locally, it will be DMA copied from the Host");
    resp_dir_msg.file_in_host = true;
  }

  result = comch_utils_send(comch_connection, &resp_dir_msg,
                            sizeof(struct comch_msg_dma_direction));
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to send DMA direction message: %s",
                 doca_error_get_descr(result));
    return result;
  }

  return result;
}

/*
 * Process an export descriptor message on the DPU
 *
 * @cfg [in]: dma copy configuration information
 * @des_msg [in]: the export descriptor message received
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
dpu_process_export_descriptor(struct dma_copy_cfg *cfg,
                              struct comch_msg_dma_export_discriptor *des_msg) {
  size_t desc_len = ntohq(des_msg->export_desc_len);

  cfg->exported_mmap = malloc(desc_len);
  if (cfg->exported_mmap == NULL) {
    DOCA_LOG_ERR("Failed to allocate export descriptor memory");
    return DOCA_ERROR_NO_MEMORY;
  }

  memcpy(cfg->exported_mmap, des_msg->exported_mmap, desc_len);
  cfg->exported_mmap_len = desc_len;
  cfg->host_addr = (uint8_t *)ntohq(des_msg->host_addr);

  return DOCA_SUCCESS;
}

void dpu_recv_event_cb(struct doca_comch_event_msg_recv *event,
                       uint8_t *recv_buffer, uint32_t msg_len,
                       struct doca_comch_connection *comch_connection) {
  struct dma_copy_cfg *cfg = comch_utils_get_user_data(comch_connection);
  struct comch_msg_dma_status *status;
  struct comch_msg *comch_msg;
  doca_error_t result;

  (void)event;

  if (cfg == NULL) {
    DOCA_LOG_ERR("Cannot get configuration information");
    return;
  }

  /* Message must at least contain a message type */
  if (msg_len < sizeof(enum comch_msg_type)) {
    DOCA_LOG_ERR("Received a message that is too small. Length: %u", msg_len);
    send_status_msg(comch_connection, STATUS_FAILURE);
    cfg->comch_state = COMCH_ERROR;
    return;
  }

  /* All messages should take the format of a comch_msg struct */
  comch_msg = (struct comch_msg *)recv_buffer;

  /*
   * The first message a DPU receives should be a direction negotiation request.
   * This should be responded to as an ack or containing the file information if
   * file is local to the DPU. The host will respond will memory information the
   * DPU can read from or write to. At this stage the DMA can be triggered.
   */

  switch (comch_msg->type) {
  case COMCH_MSG_DIRECTION:
    if (msg_len != sizeof(struct comch_msg_dma_direction)) {
      DOCA_LOG_ERR(
          "Direction message has bad length. Length: %u, expected: %lu",
          msg_len, sizeof(struct comch_msg_dma_direction));
      send_status_msg(comch_connection, STATUS_FAILURE);
      cfg->comch_state = COMCH_ERROR;
      return;
    }

    result = dpu_process_dma_direction_and_size(
        cfg, comch_connection, (struct comch_msg_dma_direction *)recv_buffer);
    if (result != DOCA_SUCCESS) {
      send_status_msg(comch_connection, STATUS_FAILURE);
      cfg->comch_state = COMCH_ERROR;
      return;
    }
    break;
  case COMCH_MSG_EXPORT_DESCRIPTOR:
    if (msg_len <= sizeof(struct comch_msg_dma_export_discriptor)) {
      DOCA_LOG_ERR("Direction message has bad length. Length: %u, expected at "
                   "least: %lu",
                   msg_len, sizeof(struct comch_msg_dma_direction));
      send_status_msg(comch_connection, STATUS_FAILURE);
      cfg->comch_state = COMCH_ERROR;
      return;
    }

    result = dpu_process_export_descriptor(
        cfg, (struct comch_msg_dma_export_discriptor *)recv_buffer);
    if (result != DOCA_SUCCESS) {
      send_status_msg(comch_connection, STATUS_FAILURE);
      cfg->comch_state = COMCH_ERROR;
      return;
    }

    /* All information is successfully received to do DMA */
    cfg->comch_state = COMCH_COMPLETE;
    break;
  case COMCH_MSG_STATUS:
    if (msg_len != sizeof(struct comch_msg_dma_status)) {
      DOCA_LOG_ERR("Status message has bad length. Length: %u, expected: %lu",
                   msg_len, sizeof(struct comch_msg_dma_status));
      send_status_msg(comch_connection, STATUS_FAILURE);
      cfg->comch_state = COMCH_ERROR;
      return;
    }

    status = (struct comch_msg_dma_status *)recv_buffer;
    if (status->is_success == STATUS_FAILURE)
      cfg->comch_state = COMCH_ERROR;

    break;
  default:
    DOCA_LOG_ERR("Received bad message type. Type: %u", comch_msg->type);
    send_status_msg(comch_connection, STATUS_FAILURE);
    cfg->comch_state = COMCH_ERROR;
    return;
  }
}

void *dpu_start_dma_copy_thread(void *arg) {
  doca_error_t result, tmp_result;
  struct thread_args *targs = (struct thread_args *)arg;
  struct dma_copy_cfg *dma_cfg = targs->dma_cfg;
  struct comch_cfg *comch_cfg = targs->comch_cfg;
  struct dma_copy_resources *resources = targs->resources;
  int thread_idx = targs->thread_idx;
  int ops = targs->ops_per_thread;
  double *setup_lt = targs->setup_latency;
  double *dma_task_lt = targs->dma_task_latency;
  double *cleanup_lt = targs->cleanup_latency;
  struct timespec total_setup_lt = {0};
  struct timespec total_dma_task_lt = {0};
  struct timespec total_cleanup_lt = {0};
  DOCA_LOG_INFO("Starging dma copy thread %d ...", thread_idx);

  struct program_core_objects *state = resources->state;
  struct doca_ctx *ctx = state->ctx[thread_idx];
  struct doca_buf_inventory *buf_inv = state->buf_inv[thread_idx];
  struct doca_pe *pe = state->pe[thread_idx];
  struct doca_mmap *remote_mmap = targs->remote_mmap;

  union doca_data ctx_user_data = {0};
  size_t num_remaining_tasks;

  get_dma_max_buf_size(resources, &dma_cfg->max_dma_buf_size);
  ctx_user_data.ptr = &num_remaining_tasks;
  doca_ctx_set_user_data(ctx, ctx_user_data);
  pthread_mutex_t *mutex = targs->mutex;

  doca_ctx_start(ctx);

  struct doca_buf *remote_doca_buf = NULL;
  struct doca_buf *local_doca_buf = NULL;

  struct timespec setup_start = {0};
  struct timespec dma_task_start = {0};
  struct timespec cleanup_start = {0};
  struct timespec cleanup_end = {0};

  for (uint64_t i = 0; i < ops; i++) {
    DOCA_LOG_INFO("Thread %d starging loop %lu ", thread_idx, i);
    num_remaining_tasks = 1;
    remote_doca_buf = NULL;
    local_doca_buf = NULL;

    clock_gettime(CLOCK_REALTIME, &setup_start);
    pthread_mutex_lock(mutex);
    /* Construct DOCA buffer for remote (Host) address range */
    result = doca_buf_inventory_buf_get_by_addr(
        buf_inv, remote_mmap, dma_cfg->host_addr, dma_cfg->file_size,
        &remote_doca_buf);
    if (result != DOCA_SUCCESS) {
      DOCA_LOG_ERR("Unable to acquire DOCA remote buffer: %s",
                   doca_error_get_descr(result));
      send_status_msg(comch_util_get_connection(comch_cfg), STATUS_FAILURE);
      goto destroy_thread_resources;
    }

    /* Construct DOCA buffer for local (DPU) address range */
    result = doca_buf_inventory_buf_get_by_addr(
        buf_inv, state->dst_mmap, dma_cfg->file_buffer, dma_cfg->file_size,
        &local_doca_buf);
    if (result != DOCA_SUCCESS) {
      DOCA_LOG_ERR("Unable to acquire DOCA local buffer: %s",
                   doca_error_get_descr(result));
      send_status_msg(comch_util_get_connection(comch_cfg), STATUS_FAILURE);
      tmp_result = doca_buf_dec_refcount(remote_doca_buf, NULL);
      if (tmp_result != DOCA_SUCCESS) {
        DOCA_ERROR_PROPAGATE(result, tmp_result);
        DOCA_LOG_ERR("Failed to destroy remote DOCA buffer: %s",
                     doca_error_get_descr(tmp_result));
      }
      goto destroy_thread_resources;
    }
    pthread_mutex_unlock(mutex);
    clock_gettime(CLOCK_REALTIME, &dma_task_start);

    /* Submit DMA task into the progress engine and wait until task completion
     */
    result = dpu_submit_dma_task(dma_cfg, resources, dma_cfg->file_size,
                                 dma_cfg->file_buffer, local_doca_buf,
                                 remote_doca_buf, &num_remaining_tasks,
                                 thread_idx, NULL, NULL);

    if (result != DOCA_SUCCESS) {
      send_status_msg(comch_util_get_connection(comch_cfg), STATUS_FAILURE);
      tmp_result = doca_buf_dec_refcount(local_doca_buf, NULL);
      if (tmp_result != DOCA_SUCCESS) {
        DOCA_ERROR_PROPAGATE(result, tmp_result);
        DOCA_LOG_ERR("Failed to destroy local DOCA buffer: %s",
                     doca_error_get_descr(tmp_result));
      }
      tmp_result = doca_buf_dec_refcount(remote_doca_buf, NULL);
      if (tmp_result != DOCA_SUCCESS) {
        DOCA_ERROR_PROPAGATE(result, tmp_result);
        DOCA_LOG_ERR("Failed to destroy remote DOCA buffer: %s",
                     doca_error_get_descr(tmp_result));
      }
      goto destroy_thread_resources;
    }

    clock_gettime(CLOCK_REALTIME, &cleanup_start);
    pthread_mutex_lock(mutex);
    tmp_result = doca_buf_dec_refcount(local_doca_buf, NULL);
    if (tmp_result != DOCA_SUCCESS) {
      DOCA_ERROR_PROPAGATE(result, tmp_result);
      DOCA_LOG_ERR("Failed to destroy local DOCA buffer: %s",
                   doca_error_get_descr(tmp_result));
    }

    tmp_result = doca_buf_dec_refcount(remote_doca_buf, NULL);
    if (tmp_result != DOCA_SUCCESS) {
      DOCA_ERROR_PROPAGATE(result, tmp_result);
      DOCA_LOG_ERR("Failed to destroy local DOCA buffer: %s",
                   doca_error_get_descr(tmp_result));
    }
    pthread_mutex_unlock(mutex);
    clock_gettime(CLOCK_REALTIME, &cleanup_end);

    timespec_add(&total_setup_lt, get_duration(setup_start, dma_task_start));
    timespec_add(&total_dma_task_lt,
                 get_duration(dma_task_start, cleanup_start));
    timespec_add(&total_cleanup_lt, get_duration(cleanup_start, cleanup_end));
  }

  *setup_lt = timespec_to_us(total_setup_lt) / (double)ops;
  *dma_task_lt = timespec_to_us(total_dma_task_lt) / (double)ops;
  *cleanup_lt = timespec_to_us(total_cleanup_lt) / (double)ops;

destroy_thread_resources:

  return (void *)result;
}

doca_error_t dpu_start_dma_copy(struct dma_copy_cfg *dma_cfg,
                                struct comch_cfg *comch_cfg) {
  struct timespec start, end;
  struct dma_copy_resources resources = {0};
  struct doca_mmap *remote_mmap = NULL;
  doca_error_t result;

  result = allocate_dma_copy_resources(&resources, dma_cfg->num_threads);
  if (result != DOCA_SUCCESS) {
    DOCA_LOG_ERR("Failed to allocate dma copy resources. ");
    return DOCA_ERROR_UNEXPECTED;
  }

  result = get_dma_max_buf_size(&resources, &dma_cfg->max_dma_buf_size);

  uint32_t access_flags = dma_cfg->is_sender
                              ? DOCA_ACCESS_FLAG_LOCAL_READ_ONLY
                              : DOCA_ACCESS_FLAG_LOCAL_READ_WRITE;

  struct timespec ts = {
      .tv_nsec = SLEEP_IN_NANOS,
  };
  while (dma_cfg->comch_state == COMCH_NEGOTIATING) {
    nanosleep(&ts, &ts);
    comch_utils_progress_connection(comch_util_get_connection(comch_cfg));
  }

  memory_alloc_and_populate(resources.state->dst_mmap, dma_cfg->file_size,
                            access_flags, &dma_cfg->file_buffer);

  doca_mmap_create_from_export(NULL, (const void *)dma_cfg->exported_mmap,
                               dma_cfg->exported_mmap_len, resources.state->dev,
                               &remote_mmap);

  int ops_per_thread = dma_cfg->stat.ops / dma_cfg->num_threads;
  pthread_mutex_t mutex;
  pthread_mutex_init(&mutex, NULL);

  pthread_t *threads = calloc(dma_cfg->num_threads, sizeof(pthread_t));
  double *setup_latency = calloc(dma_cfg->num_threads, sizeof(double));
  double *dma_task_latency = calloc(dma_cfg->num_threads, sizeof(double));
  double *cleanup_latency = calloc(dma_cfg->num_threads, sizeof(double));
  struct thread_args *targs =
      calloc(dma_cfg->num_threads, sizeof(struct thread_args));
  for (int t = 0; t < dma_cfg->num_threads; t++) {
    targs[t].thread_idx = t;
    targs[t].dma_cfg = dma_cfg;
    targs[t].comch_cfg = comch_cfg;
    targs[t].resources = &resources;
    targs[t].remote_mmap = remote_mmap;
    targs[t].ops_per_thread = ops_per_thread;
    targs[t].mutex = &mutex;
    targs[t].setup_latency = setup_latency + t;
    targs[t].dma_task_latency = dma_task_latency + t;
    targs[t].cleanup_latency = cleanup_latency + t;
  }
  clock_gettime(CLOCK_REALTIME, &start);
  for (int i = 0; i < dma_cfg->num_threads; i++) {
    pthread_create(&threads[i], NULL, dpu_start_dma_copy_thread,
                   (void *)&targs[i]);
    DOCA_LOG_INFO("Created one thread.");
  }

  for (int i = 0; i < dma_cfg->num_threads; i++) {
    void *ret;
    pthread_join(threads[i], &ret);
    doca_error_t tmp_result = (doca_error_t)ret;
    if (tmp_result != DOCA_SUCCESS) {
      DOCA_LOG_ERR("Thread %d failed", i);
    } else {
      DOCA_LOG_INFO("Joined thread %d ", i);
    }
  }
  clock_gettime(CLOCK_REALTIME, &end);
  dma_cfg->stat.total_time = get_duration(start, end);
  dma_cfg->stat.avg_setup_lt = average(setup_latency, dma_cfg->num_threads);
  dma_cfg->stat.avg_task_lt = average(dma_task_latency, dma_cfg->num_threads);
  dma_cfg->stat.avg_cleanup_lt = average(cleanup_latency, dma_cfg->num_threads);
  write_statistics_to_file(&dma_cfg->stat, "result.json");
  free(setup_latency);
  free(dma_task_latency);
  free(cleanup_latency);

  pthread_mutex_destroy(&mutex);

  /* Clean up */
  doca_mmap_stop(remote_mmap);
  doca_mmap_destroy(remote_mmap);
  if (dma_cfg->exported_mmap != NULL) {
    free(dma_cfg->exported_mmap);
  }
  doca_mmap_stop(resources.state->dst_mmap);
  doca_mmap_destroy(resources.state->dst_mmap);

  for (int t = 0; t < dma_cfg->num_threads; t++) {
    struct doca_pe *pe = resources.state->pe[t];
    struct doca_ctx *ctx = resources.state->ctx[t];
    struct doca_buf_inventory *buf_inv = resources.state->buf_inv[t];
    request_stop_ctx(pe, ctx);

    if (resources.dma_ctx[t] != NULL) {
      doca_dma_destroy(resources.dma_ctx[t]);
    }

    if (buf_inv != NULL) {
      doca_buf_inventory_stop(buf_inv);
      doca_buf_inventory_destroy(buf_inv);
    }
    if (pe != NULL) {
      doca_pe_destroy(pe);
    }
    ctx = NULL;
  }

  free(threads);
  doca_dev_close(resources.state->dev);
  free(dma_cfg->file_buffer);
  free(resources.state->buf_inv);
  free(resources.state->pe);
  free(resources.state->ctx);
  free(resources.dma_ctx);
  free(resources.state);

  send_status_msg(comch_util_get_connection(comch_cfg), STATUS_SUCCESS);
  return result;
}
