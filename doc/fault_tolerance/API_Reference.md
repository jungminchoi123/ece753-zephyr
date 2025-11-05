# Fault Tolerance Framework API Reference

**ECE753 Project - API Documentation**  
**Version:** 1.0

## Table of Contents

1. [Core API Functions](#core-api-functions)
2. [Data Structures](#data-structures)
3. [Enumerations](#enumerations)
4. [Macros](#macros)
5. [Configuration](#configuration)
6. [Usage Examples](#usage-examples)

## Core API Functions

### Framework Initialization

#### `ft_init()`
```c
int ft_init(void);
```
**Description**: Initializes the fault tolerance framework subsystem.

**Returns**: 
- `0` on success
- `-ENOTSUP` if framework not enabled
- `-ENOMEM` if initialization fails

**Usage**:
```c
int ret = ft_init();
if (ret != 0) {
    printk("Fault tolerance initialization failed: %d\n", ret);
    return ret;
}
```

---

### Fault Reporting

#### `ft_report_fault()`
```c
void ft_report_fault(enum ft_fault_type type, 
                     enum ft_fault_severity severity,
                     const char *description, 
                     uintptr_t context[4]);
```
**Description**: Reports a fault to the framework for processing and recovery.

**Parameters**:
- `type`: Type of fault detected (see `ft_fault_type` enum)
- `severity`: Severity level of the fault (see `ft_fault_severity` enum)  
- `description`: Human-readable description of the fault
- `context`: Array of 4 context values (addresses, error codes, etc.)

**Usage**:
```c
uintptr_t ctx[4] = {(uintptr_t)failed_ptr, errno, 0, 0};
ft_report_fault(FT_FAULT_MEMORY_CORRUPTION, FT_SEVERITY_HIGH,
                "Buffer overflow detected", ctx);
```

#### `ft_report_fault_test()`
```c
void ft_report_fault_test(enum ft_fault_type type,
                          enum ft_fault_severity severity, 
                          const char *description,
                          uintptr_t context[4]);
```
**Description**: Reports a fault in test mode (safe, non-crashing fault injection).

**Note**: Only functions when `ft_set_test_mode(true)` has been called.

**Usage**:
```c
ft_set_test_mode(true);
uintptr_t test_ctx[4] = {0x1234, 0x5678, 0, 0};
ft_report_fault_test(FT_FAULT_DEADLOCK_DETECTED, FT_SEVERITY_CRITICAL,
                     "Simulated deadlock for testing", test_ctx);
```

---

### Handler Registration

#### `ft_register_fault_handler()`
```c
int ft_register_fault_handler(enum ft_fault_type type,
                              ft_fault_handler_t handler);
```
**Description**: Registers a custom fault handler for a specific fault type.

**Parameters**:
- `type`: Fault type to handle
- `handler`: Function pointer to handler (see `ft_fault_handler_t`)

**Returns**: 
- `0` on success
- `-EINVAL` if invalid parameters
- `-ENOMEM` if registration fails

**Usage**:
```c
static enum ft_recovery_action my_handler(const struct ft_fault_context *ctx) {
    printk("Handling fault type %d, severity %d\n", ctx->fault_type, ctx->severity);
    return FT_RECOVERY_RESTART;
}

int ret = ft_register_fault_handler(FT_FAULT_RESOURCE_EXHAUSTION, my_handler);
```

#### `ft_register_recovery_callback()`
```c
int ft_register_recovery_callback(ft_recovery_callback_t callback);
```
**Description**: Registers a callback to be notified of recovery actions.

**Parameters**:
- `callback`: Function to call after recovery actions (see `ft_recovery_callback_t`)

**Usage**:
```c
static int recovery_callback(const struct ft_fault_context *ctx, 
                            enum ft_recovery_action action) {
    printk("Recovery action %d completed for fault %d\n", action, ctx->fault_type);
    return 0;
}

ft_register_recovery_callback(recovery_callback);
```

---

### System Monitoring

#### `ft_get_stats()`
```c
void ft_get_stats(struct ft_fault_stats *stats);
```
**Description**: Retrieves current fault tolerance statistics.

**Parameters**:
- `stats`: Pointer to structure to fill with statistics

**Usage**:
```c
struct ft_fault_stats stats;
ft_get_stats(&stats);
printk("Total faults: %d, Recoveries: %d\n", 
       stats.total_faults, stats.successful_recoveries);
```

#### `ft_get_memory_stats()`
```c
void ft_get_memory_stats(struct ft_memory_stats *stats);
```
**Description**: Retrieves memory monitoring statistics.

**Usage**:
```c
struct ft_memory_stats mem_stats;
ft_get_memory_stats(&mem_stats);
printk("Heap used: %zu, free: %zu\n", mem_stats.heap_used, mem_stats.heap_free);
```

#### `ft_monitor_memory_usage()`
```c
void ft_monitor_memory_usage(void);
```
**Description**: Triggers immediate memory usage monitoring and fault detection.

#### `ft_monitor_stack_usage()`
```c
void ft_monitor_stack_usage(void);
```
**Description**: Monitors thread stack usage and detects potential overflows.

---

### Memory Tracking

#### `ft_track_allocation()`
```c
void ft_track_allocation(void *ptr, size_t size);
```
**Description**: Tracks memory allocation for leak detection and monitoring.

**Parameters**:
- `ptr`: Pointer to allocated memory
- `size`: Size of allocation in bytes

#### `ft_track_deallocation()`
```c
void ft_track_deallocation(void *ptr);
```
**Description**: Records memory deallocation to update tracking.

---

### Test Mode Control

#### `ft_set_test_mode()`
```c
void ft_set_test_mode(bool enable);
```
**Description**: Enables or disables test mode for safe fault injection.

**Parameters**:
- `enable`: `true` to enable test mode, `false` to disable

**Usage**:
```c
// Enable safe fault injection for testing
ft_set_test_mode(true);

// Inject test faults without system crashes
FT_REPORT_FAULT_TEST(FT_FAULT_TIMING_VIOLATION, FT_SEVERITY_MEDIUM, 
                     "Test timing fault", ctx);

// Disable test mode for production
ft_set_test_mode(false);
```

---

## Data Structures

### `struct ft_fault_context`
```c
struct ft_fault_context {
    enum ft_fault_type fault_type;        /* Type of fault */
    enum ft_fault_severity severity;      /* Severity level */
    const char *description;              /* Fault description */
    uintptr_t context[4];                 /* Context information */
    uint32_t timestamp;                   /* When fault occurred */
    struct k_thread *thread;              /* Thread context */
    void *stack_trace[8];                 /* Stack trace */
    uint8_t stack_depth;                  /* Stack trace depth */
};
```

### `struct ft_fault_stats`
```c
struct ft_fault_stats {
    uint32_t total_faults;                /* Total faults detected */
    uint32_t successful_recoveries;       /* Successful recovery actions */
    uint32_t failed_recoveries;           /* Failed recovery attempts */
    uint32_t mtbf_ms;                     /* Mean time between failures */
    uint32_t fault_counts[FT_FAULT_COUNT]; /* Per-type fault counts */
};
```

### `struct ft_memory_stats`
```c
struct ft_memory_stats {
    size_t heap_used;                     /* Current heap usage */
    size_t heap_free;                     /* Available heap memory */
    uint32_t tracked_allocations;         /* Number tracked allocations */
    size_t peak_usage;                    /* Peak memory usage */
    uint32_t allocation_failures;         /* Failed allocation count */
};
```

---

## Enumerations

### `enum ft_fault_type`
```c
enum ft_fault_type {
    FT_FAULT_STACK_OVERFLOW,              /* Thread stack overflow */
    FT_FAULT_MEMORY_CORRUPTION,           /* Memory corruption detected */
    FT_FAULT_DEADLOCK_DETECTED,           /* System deadlock */
    FT_FAULT_RESOURCE_EXHAUSTION,         /* Resource exhaustion */
    FT_FAULT_TIMING_VIOLATION,            /* Real-time constraint violation */
    FT_FAULT_DATA_CORRUPTION,             /* Data integrity failure */
    FT_FAULT_HARDWARE_FAILURE,            /* Hardware malfunction */
    FT_FAULT_COMM_FAILURE,                /* Communication failure */
    FT_FAULT_PERIPHERAL_FAILURE,          /* Peripheral device failure */
    FT_FAULT_POWER_ANOMALY,               /* Power supply issue */
    FT_FAULT_CONFIG_ERROR,                /* Configuration error */
    FT_FAULT_UNKNOWN,                     /* Unknown/unclassified fault */
    FT_FAULT_COUNT                        /* Total number of fault types */
};
```

### `enum ft_fault_severity`
```c
enum ft_fault_severity {
    FT_SEVERITY_LOW = 0,                  /* Low impact, informational */
    FT_SEVERITY_MEDIUM = 1,               /* Medium impact, may affect performance */
    FT_SEVERITY_HIGH = 2,                 /* High impact, affects functionality */
    FT_SEVERITY_CRITICAL = 3              /* Critical, system integrity at risk */
};
```

### `enum ft_recovery_action`
```c
enum ft_recovery_action {
    FT_RECOVERY_NONE,                     /* No recovery action needed */
    FT_RECOVERY_RETRY,                    /* Retry the failed operation */
    FT_RECOVERY_RESTART,                  /* Restart affected subsystem */
    FT_RECOVERY_SAFE_MODE,                /* Enter safe/degraded mode */
    FT_RECOVERY_RESET_SYSTEM,             /* System reset required */
    FT_RECOVERY_CUSTOM,                   /* Custom recovery handler */
    FT_RECOVERY_ISOLATE                   /* Isolate faulty component */
};
```

---

## Macros

### `FT_REPORT_FAULT(type, severity, desc, ctx)`
**Description**: Convenience macro for fault reporting in production code.

**Usage**:
```c
uintptr_t context[4] = {error_code, address, 0, 0};
FT_REPORT_FAULT(FT_FAULT_HARDWARE_FAILURE, FT_SEVERITY_HIGH,
                "Sensor communication timeout", context);
```

### `FT_REPORT_FAULT_TEST(type, severity, desc, ctx)`
**Description**: Safe fault reporting macro for testing (requires test mode).

**Usage**:
```c
ft_set_test_mode(true);
uintptr_t test_ctx[4] = {0, 0, 0, 0};
FT_REPORT_FAULT_TEST(FT_FAULT_MEMORY_CORRUPTION, FT_SEVERITY_CRITICAL,
                     "Simulated memory corruption", test_ctx);
```

---

## Function Pointer Types

### `ft_fault_handler_t`
```c
typedef enum ft_recovery_action (*ft_fault_handler_t)(const struct ft_fault_context *context);
```
**Description**: Function pointer type for custom fault handlers.

**Parameters**:
- `context`: Complete fault context information

**Returns**: Recovery action to take (see `ft_recovery_action` enum)

### `ft_recovery_callback_t`
```c
typedef int (*ft_recovery_callback_t)(const struct ft_fault_context *context,
                                      enum ft_recovery_action action);
```
**Description**: Function pointer type for recovery completion callbacks.

**Returns**: 0 on success, negative error code on failure

---

## Configuration Options

### Kconfig Symbols

```kconfig
CONFIG_FAULT_TOLERANCE=y                 # Enable framework
CONFIG_FAULT_TOLERANCE_STATISTICS=y      # Enable statistics collection  
CONFIG_FAULT_TOLERANCE_AUTO_RECOVERY=y   # Enable automatic recovery
CONFIG_FAULT_TOLERANCE_STACK_MONITORING=y # Stack overflow monitoring
CONFIG_FAULT_TOLERANCE_MEMORY_MONITORING=y # Memory leak detection
CONFIG_FAULT_TOLERANCE_TIMING_MONITORING=y # Timing violation detection
CONFIG_FAULT_TOLERANCE_DEADLOCK_DETECTION=y # Deadlock detection
CONFIG_FAULT_TOLERANCE_WATCHDOG_INTEGRATION=y # Watchdog integration

# Resource limits
CONFIG_FAULT_TOLERANCE_MAX_LOG_ENTRIES=128
CONFIG_FAULT_TOLERANCE_MAX_HANDLERS_PER_TYPE=4
CONFIG_FAULT_TOLERANCE_MAX_RECOVERY_CALLBACKS=8
```

---

## Usage Examples

### Basic Framework Setup
```c
#include <fault_tolerance/fault_tolerance.h>

int main(void) {
    // Initialize framework
    int ret = ft_init();
    if (ret != 0) {
        printk("FT initialization failed: %d\n", ret);
        return ret;
    }
    
    // Register custom handlers for critical faults
    ft_register_fault_handler(FT_FAULT_MEMORY_CORRUPTION, memory_fault_handler);
    ft_register_fault_handler(FT_FAULT_DEADLOCK_DETECTED, deadlock_handler);
    
    // Register recovery callback for monitoring
    ft_register_recovery_callback(recovery_monitor);
    
    printk("Fault tolerance framework initialized\n");
    return 0;
}
```

### Custom Fault Handler
```c
static enum ft_recovery_action memory_fault_handler(const struct ft_fault_context *ctx) {
    printk("Memory fault detected: %s\n", ctx->description);
    printk("Context: 0x%08x, 0x%08x\n", ctx->context[0], ctx->context[1]);
    
    // Analyze fault severity and context
    if (ctx->severity >= FT_SEVERITY_HIGH) {
        // High severity - restart subsystem
        return FT_RECOVERY_RESTART;
    } else {
        // Lower severity - retry operation
        return FT_RECOVERY_RETRY;
    }
}
```

### Memory Monitoring Example
```c
void monitor_system_health(void) {
    struct ft_memory_stats mem_stats;
    struct ft_fault_stats fault_stats;
    
    // Get current statistics
    ft_get_memory_stats(&mem_stats);
    ft_get_stats(&fault_stats);
    
    // Check memory usage
    if (mem_stats.heap_used > (mem_stats.heap_used + mem_stats.heap_free) * 0.9) {
        uintptr_t ctx[4] = {mem_stats.heap_used, mem_stats.heap_free, 0, 0};
        FT_REPORT_FAULT(FT_FAULT_RESOURCE_EXHAUSTION, FT_SEVERITY_HIGH,
                        "High memory usage detected", ctx);
    }
    
    printk("System Health: %d faults, %d recoveries\n",
           fault_stats.total_faults, fault_stats.successful_recoveries);
}
```

### Safe Testing Example
```c
void run_fault_injection_test(void) {
    printk("Starting fault injection test\n");
    
    // Enable test mode for safe fault injection
    ft_set_test_mode(true);
    
    // Test various fault scenarios
    uintptr_t test_ctx[4] = {0, 0, 0, 0};
    
    FT_REPORT_FAULT_TEST(FT_FAULT_MEMORY_CORRUPTION, FT_SEVERITY_CRITICAL,
                         "Test memory corruption", test_ctx);
                         
    FT_REPORT_FAULT_TEST(FT_FAULT_DEADLOCK_DETECTED, FT_SEVERITY_HIGH,
                         "Test deadlock scenario", test_ctx);
                         
    FT_REPORT_FAULT_TEST(FT_FAULT_TIMING_VIOLATION, FT_SEVERITY_MEDIUM,
                         "Test timing violation", test_ctx);
    
    // Get test results
    struct ft_fault_stats stats;
    ft_get_stats(&stats);
    printk("Test completed: %d faults, %d recoveries\n",
           stats.total_faults, stats.successful_recoveries);
    
    // Disable test mode
    ft_set_test_mode(false);
}
```

---

## Error Handling

### Return Codes
- `0`: Success
- `-EINVAL`: Invalid parameter
- `-ENOTSUP`: Feature not supported/enabled  
- `-ENOMEM`: Memory allocation failed
- `-EBUSY`: Resource busy
- `-ENOENT`: Handler not found

### Best Practices
1. Always check return codes from `ft_init()`
2. Register handlers early in initialization
3. Use test mode for validation and testing
4. Monitor statistics regularly for system health
5. Implement custom handlers for critical fault types
6. Use appropriate severity levels for different fault conditions