/**
 * @file ft_thread_recovery.h
 * @brief Thread Recovery Module for Fault Tolerance Framework
 * @author Jack Ostapeic
 */

#ifndef ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_THREAD_RECOVERY_H_
#define ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_THREAD_RECOVERY_H_

#include <zephyr/fault_tolerance/ft_core.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Thread recovery configuration */
struct ft_thread_recovery_info {
    k_thread_entry_t entry_point;      /**< Original thread entry point */
    void *p1, *p2, *p3;                 /**< Original thread parameters */
    size_t stack_size;                  /**< Stack size for recreated thread */
    int priority;                       /**< Thread priority */
    uint32_t options;                   /**< Thread creation options */
    k_timeout_t delay;                  /**< Delay before starting new thread */
    const char *name;                   /**< Thread name */
    uint32_t max_restarts;              /**< Maximum restart attempts */
    uint32_t restart_count;             /**< Current restart count */
};

/** @brief Thread registry entry */
struct ft_thread_entry {
    sys_snode_t node;                   /**< List node */
    k_tid_t thread_id;                  /**< Thread ID */
    struct ft_thread_recovery_info info; /**< Recovery information */
    bool auto_recovery;                 /**< Enable automatic recovery */
};

/**
 * @brief Initialize thread recovery module
 * 
 * @return 0 on success, negative error code on failure
 */
int ft_thread_recovery_init(void);

/**
 * @brief Register a thread for fault tolerance monitoring
 * 
 * @param thread_id Thread to monitor
 * @param entry_point Thread entry point
 * @param p1 Thread parameter 1
 * @param p2 Thread parameter 2  
 * @param p3 Thread parameter 3
 * @param stack_size Stack size for recovery
 * @param priority Thread priority
 * @param options Thread options
 * @param name Thread name
 * @return 0 on success, negative error code on failure
 */
int ft_thread_register(k_tid_t thread_id, k_thread_entry_t entry_point,
                      void *p1, void *p2, void *p3,
                      size_t stack_size, int priority, uint32_t options,
                      const char *name);

/**
 * @brief Unregister a thread from fault tolerance monitoring
 * 
 * @param thread_id Thread to unregister
 * @return 0 on success, negative error code on failure
 */
int ft_thread_unregister(k_tid_t thread_id);

/**
 * @brief Recover a faulted thread
 * 
 * @param thread_id Faulted thread ID
 * @param new_thread_id Pointer to store new thread ID
 * @return 0 on success, negative error code on failure
 */
int ft_thread_recover(k_tid_t thread_id, k_tid_t *new_thread_id);

/**
 * @brief Set thread recovery configuration
 * 
 * @param thread_id Thread ID
 * @param auto_recovery Enable automatic recovery
 * @param max_restarts Maximum restart attempts
 * @return 0 on success, negative error code on failure
 */
int ft_thread_set_recovery_config(k_tid_t thread_id, bool auto_recovery, 
                                  uint32_t max_restarts);

/**
 * @brief Get thread recovery statistics
 * 
 * @param thread_id Thread ID
 * @param restart_count Pointer to store restart count
 * @return 0 on success, negative error code on failure
 */
int ft_thread_get_stats(k_tid_t thread_id, uint32_t *restart_count);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_THREAD_RECOVERY_H_ */