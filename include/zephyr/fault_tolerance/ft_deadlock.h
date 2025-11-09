/**
 * @file ft_deadlock.h
 * @brief Deadlock Detection and Recovery Module
 * @author Jack Ostapeic
 * 
 * This header provides deadlock detection and recovery capabilities
 * for the Zephyr Fault Tolerance Framework.
 */

#ifndef ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_DEADLOCK_H_
#define ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_DEADLOCK_H_

#include <zephyr/kernel.h>
#include <zephyr/fault_tolerance/ft_core.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_FT_ENABLE_DEADLOCK_DETECTION

/** @brief Resource tracking information */
struct ft_resource_info {
    struct k_mutex *mutex;
    const char *name;
    k_tid_t owner;
    k_tid_t waiting_threads[CONFIG_MP_MAX_NUM_CPUS * 4];
    uint32_t wait_count;
    int64_t acquire_time;
    bool in_use;
};

/** @brief Thread dependency information */
struct ft_thread_dependency {
    k_tid_t thread_id;
    const char *thread_name;
    struct k_mutex *owned_resources[8];
    struct k_mutex *waiting_for;
    int64_t wait_start_time;
    bool is_blocked;
    bool is_active;
    uint32_t priority;
};

/** @brief Deadlock pattern information */
struct ft_deadlock_pattern {
    k_tid_t thread_1;
    k_tid_t thread_2;
    struct k_mutex *resource_1;
    struct k_mutex *resource_2;
    int64_t detection_time;
    enum { CIRCULAR_WAIT, TIMEOUT_BASED, PRIORITY_INVERSION } type;
    bool resolved;
};

/** @brief Deadlock detection statistics */
struct ft_deadlock_stats {
    uint32_t resources_monitored;
    uint32_t threads_monitored;
    uint32_t deadlocks_detected;
    uint32_t deadlocks_resolved;
    uint32_t resource_preemptions;
    uint32_t thread_suspensions;
    uint32_t false_positives;
};

/**
 * @brief Initialize deadlock detection module
 * 
 * @return 0 on success, negative error code on failure
 */
int ft_deadlock_init(void);

/**
 * @brief Shutdown deadlock detection module
 * 
 * @return 0 on success, negative error code on failure
 */
int ft_deadlock_shutdown(void);

/**
 * @brief Register a resource for deadlock monitoring
 * 
 * @param mutex Mutex to monitor
 * @param name Human-readable resource name
 * @return 0 on success, negative error code on failure
 */
int ft_deadlock_register_resource(struct k_mutex *mutex, const char *name);

/**
 * @brief Unregister a resource from deadlock monitoring
 * 
 * @param mutex Mutex to unregister
 * @return 0 on success, negative error code on failure
 */
int ft_deadlock_unregister_resource(struct k_mutex *mutex);

/**
 * @brief Register a thread for deadlock monitoring
 * 
 * @param thread_id Thread to monitor
 * @param name Human-readable thread name
 * @return 0 on success, negative error code on failure
 */
int ft_deadlock_register_thread(k_tid_t thread_id, const char *name);

/**
 * @brief Unregister a thread from deadlock monitoring
 * 
 * @param thread_id Thread to unregister
 * @return 0 on success, negative error code on failure
 */
int ft_deadlock_unregister_thread(k_tid_t thread_id);

/**
 * @brief Report resource acquisition
 * 
 * @param thread_id Thread acquiring resource
 * @param mutex Resource being acquired
 * @return 0 on success, negative error code on failure
 */
int ft_deadlock_report_acquire(k_tid_t thread_id, struct k_mutex *mutex);

/**
 * @brief Report resource release
 * 
 * @param thread_id Thread releasing resource
 * @param mutex Resource being released
 * @return 0 on success, negative error code on failure
 */
int ft_deadlock_report_release(k_tid_t thread_id, struct k_mutex *mutex);

/**
 * @brief Report thread waiting for resource
 * 
 * @param thread_id Thread waiting
 * @param mutex Resource being waited for
 * @return 0 on success, negative error code on failure
 */
int ft_deadlock_report_wait(k_tid_t thread_id, struct k_mutex *mutex);

/**
 * @brief Check for deadlocks using circular dependency analysis
 * 
 * @return Number of deadlocks detected
 */
uint32_t ft_deadlock_check_circular_dependencies(void);

/**
 * @brief Check for timeout-based deadlocks
 * 
 * @return Number of timeout deadlocks detected
 */
uint32_t ft_deadlock_check_timeouts(void);

/**
 * @brief Resolve deadlock through resource preemption
 * 
 * @param pattern Deadlock pattern to resolve
 * @return 0 on success, negative error code on failure
 */
int ft_deadlock_resolve_preemption(struct ft_deadlock_pattern *pattern);

/**
 * @brief Get deadlock detection statistics
 * 
 * @param stats Output buffer for statistics
 * @return 0 on success, negative error code on failure
 */
int ft_deadlock_get_stats(struct ft_deadlock_stats *stats);

/**
 * @brief Force deadlock detection check
 * 
 * @return Number of deadlocks found
 */
uint32_t ft_deadlock_force_check(void);

#else /* CONFIG_FT_ENABLE_DEADLOCK_DETECTION */

/* Stub implementations when module is disabled */
static inline int ft_deadlock_init(void) { return 0; }
static inline int ft_deadlock_shutdown(void) { return 0; }
static inline int ft_deadlock_register_resource(struct k_mutex *mutex, const char *name) { return -ENOTSUP; }
static inline int ft_deadlock_register_thread(k_tid_t thread_id, const char *name) { return -ENOTSUP; }
static inline uint32_t ft_deadlock_force_check(void) { return 0; }

#endif /* CONFIG_FT_ENABLE_DEADLOCK_DETECTION */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_DEADLOCK_H_ */