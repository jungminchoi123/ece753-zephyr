/**
 * @file ft_stack_overflow.h
 * @brief Stack Overflow Protection Module
 * @author Jack Ostapeic
 * 
 * This header provides stack overflow detection and prevention capabilities
 * for the Zephyr Fault Tolerance Framework.
 */

#ifndef ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_STACK_OVERFLOW_H_
#define ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_STACK_OVERFLOW_H_

#include <zephyr/kernel.h>
#include <zephyr/fault_tolerance/ft_core.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_FT_ENABLE_STACK_OVERFLOW_PROTECTION

/** @brief Stack overflow monitoring data */
struct ft_stack_monitor {
    k_tid_t thread_id;
    const char *thread_name;
    size_t stack_size;
    size_t current_usage;
    size_t max_usage;
    uint32_t usage_percent;
    uint64_t last_check;
    bool is_suspended;
    uint32_t suspension_count;
};

/** @brief Stack overflow statistics */
struct ft_stack_overflow_stats {
    uint32_t threads_monitored;
    uint32_t checks_performed;
    uint32_t violations_detected;
    uint32_t threads_suspended;
    uint32_t threads_recovered;
    uint32_t false_positives;
};

/**
 * @brief Initialize stack overflow protection module
 * 
 * @return 0 on success, negative error code on failure
 */
int ft_stack_overflow_init(void);

/**
 * @brief Shutdown stack overflow protection module
 * 
 * @return 0 on success, negative error code on failure
 */
int ft_stack_overflow_shutdown(void);

/**
 * @brief Register a thread for stack monitoring
 * 
 * @param thread_id Thread to monitor
 * @return 0 on success, negative error code on failure
 */
int ft_stack_overflow_register_thread(k_tid_t thread_id, const char *name);

/**
 * @brief Unregister a thread from stack monitoring
 * 
 * @param thread_id Thread to unregister
 * @return 0 on success, negative error code on failure
 */
int ft_stack_overflow_unregister_thread(k_tid_t thread_id);

/**
 * @brief Get stack monitoring data for a thread
 * 
 * @param thread_id Thread to query
 * @param monitor Output buffer for monitoring data
 * @return 0 on success, negative error code on failure
 */
int ft_stack_overflow_get_monitor_data(k_tid_t thread_id, struct ft_stack_monitor *monitor);

/**
 * @brief Get stack overflow statistics
 * 
 * @param stats Output buffer for statistics
 * @return 0 on success, negative error code on failure
 */
int ft_stack_overflow_get_stats(struct ft_stack_overflow_stats *stats);

/**
 * @brief Force a stack check for all monitored threads
 * 
 * @return Number of threads checked
 */
uint32_t ft_stack_overflow_check_all_threads(void);

/**
 * @brief Suspend a thread due to stack overflow risk
 * 
 * @param thread_id Thread to suspend
 * @return 0 on success, negative error code on failure
 */
int ft_stack_overflow_suspend_thread(k_tid_t thread_id);

/**
 * @brief Resume a previously suspended thread
 * 
 * @param thread_id Thread to resume
 * @return 0 on success, negative error code on failure
 */
int ft_stack_overflow_resume_thread(k_tid_t thread_id);

#else /* CONFIG_FT_ENABLE_STACK_OVERFLOW_PROTECTION */

/* Stub implementations when module is disabled */
static inline int ft_stack_overflow_init(void) { return 0; }
static inline int ft_stack_overflow_shutdown(void) { return 0; }
static inline int ft_stack_overflow_register_thread(k_tid_t thread_id) { return -ENOTSUP; }
static inline int ft_stack_overflow_unregister_thread(k_tid_t thread_id) { return -ENOTSUP; }
static inline uint32_t ft_stack_overflow_check_all_threads(void) { return 0; }

#endif /* CONFIG_FT_ENABLE_STACK_OVERFLOW_PROTECTION */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_STACK_OVERFLOW_H_ */