/**
 * @file ft_race_condition.h
 * @brief Race Condition Detection and Prevention Module
 * @author Jack Ostapeic
 * 
 * This header provides race condition detection and prevention capabilities
 * for the Zephyr Fault Tolerance Framework using Lamport logical clocks.
 */

#ifndef ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_RACE_CONDITION_H_
#define ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_RACE_CONDITION_H_

#include <zephyr/kernel.h>
#include <zephyr/fault_tolerance/ft_core.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_FT_ENABLE_RACE_CONDITION_DETECTION

/** @brief Memory access information */
struct ft_memory_access {
    void *address;
    size_t size;
    k_tid_t thread_id;
    uint64_t timestamp;
    uint32_t lamport_clock;
    enum { READ_ACCESS, WRITE_ACCESS } type;
    const char *function_name;
    const char *file_name;
    int line_number;
};

/** @brief Shared resource protection */
struct ft_shared_resource {
    void *base_address;
    size_t size;
    const char *name;
    struct k_mutex protection_mutex;
    struct ft_memory_access recent_accesses[CONFIG_FT_RACE_CONDITION_ACCESS_HISTORY_SIZE];
    uint32_t access_count;
    uint32_t write_count;
    bool is_protected;
    uint64_t last_access_time;
};

/** @brief Thread state information */
struct ft_thread_state {
    k_tid_t thread_id;
    const char *thread_name;
    uint32_t lamport_clock;
    uint32_t total_accesses;
    uint32_t race_conditions_detected;
    uint32_t successful_synchronizations;
    bool is_active;
};

/** @brief Race condition detection result */
struct ft_race_detection {
    struct ft_memory_access access1;
    struct ft_memory_access access2;
    uint64_t time_difference;
    bool is_true_race;
    enum { CONCURRENT_WRITE, READ_WRITE_CONFLICT, WRITE_WRITE_CONFLICT } conflict_type;
};

/** @brief Race condition prevention statistics */
struct ft_race_stats {
    uint32_t total_accesses_monitored;
    uint32_t concurrent_accesses_detected;
    uint32_t race_conditions_detected;
    uint32_t race_conditions_prevented;
    uint32_t synchronizations_performed;
    uint32_t false_positives;
    uint32_t protected_resources;
    uint32_t active_threads;
};

/**
 * @brief Initialize race condition detection module
 * 
 * @return 0 on success, negative error code on failure
 */
int ft_race_init(void);

/**
 * @brief Shutdown race condition detection module
 * 
 * @return 0 on success, negative error code on failure
 */
int ft_race_shutdown(void);

/**
 * @brief Register shared resource for race condition monitoring
 * 
 * @param base_address Base address of shared resource
 * @param size Size of the shared resource
 * @param name Human-readable name for the resource
 * @return 0 on success, negative error code on failure
 */
int ft_race_register_resource(void *base_address, size_t size, const char *name);

/**
 * @brief Unregister shared resource
 * 
 * @param base_address Base address of resource to unregister
 * @return 0 on success, negative error code on failure
 */
int ft_race_unregister_resource(void *base_address);

/**
 * @brief Register thread for race condition monitoring
 * 
 * @param thread_id Thread to monitor
 * @param name Human-readable thread name
 * @return 0 on success, negative error code on failure
 */
int ft_race_register_thread(k_tid_t thread_id, const char *name);

/**
 * @brief Report memory read access
 * 
 * @param address Memory address being read
 * @param size Number of bytes being read
 * @param function Function name where access occurs
 * @param file File name where access occurs
 * @param line Line number where access occurs
 * @return 0 on success, negative error code on failure
 */
int ft_race_report_read(void *address, size_t size, const char *function, 
                       const char *file, int line);

/**
 * @brief Report memory write access
 * 
 * @param address Memory address being written
 * @param size Number of bytes being written
 * @param function Function name where access occurs
 * @param file File name where access occurs
 * @param line Line number where access occurs
 * @return 0 on success, negative error code on failure
 */
int ft_race_report_write(void *address, size_t size, const char *function,
                        const char *file, int line);

/**
 * @brief Check for concurrent access patterns
 * 
 * @param detection Output buffer for detection results
 * @return 1 if race condition detected, 0 if none, negative on error
 */
int ft_race_check_concurrent_access(struct ft_race_detection *detection);

/**
 * @brief Synchronize access to shared resource
 * 
 * @param address Base address of resource to synchronize
 * @param timeout_ms Maximum time to wait for synchronization
 * @return 0 on success, negative error code on failure
 */
int ft_race_synchronize_access(void *address, uint32_t timeout_ms);

/**
 * @brief Update Lamport logical clock for current thread
 * 
 * @param received_clock Clock value received from another thread
 * @return New clock value for current thread
 */
uint32_t ft_race_update_lamport_clock(uint32_t received_clock);

/**
 * @brief Get current thread's Lamport clock
 * 
 * @return Current Lamport clock value
 */
uint32_t ft_race_get_lamport_clock(void);

/**
 * @brief Get race condition statistics
 * 
 * @param stats Output buffer for statistics
 * @return 0 on success, negative error code on failure
 */
int ft_race_get_stats(struct ft_race_stats *stats);

/**
 * @brief Force race condition detection check
 * 
 * @return Number of race conditions detected
 */
uint32_t ft_race_force_check(void);

/**
 * @brief Enable/disable race condition prevention
 * 
 * @param enable True to enable prevention, false to disable
 * @return 0 on success, negative error code on failure
 */
int ft_race_set_prevention_mode(bool enable);

/* Convenience macros for instrumented access */
#define FT_RACE_READ(addr, size) \
    ft_race_report_read((void *)(addr), size, __func__, __FILE__, __LINE__)

#define FT_RACE_WRITE(addr, size) \
    ft_race_report_write((void *)(addr), size, __func__, __FILE__, __LINE__)

#else /* CONFIG_FT_ENABLE_RACE_CONDITION_DETECTION */

/* Stub implementations when module is disabled */
static inline int ft_race_init(void) { return 0; }
static inline int ft_race_shutdown(void) { return 0; }
static inline int ft_race_register_resource(void *base_address, size_t size, const char *name) { return -ENOTSUP; }
static inline int ft_race_register_thread(k_tid_t thread_id, const char *name) { return -ENOTSUP; }
static inline uint32_t ft_race_force_check(void) { return 0; }
static inline uint32_t ft_race_get_lamport_clock(void) { return 0; }

/* Stub macros when module is disabled */
#define FT_RACE_READ(addr, size) do { } while (0)
#define FT_RACE_WRITE(addr, size) do { } while (0)

#endif /* CONFIG_FT_ENABLE_RACE_CONDITION_DETECTION */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_RACE_CONDITION_H_ */