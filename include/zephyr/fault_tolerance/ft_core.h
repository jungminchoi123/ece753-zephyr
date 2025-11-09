/**
 * @file ft_core.h
 * @brief Core Fault Tolerance Framework for Zephyr RTOS
 * @author Jack Ostapeic
 * 
 * This header defines the core fault tolerance framework API for Zephyr RTOS.
 * It provides comprehensive fault detection, reporting, and recovery mechanisms
 * that integrate seamlessly with Zephyr's existing subsystems.
 */

#ifndef ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_CORE_H_
#define ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_CORE_H_

#include <zephyr/kernel.h>
#include <zephyr/sys/slist.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum number of fault types */
#define FT_FAULT_TYPE_MAX 16

/** @brief Maximum description length */
#define FT_MAX_DESCRIPTION_LEN 64

/** @brief Maximum custom data size */
#define FT_MAX_CUSTOM_DATA_SIZE 256

/** @brief Fault types */
enum ft_fault_type {
    FT_FAULT_STACK_OVERFLOW = 0,    /**< Stack overflow detected */
    FT_FAULT_MEMORY_CORRUPTION,     /**< Memory corruption detected */
    FT_FAULT_DEADLOCK,              /**< Deadlock detected */
    FT_FAULT_HARDWARE_ERROR,        /**< Hardware error detected */
    FT_FAULT_TIMEOUT,               /**< Timeout exceeded */
    FT_FAULT_ASSERTION_FAILED,      /**< Assertion failed */
    FT_FAULT_WATCHDOG_TIMEOUT,      /**< Watchdog timeout */
    FT_FAULT_THREAD_EXCEPTION,      /**< Thread exception */
    FT_FAULT_RESOURCE_EXHAUSTION,   /**< Resource exhaustion */
    FT_FAULT_CUSTOM,                /**< Custom fault type */
    FT_FAULT_TYPE_COUNT
};

/** @brief Severity levels */
enum ft_severity {
    FT_SEVERITY_INFO = 0,       /**< Informational */
    FT_SEVERITY_WARNING,        /**< Warning level */
    FT_SEVERITY_ERROR,          /**< Error level */
    FT_SEVERITY_CRITICAL,       /**< Critical level */
    FT_SEVERITY_FATAL           /**< Fatal level */
};

/** @brief Recovery actions */
enum ft_recovery_action {
    FT_RECOVERY_NONE = 0,           /**< No recovery action */
    FT_RECOVERY_RESTART_THREAD,     /**< Restart the faulted thread */
    FT_RECOVERY_RESTART_SUBSYSTEM,  /**< Restart entire subsystem */
    FT_RECOVERY_SYSTEM_REBOOT,      /**< Reboot the system */
    FT_RECOVERY_GRACEFUL_SHUTDOWN,  /**< Graceful system shutdown */
    FT_RECOVERY_CUSTOM              /**< Custom recovery handler */
};

/** @brief Fault handler return codes */
enum ft_handler_result {
    FT_HANDLER_CONTINUE = 0,    /**< Continue to next handler */
    FT_HANDLER_HANDLED,         /**< Fault was handled, stop processing */
    FT_HANDLER_ERROR            /**< Handler encountered error */
};

/** @brief Fault context structure */
struct ft_fault_context {
    enum ft_fault_type fault_type;          /**< Type of fault */
    enum ft_severity severity;              /**< Severity level */
    k_tid_t thread_id;                      /**< Faulted thread ID */
    uint32_t error_code;                    /**< Error code */
    uint64_t timestamp;                     /**< Fault timestamp */
    const char *description;                /**< Fault description */
    void *custom_data;                      /**< Custom fault data */
    size_t custom_data_size;                /**< Size of custom data */
    const struct arch_esf *esf;             /**< Exception stack frame */
    void *fault_address;                    /**< Fault address if applicable */
};

/** @brief Recovery context structure */
struct ft_recovery_context {
    enum ft_recovery_action action;         /**< Recovery action to take */
    k_tid_t target_thread;                  /**< Target thread for recovery */
    void *recovery_data;                    /**< Recovery-specific data */
    size_t recovery_data_size;              /**< Size of recovery data */
    uint32_t timeout_ms;                    /**< Recovery timeout */
    bool async_recovery;                    /**< Asynchronous recovery flag */
};

/** @brief Custom recovery handler function */
typedef int (*ft_recovery_handler_t)(struct ft_recovery_context *ctx);

/** @brief Fault handler function signature */
typedef enum ft_handler_result (*ft_fault_handler_t)(
    const struct ft_fault_context *fault_ctx,
    struct ft_recovery_context *recovery_ctx,
    void *user_data
);

/** @brief Fault handler registration structure */
struct ft_handler {
    sys_snode_t node;                       /**< List node */
    enum ft_fault_type fault_type;          /**< Fault type to handle */
    ft_fault_handler_t handler;             /**< Handler function */
    void *user_data;                        /**< User-provided data */
    int priority;                           /**< Handler priority (lower = higher priority) */
    const char *name;                       /**< Handler name for debugging */
};

/** @brief Framework statistics */
struct ft_stats {
    atomic_t total_faults;                  /**< Total faults reported */
    atomic_t recovered_faults;              /**< Successfully recovered faults */
    atomic_t unrecoverable_faults;          /**< Unrecoverable faults */
    atomic_t handler_errors;                /**< Handler execution errors */
    atomic_t fault_counts[FT_FAULT_TYPE_COUNT]; /**< Per-type fault counts */
    uint64_t last_fault_timestamp;          /**< Timestamp of last fault */
    enum ft_fault_type last_fault_type;     /**< Type of last fault */
};

/** @brief Thread recovery configuration */
struct ft_thread_recovery_config {
    size_t new_stack_size;                  /**< New stack size for thread */
    int priority;                           /**< New thread priority */
    uint32_t options;                       /**< Thread options */
    k_thread_entry_t entry_point;          /**< Thread entry point */
    void *p1, *p2, *p3;                     /**< Thread parameters */
};

/** @brief Fault tolerance framework configuration */
struct ft_config {
    bool enable_logging;                    /**< Enable fault logging */
    bool enable_statistics;                 /**< Enable statistics collection */
    bool enable_async_recovery;             /**< Enable asynchronous recovery */
    uint32_t max_recovery_attempts;         /**< Maximum recovery attempts */
    uint32_t recovery_timeout_ms;           /**< Default recovery timeout */
};

/** @brief API Functions */

/**
 * @brief Initialize the fault tolerance framework
 * 
 * @param config Framework configuration (NULL for defaults)
 * @return 0 on success, negative error code on failure
 */
int ft_init(const struct ft_config *config);

/**
 * @brief Register a fault handler
 * 
 * @param handler Handler structure to register
 * @return 0 on success, negative error code on failure
 */
int ft_register_handler(struct ft_handler *handler);

/**
 * @brief Unregister a fault handler
 * 
 * @param handler Handler structure to unregister
 * @return 0 on success, negative error code on failure
 */
int ft_unregister_handler(struct ft_handler *handler);

/**
 * @brief Report a fault to the framework
 * 
 * @param fault_ctx Fault context information
 * @return 0 on success, negative error code on failure
 */
int ft_report_fault(const struct ft_fault_context *fault_ctx);

/**
 * @brief Get framework statistics
 * 
 * @param stats Statistics structure to fill
 * @return 0 on success, negative error code on failure
 */
int ft_get_stats(struct ft_stats *stats);

/**
 * @brief Reset framework statistics
 * 
 * @return 0 on success, negative error code on failure
 */
int ft_reset_stats(void);

/**
 * @brief Check if framework is initialized
 * 
 * @return true if initialized, false otherwise
 */
bool ft_is_initialized(void);

/**
 * @brief Set recovery handler for custom recovery actions
 * 
 * @param handler Custom recovery handler
 * @return 0 on success, negative error code on failure
 */
int ft_set_recovery_handler(ft_recovery_handler_t handler);

/**
 * @brief Enable or disable fault type
 * 
 * @param fault_type Fault type to configure
 * @param enable Enable or disable
 * @return 0 on success, negative error code on failure
 */
int ft_configure_fault_type(enum ft_fault_type fault_type, bool enable);

/**
 * @brief Get fault type name string
 * 
 * @param fault_type Fault type
 * @return String name of fault type
 */
const char *ft_get_fault_type_name(enum ft_fault_type fault_type);

/**
 * @brief Get severity name string
 * 
 * @param severity Severity level
 * @return String name of severity level
 */
const char *ft_get_severity_name(enum ft_severity severity);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_CORE_H_ */