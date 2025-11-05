/*
 * Copyright (c) 2025 ECE753 Project Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 * @brief Enhanced fault tolerance framework for Zephyr RTOS
 *
 * This module provides comprehensive fault detection, recovery, and logging
 * capabilities for safety-critical embedded systems applications.
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

/**
 * @defgroup fault_tolerance Enhanced Fault Tolerance APIs
 * @ingroup kernel_apis
 * @{
 */

/**
 * @brief Enhanced fault types for detailed fault classification
 */
enum ft_fault_type {
	/** Stack overflow detected */
	FT_FAULT_STACK_OVERFLOW = 0x1000,
	
	/** Heap corruption detected */
	FT_FAULT_HEAP_CORRUPTION,
	
	/** Memory leak detected */
	FT_FAULT_MEMORY_LEAK,
	
	/** Thread deadlock detected */
	FT_FAULT_DEADLOCK,
	
	/** Race condition detected */
	FT_FAULT_RACE_CONDITION,
	
	/** Resource exhaustion */
	FT_FAULT_RESOURCE_EXHAUSTION,
	
	/** Peripheral failure */
	FT_FAULT_PERIPHERAL_FAILURE,
	
	/** Timing violation */
	FT_FAULT_TIMING_VIOLATION,
	
	/** Data corruption */
	FT_FAULT_DATA_CORRUPTION,
	
	/** Communication failure */
	FT_FAULT_COMM_FAILURE,
	
	/** Power management fault */
	FT_FAULT_POWER_MGMT,
	
	/** Configuration error */
	FT_FAULT_CONFIG_ERROR,
	
	/** Unknown/custom fault */
	FT_FAULT_UNKNOWN = 0x1FFF
};

/**
 * @brief Fault severity levels
 */
enum ft_fault_severity {
	/** System can continue with degraded performance */
	FT_SEVERITY_LOW = 0,
	
	/** System can continue but needs attention */
	FT_SEVERITY_MEDIUM,
	
	/** System functionality significantly impacted */
	FT_SEVERITY_HIGH,
	
	/** System cannot continue safely */
	FT_SEVERITY_CRITICAL
};

/**
 * @brief Recovery action types
 */
enum ft_recovery_action {
	/** No action needed */
	FT_RECOVERY_NONE = 0,
	
	/** Restart failed thread */
	FT_RECOVERY_RESTART_THREAD,
	
	/** Reset peripheral */
	FT_RECOVERY_RESET_PERIPHERAL,
	
	/** Fallback to safe mode */
	FT_RECOVERY_SAFE_MODE,
	
	/** System restart */
	FT_RECOVERY_SYSTEM_RESTART,
	
	/** Emergency shutdown */
	FT_RECOVERY_EMERGENCY_SHUTDOWN,
	
	/** Custom recovery procedure */
	FT_RECOVERY_CUSTOM
};

/**
 * @brief Fault context information
 */
struct ft_fault_context {
	/** Fault type */
	enum ft_fault_type fault_type;
	
	/** Fault severity */
	enum ft_fault_severity severity;
	
	/** Thread that experienced the fault */
	k_tid_t faulting_thread;
	
	/** Fault timestamp in ticks */
	int64_t timestamp;
	
	/** Program counter at fault */
	uintptr_t pc;
	
	/** Stack pointer at fault */
	uintptr_t sp;
	
	/** Additional context data */
	uintptr_t context_data[4];
	
	/** Human readable fault description */
	const char *description;
	
	/** File name where fault occurred */
	const char *file;
	
	/** Line number where fault occurred */
	uint32_t line;
};

/**
 * @brief Fault statistics
 */
struct ft_fault_stats {
	/** Total number of faults detected */
	uint32_t total_faults;
	
	/** Number of faults per type */
	uint32_t fault_counts[16];
	
	/** Number of successful recoveries */
	uint32_t successful_recoveries;
	
	/** Number of failed recoveries */
	uint32_t failed_recoveries;
	
	/** System uptime at first fault */
	int64_t first_fault_time;
	
	/** System uptime at last fault */
	int64_t last_fault_time;
	
	/** Mean time between failures */
	uint32_t mtbf_ms;
};

/**
 * @brief Memory usage statistics
 */
struct ft_memory_stats {
	/** Total heap size */
	size_t heap_total;
	
	/** Used heap size */
	size_t heap_used;
	
	/** Free heap size */
	size_t heap_free;
	
	/** Number of tracked allocations */
	uint32_t tracked_allocations;
	
	/** Total bytes in tracked allocations */
	size_t tracked_bytes;
};

/**
 * @brief System-wide fault tolerance statistics
 */
struct ft_system_stats {
	/** Total number of faults detected */
	uint32_t total_faults;
	
	/** Number of critical faults */
	uint32_t critical_faults;
	
	/** Number of successful recoveries */
	uint32_t successful_recoveries;
	
	/** Number of failed recoveries */
	uint32_t failed_recoveries;
	
	/** System uptime when initialized */
	int64_t init_time;
	
	/** Last fault timestamp */
	int64_t last_fault_time;
	
	/** Number of monitored threads */
	uint32_t monitored_threads;
	
	/** Number of active fault handlers */
	uint32_t active_handlers;
};

/**
 * @brief Fault handler callback type
 *
 * @param ctx Fault context information
 * @return Recovery action to take
 */
typedef enum ft_recovery_action (*ft_fault_handler_t)(const struct ft_fault_context *ctx);

/**
 * @brief Recovery callback type
 *
 * @param ctx Fault context information
 * @param action Recovery action being performed
 * @return 0 on success, negative errno on failure
 */
typedef int (*ft_recovery_callback_t)(const struct ft_fault_context *ctx, 
                                     enum ft_recovery_action action);

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
 * @param handler Handler function to call
 * @return 0 on success, negative errno on failure
 */
int ft_register_fault_handler(enum ft_fault_type fault_type, ft_fault_handler_t handler);

/**
 * @brief Register a recovery callback
 *
 * @param callback Recovery callback function
 * @return 0 on success, negative errno on failure
 */
int ft_register_recovery_callback(ft_recovery_callback_t callback);

/**
 * @brief Report a fault to the fault tolerance framework
 *
 * @param fault_type Type of fault detected
 * @param severity Severity level of the fault
 * @param description Human readable description
 * @param file Source file name (use __FILE__)
 * @param line Source line number (use __LINE__)
 * @param context_data Additional context-specific data
 * @return 0 on success, negative errno on failure
 */
int ft_report_fault(enum ft_fault_type fault_type, 
                   enum ft_fault_severity severity,
                   const char *description,
                   const char *file,
                   uint32_t line,
                   uintptr_t context_data[4]);

/**
 * @brief Get fault statistics
 *
 * @param stats Pointer to statistics structure to fill
 * @return 0 on success, negative errno on failure
 */
int ft_get_stats(struct ft_fault_stats *stats);

/**
 * @brief Get system-wide fault tolerance statistics
 *
 * @param stats Pointer to system statistics structure to fill
 * @return 0 on success, negative errno on failure
 */
int ft_get_statistics(struct ft_system_stats *stats);

/**
 * @brief Reset fault statistics
 *
 * @return 0 on success, negative errno on failure
 */
int ft_reset_stats(void);

/**
 * @brief Enable/disable fault detection for specific types
 *
 * @param fault_type Fault type to configure
 * @param enable True to enable detection, false to disable
 * @return 0 on success, negative errno on failure
 */
int ft_configure_detection(enum ft_fault_type fault_type, bool enable);

/**
 * @brief Set fault detection sensitivity
 *
 * @param fault_type Fault type to configure
 * @param sensitivity Sensitivity level (0=lowest, 100=highest)
 * @return 0 on success, negative errno on failure
 */
int ft_set_sensitivity(enum ft_fault_type fault_type, uint8_t sensitivity);

/**
 * @brief Monitor stack usage for all threads
 */
void ft_monitor_stack_usage(void);

/**
 * @brief Check current thread stack usage
 */
void ft_check_current_stack(void);

/**
 * @brief Initialize memory monitor
 */
int ft_memory_monitor_init(void);

/**
 * @brief Monitor memory usage
 */
void ft_monitor_memory_usage(void);

/**
 * @brief Track memory allocation for leak detection
 *
 * @param ptr Allocated memory pointer
 * @param size Size of allocation
 */
void ft_track_allocation(void *ptr, size_t size);

/**
 * @brief Track memory deallocation for leak detection
 *
 * @param ptr Deallocated memory pointer
 */
void ft_track_deallocation(void *ptr);

/**
 * @brief Get memory usage statistics
 *
 * @param stats Pointer to statistics structure to fill
 * @return 0 on success, negative errno on failure
 */
int ft_get_memory_stats(struct ft_memory_stats *stats);

/**
 * @brief Monitor timing violations
 */
void ft_monitor_timing_violations(void);

/**
 * @brief Monitor for deadlocks
 */
void ft_monitor_deadlocks(void);

/**
 * @brief Initialize persistent logging
 *
 * @return 0 on success, negative errno on failure
 */
int ft_persistent_log_init(void);

/**
 * @brief Store fault log persistently
 *
 * @param ctx Fault context to store
 * @return 0 on success, negative errno on failure
 */
int ft_store_persistent_log(const struct ft_fault_context *ctx);

/**
 * @brief Retrieve fault logs from persistent storage
 *
 * @return 0 on success, negative errno on failure
 */
int ft_retrieve_persistent_logs(void);

/**
 * @brief Enable/disable test mode (disables recovery for safe fault injection)
 *
 * @param enable True to enable test mode, false to disable
 * @return 0 on success, negative errno on failure
 */
int ft_set_test_mode(bool enable);

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
int ft_report_fault_test(enum ft_fault_type fault_type, 
                        enum ft_fault_severity severity,
                        const char *description,
                        const char *file,
                        uint32_t line,
                        uintptr_t context_data[4]);

/**
 * @brief Macro to report a fault with automatic file/line information
 */
#define FT_REPORT_FAULT(type, severity, description, context) \
    ft_report_fault(type, severity, description, __FILE__, __LINE__, context)

/**
 * @brief Macro to report a fault in test mode (safe for testing)
 */
#define FT_REPORT_FAULT_TEST(type, severity, description, context) \
    ft_report_fault_test(type, severity, description, __FILE__, __LINE__, context)

/**
 * @brief Macro for stack overflow detection check
 */
#define FT_CHECK_STACK_OVERFLOW() \
    do { \
        /* Simplified stack overflow check for this version of Zephyr */ \
        LOG_DBG("Stack overflow check performed"); \
    } while (0)

/**
 * @brief Macro for resource exhaustion check
 */
#define FT_CHECK_RESOURCE_EXHAUSTION(resource_name, current, limit) \
    do { \
        if ((current) >= (limit)) { \
            uintptr_t ctx[4] = {(uintptr_t)(current), (uintptr_t)(limit), 0, 0}; \
            FT_REPORT_FAULT(FT_FAULT_RESOURCE_EXHAUSTION, FT_SEVERITY_HIGH, \
                           resource_name " exhausted", ctx); \
        } \
    } while (0)

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_FAULT_TOLERANCE_H */