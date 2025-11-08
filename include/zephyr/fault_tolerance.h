/**
 * @file
 * @author Jack Ostapeic, MS ECE student at UW-Madison
 * @brief Enhanced fault tolerance framework for Zephyr RTOS
 *
 * This header defines advanced fault types and structures for a comprehensive
 * fault tolerance system, enabling robust fault detection, recovery, and logging mechanisms.
 */

#ifndef ZEPHYR_INCLUDE_FAULT_TOLERANCE_H
#define ZEPHYR_INCLUDE_FAULT_TOLERANCE_H

#include <zephyr/kernel.h>
#include <zephyr/fatal.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FT_FAULT_NUM_TYPES 1

// enhanced fault types for detailed fault classification
enum ft_fault_type {
    FT_FAULT_STACK_OVERFLOW = 0x1000,       // stack overflow detected
    // FT_FAULT_HEAP_CORRUPTION,               // heap corruption detected
    // FT_FAULT_MEMORY_LEAK,                   // memory leak detected
    // FT_FAULT_DEADLOCK,                      // deadlock detected
    // FT_FAULT_RACE_CONDITION,                // race condition detected
    // FT_FAULT_RESOURCE_EXHAUSTION,           // resource exhaustion detected
    // FT_FAULT_PERIPHERAL_FAILURE,            // peripheral failure detected
    // FT_FAULT_TIMING_VIOLATION,              // timing violation detected
    // FT_FAULT_DATA_CORRUPTION,               // data corruption detected
    // FT_FAULT_COMM_FAILURE,                  // communication failure detected
    // FT_FAULT_POWER_MGMT,                    // power management fault detected
    // FT_FAULT_CONFIG_ERROR,                  // configuration error detected
    // FT_FAULT_UNKNOWN = 0x1FFF               // unknown fault type
};

// fault severity levels
enum ft_fault_severity {
    FT_SEVERITY_LOW = 0,                    // system can continue with degraded performance
    FT_SEVERITY_MEDIUM,                     // system can continue but needs attention
    FT_SEVERITY_HIGH,                       // system functionality significantly impaired
    FT_SEVERITY_CRITICAL                    // system cannot continue safely
};

// recovery action steps for fault handling
enum ft_recovery_action {
    FT_RECOVERY_NONE = 0,                   // no recovery action
    FT_RECOVERY_RESTART_THREAD,             // restart the affected thread
    FT_RECOVERY_RESET_PERIPHERAL,           // reset the affected peripheral
    FT_RECOVERY_SAFE_MODE,                  // fallback to safe mode
    FT_RECOVERY_SYSTEM_RESTART,             // restart the entire system
    FT_RECOVERY_EMERGENCY_SHUTDOWN,         // perform emergency shutdown
    FT_RECOVERY_CUSTOM = 0xFF               // custom recovery action
};

// fault context information
struct ft_fault_context {
    enum ft_fault_type fault_type;          // type of fault
    enum ft_fault_severity severity;        // severity level of the fault
    k_tid_t thread_id;                      // thread identifier where fault occurred
    int64_t timestamp;                      // timestamp of fault occurrence in ticks
    uintptr_t pc;                           // program counter at fault occurrence
    uintptr_t sp;                           // stack pointer at fault occurrence
    uintptr_t context_data[4];              // additional context data
    const char *description;                // human-readable fault description
    const char *file;                       // file name where the fault occured
    uint32_t line;                          // line number where the fault occured
};

// fault statistics
struct ft_fault_stats {
    uint32_t total_faults;                          // total number of faults detected
    uint32_t fault_counts[FT_FAULT_NUM_TYPES];      // counts per fault type
    uint32_t recovery_attempts;                     // number of recovery attempts
    uint32_t recovery_successes;                    // number of successful recoveries
    uint32_t recovery_failures;                     // number of failed recoveries
    int64_t  first_fault_time;                      // system uptime at first fault
    int64_t  last_fault_time;                       // system uptime at last fault
    uint32_t mtbf_ms;                               // mean time between faults in milliseconds
};  

// memory usage statistics
struct ft_memory_stats {
    size_t heap_total;                      // total heap size
    size_t heap_used;                       // used heap size
    size_t heap_free;                       // free heap size
    uint32_t tracked_allocations;           // number of tracked memory allocations
    size_t tracked_bytes;                   // total bytes in tracked allocations
};

struct ft_system_stats {
    uint32_t total_faults;                  // total number of faults detected 
    uint32_t critical_faults;               // number of critical faults detected
    uint32_t recoveries_successful;         // number of successful recoveries
    uint32_t recoveries_failed;             // number of failed recoveries
    int64_t  init_time;                     // system uptime at initialization
    int64_t  last_fault_time;               // system uptime at last fault
    uint32_t monitored_threads;             // number of monitored threads
    uint32_t active_handlers;               // number of active fault handlers
};

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
// Type definitions for global use
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

/**
    * @brief Fault handler callback type
    *
    * @param ctx Fault context information
    * 
    * @return recovery action to be taken
 */
typedef enum ft_recovery_action (*ft_fault_handler_t)(const struct ft_fault_context *ctx);

/**
    * @brief Recovery callback type
    *
    * @param ctx Fault context information 
    * @param action Recovery action being performed
    *
    * @return 0 on success, negative errno on failure
 */
typedef int (*ft_recovery_callback_t)(const struct ft_fault_context *ctx, 
                                      enum ft_recovery_action action);
                            
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
// Overwrites and extensions to existing fatal error handling
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
// void k_sys_fatal_error_handler(unsigned int reason, 
//                                const struct arch_esf *esf);

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
// Initialization, registration, and configuration functions
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

/**
    * @brief Initialize the fault tolerance framework
    *
    * @return 0 on success, negative errno on failure
 */
int ft_init(void);

/**
    * @brief Register a fault handler for a specific fault type
    *
    * @param fault_type Fault type to handle
    * @param handler Fault handler callback
    *
    * @return 0 on success, negative errno on failure
 */
// int ft_register_fault_handler(enum ft_fault_type fault_type, ft_fault_handler_t handler);

/**
    * @brief Register a recovery callback
    *
    * @param callback Recovery callback function
    *
    * @return 0 on success, negative errno on failure
 */
// int ft_register_recovery_callback(ft_recovery_callback_t callback);

/**
 * @brief Enable/disable fault detection for specific types
 *
 * @param fault_type Fault type to configure
 * @param enable True to enable detection, false to disable
 *
 * @return 0 on success, negative errno on failure
 */
// int ft_configure_detection(enum ft_fault_type fault_type, bool enable);

/**
 * @brief Set fault detection sensitivity
 *
 * @param fault_type Fault type to configure
 * @param sensitivity Sensitivity level (0=lowest, 100=highest)
 *
 * @return 0 on success, negative errno on failure
 */
// int ft_set_sensitivity(enum ft_fault_type fault_type, uint8_t sensitivity);


//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Get and reset stats functions
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

/**
    * @brief Retrieve fault statistics
    *
    * @param stats Pointer to fault stats structure to populate
    *
    * @return 0 on success, negative errno on failure
 */
// int ft_get_fault_stats(struct ft_fault_stats *stats);

/**
    * @brief Reset fault statistics
    *
    * @return 0 on success, negative errno on failure
 */
// not sure this is needed
// int ft_reset_fault_stats(void);

/**
    * @brief Retrieve system-wide fault statistics
    *
    * @param stats Pointer to system statistics structure to populate
    *
    * @return 0 on success, negative errno on failure
 */
// int ft_get_sys_stats(struct ft_system_stats *stats);

/**
    * @brief Reset system-wide fault statistics
    *
    * @return 0 on success, negative errno on failure
 */
// TODO : Not sure this is neeeded
// int ft_reset_sys_stats(void);

/**
 * @brief Get memory usage statistics
 *
 * @param stats Pointer to statistics structure to fill
 *
 * @return 0 on success, negative errno on failure
 */
// int ft_get_memory_stats(struct ft_memory_stats *stats);

/**
    * @brief Reset memory usage statistics
    *
    * @return 0 on success, negative errno on failure
 */
// TODO : not sure this is needed
// int ft_reset_memory_stats(void);

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Memory related functions
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

/**
 * @brief Monitor stack usage for all threads
 */
// void ft_monitor_stack_usage(void);

/**
 * @brief Check current thread stack usage
 */
// void ft_check_current_stack(void);

/**
 * @brief Initialize memory monitor
 */
// int ft_memory_monitor_init(void);

/**
 * @brief Monitor memory usage
 */
// void ft_monitor_memory_usage(void);

/**
 * @brief Track memory allocation for leak detection
 *
 * @param ptr Allocated memory pointer
 * @param size Size of allocation
 */
// void ft_track_allocation(void *ptr, size_t size);

/**
 * @brief Track memory deallocation for leak detection
 *
 * @param ptr Deallocated memory pointer
 */
// void ft_track_deallocation(void *ptr);

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Timing related functions
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

/**
 * @brief Monitor timing violations
 */
// void ft_monitor_timing_violations(void);

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Concurrency related functions
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

/**
 * @brief Monitor for deadlocks
 */
// void ft_monitor_deadlocks(void);

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Persistent logging functions
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

/**
 * @brief Initialize persistent logging
 *
 * @return 0 on success, negative errno on failure
 */
// int ft_persistent_log_init(void);

/**
 * @brief Store fault log persistently
 *
 * @param ctx Fault context to store
 * @return 0 on success, negative errno on failure
 */
// int ft_store_persistent_log(const struct ft_fault_context *ctx);

/**
 * @brief Retrieve fault logs from persistent storage
 *
 * @return 0 on success, negative errno on failure
 */
// int ft_retrieve_persistent_logs(void);

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Test mode functions 
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

/**
 * @brief Enable/disable test mode (disables recovery for safe fault injection)
 *
 * @param enable True to enable test mode, false to disable
 * @return 0 on success, negative errno on failure
 */
// int ft_set_test_mode(bool enable);

/**
 * @brief Report a fault in test mode (no recovery triggered)
 *
 * @param fault_type Type of fault detected
 * @param severity Severity level of the fault
 * @param description Human readable description
 * @param file Source file name (use __FILE__)
 * @param line Source line number (use __LINE__)
 * @param context_data Additional context-specific data
 * @return 0 on success, negative errno on failure
 */
// int ft_report_fault_test(enum ft_fault_type fault_type, 
//                         enum ft_fault_severity severity,
//                         const char *description,
//                         const char *file,
//                         uint32_t line,
//                         uintptr_t context_data[4]);


// Macro to report fault with automatic file/line information
// TODO : FT_REPORT_FAULT

// Macro to report a fault in test mode
// TODO : FT_REPORT_FAULT_TEST

// Macro for stack overflow detection check
// TODO : FT_CHECK_STACK_OVERFLOW

// Macro for resource exhuastion check
// TODO : FT_CHECK_RESOURCE_EXHAUSTION

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_FAULT_TOLERANCE_H */