/** @file
 * @brief Fault Tolerance API Header
 * @author Jack Ostapeic, MS ECE student at UW-Madison
 * ECE753 - Safety-Critical Embedded Systems
 *
 * This header defines the Fault Tolerance (FT) API for Zephyr RTOS.
 * It includes function prototypes, data structures, and macros for
 * implementing fault detection, reporting, and recovery mechanisms.
 */

#ifndef ZEPHYR_INCLUDE_FAULT_TOLERANCE_H
#define ZEPHYR_INCLUDE_FAULT_TOLERANCE_H

// standard zephyr includes
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>

// fatal inclusion
#include <zephyr/fatal.h>
#include <zephyr/fatal_types.h>

// // Logging inclusion
// #include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ft_fault_type {
    FT_STACK_OVERFLOW_FAULT = 0x100,
    FT_UNKNOWN_FAULT = 0x101,
    // FT_MEMORY_LEAK_FAULT = 0x102,
    // FT_DEADLOCK_FAULT = 0x103,
    // FT_RACE_CONDITION_FAULT = 0x104,
    // FT_BUFFER_OVERFLOW_FAULT = 0x105,
    // FT_TIMING_VIOLATION_FAULT = 0x106,
    // FT_ASSERT_FAULT = 0x107,
    // FT_DIVISION_BY_ZERO_FAULT = 0x108,
    
    // /* ARM Cortex-M3 specific fault types */
    // FT_NULL_POINTER_FAULT = 0x200,
    // FT_HARD_FAULT = 0x201,
    // FT_MEMORY_MANAGEMENT_FAULT = 0x202,
    // FT_BUS_FAULT = 0x203,
    // FT_USAGE_FAULT = 0x204,
    // FT_PERIPHERAL_ACCESS_FAULT = 0x205,
    // FT_MPU_VIOLATION_FAULT = 0x206,
    // FT_HEAP_CORRUPTION_FAULT = 0x207,
    // FT_DOUBLE_FREE_FAULT = 0x208,
    // FT_WATCHDOG_TIMEOUT_FAULT = 0x209,
    // FT_INVALID_INSTRUCTION_FAULT = 0x20A,
    // FT_UNALIGNED_ACCESS_FAULT = 0x20B
};

enum ft_fault_severity {
    FT_SEVERITY_LOW = 0,
    FT_SEVERITY_MEDIUM,
    FT_SEVERITY_HIGH,
    FT_SEVERITY_CRITICAL
};

enum ft_recovery_action {
    FT_RECOVERY_NONE = 0
};

struct ft_fault_context {
    enum ft_fault_type fault_type;              // type of fault
    enum ft_fault_severity severity;            // severity level
};

/* ARM Cortex-M3 specific fault context */
struct ft_cortexm_fault_context {
    enum ft_fault_type fault_type;              // type of fault
    enum ft_fault_severity severity;            // severity level
    uint32_t fault_address;                     // faulting memory address
    uint32_t fault_pc;                          // program counter at fault
    uint32_t fault_lr;                          // link register
    uint32_t fault_status;                      // fault status register
    struct k_thread *fault_thread;              // faulting thread
    uint32_t stack_pointer;                     // stack pointer at fault
};

struct ft_fault_stats {
    uint32_t total_faults;                      // total faults detected
};

struct ft_memory_stats {
    size_t total_heap;                     // total heap size   
    size_t used_heap;                      // used heap size
    size_t free_heap;                      // free heap size
};

struct ft_system_stats {
    struct ft_fault_stats fault_stats;          // fault statistics
    struct ft_memory_stats memory_stats;        // memory statistics
    int64_t init_time;                          // system uptime at init
};

/**
    * @brief Initialize the Fault Tolerance subsystem
    *
    * This function sets up the necessary data structures and state
    * for the Fault Tolerance API to operate correctly.
 */
int ft_init(void);

/**
    * @brief Check stack usage of current thread
    *
    * This function checks the stack usage of the calling thread
    * and reports any potential overflow conditions.
 */
// int ft_check_stack_usage(void);

/**
    * @brief Report a fault to the fault tolerance system
    *
    * This function allows applications to report faults to the FT system.
 */
// int ft_report_fault(enum ft_fault_type fault_type, enum ft_fault_severity severity);

/* ARM Cortex-M3 Enhanced Fault Tolerance API */

/**
 * @brief Report an ARM Cortex-M fault with detailed context
 * 
 * This function reports faults with ARM Cortex-M3 specific information
 * including fault address, PC, and register context.
 *
 * @param context Pointer to detailed ARM fault context
 * @return 0 on success, negative on error
 */
// int ft_report_cortexm_fault(struct ft_cortexm_fault_context *context);

/**
 * @brief Initialize MPU-based memory protection
 *
 * Sets up Memory Protection Unit regions for fault isolation
 * and detection of memory access violations.
 *
 * @return 0 on success, negative on error
 */
// int ft_mpu_init(void);

/**
 * @brief Configure thread isolation using MPU
 *
 * Isolates a thread using MPU regions to prevent it from
 * corrupting other threads or system memory.
 *
 * @param thread Pointer to thread to isolate
 * @return 0 on success, negative on error
 */
// int ft_isolate_thread(struct k_thread *thread);

/**
 * @brief Enable hardware fault detection
 *
 * Configures ARM Cortex-M3 fault detection features including
 * divide-by-zero traps, unaligned access detection, etc.
 *
 * @return 0 on success, negative on error
 */
// int ft_enable_hardware_faults(void);

/**
 * @brief Restart a faulted thread safely
 *
 * Safely restarts a thread that has encountered a recoverable fault,
 * resetting its stack and context.
 *
 * @param thread Pointer to thread to restart
 * @param entry_func New entry function for the thread
 * @return 0 on success, negative on error
 */
// int ft_restart_thread(struct k_thread *thread, k_thread_entry_t entry_func);

/**
 * @brief Get detailed fault statistics
 *
 * Returns comprehensive fault statistics including ARM-specific
 * fault counts and recovery success rates.
 *
 * @param stats Pointer to statistics structure to fill
 * @return 0 on success, negative on error
 */
// int ft_get_detailed_stats(struct ft_system_stats *stats);

// void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_FAULT_TOLERANCE_H */

