# Zephyr RTOS Fault Tolerance Framework

## Overview

This comprehensive fault tolerance framework provides robust fault detection, reporting, and recovery mechanisms for Zephyr RTOS applications. It integrates seamlessly with Zephyr's existing subsystems while providing modular, configurable fault handling capabilities.

## Architecture

### Core Components

1. **Core Framework (`ft_core.h/c`)** - Main fault tolerance engine
2. **Thread Recovery Module (`ft_thread_recovery.h/c`)** - Specialized thread fault handling
3. **Framework Integration (`ft_framework.c`)** - Unified initialization
4. **Configuration System (`Kconfig`)** - Flexible build-time configuration

### Key Features

- **Modular Design**: Each component can be enabled/disabled independently
- **Priority-based Handler System**: Multiple handlers can be registered with priority ordering
- **Asynchronous Recovery**: Non-blocking fault recovery using work queues
- **Comprehensive Statistics**: Detailed fault tracking and reporting
- **Thread Recovery**: Automatic restart of faulted threads with configurable policies
- **Seamless Integration**: Works with Zephyr's existing fatal error handling

## API Reference

### Core Framework Functions

```c
// Initialize the complete fault tolerance framework
int fault_tolerance_init(const struct ft_config *config);

// Core framework functions
int ft_init(const struct ft_config *config);
int ft_register_handler(struct ft_handler *handler);
int ft_report_fault(const struct ft_fault_context *fault_ctx);
int ft_get_stats(struct ft_stats *stats);
```

### Fault Types

```c
enum ft_fault_type {
    FT_FAULT_STACK_OVERFLOW,      // Stack overflow detected
    FT_FAULT_MEMORY_CORRUPTION,   // Memory corruption detected
    FT_FAULT_DEADLOCK,            // Deadlock detected
    FT_FAULT_HARDWARE_ERROR,      // Hardware error detected
    FT_FAULT_TIMEOUT,             // Timeout exceeded
    FT_FAULT_ASSERTION_FAILED,    // Assertion failed
    FT_FAULT_WATCHDOG_TIMEOUT,    // Watchdog timeout
    FT_FAULT_THREAD_EXCEPTION,    // Thread exception
    FT_FAULT_RESOURCE_EXHAUSTION, // Resource exhaustion
    FT_FAULT_CUSTOM               // Custom fault type
};
```

### Recovery Actions

```c
enum ft_recovery_action {
    FT_RECOVERY_NONE,              // No recovery action
    FT_RECOVERY_RESTART_THREAD,    // Restart the faulted thread
    FT_RECOVERY_RESTART_SUBSYSTEM, // Restart entire subsystem
    FT_RECOVERY_SYSTEM_REBOOT,     // Reboot the system
    FT_RECOVERY_GRACEFUL_SHUTDOWN, // Graceful system shutdown
    FT_RECOVERY_CUSTOM             // Custom recovery handler
};
```

## Configuration Options

### Core Configuration

```kconfig
CONFIG_FAULT_TOLERANCE=y                    # Enable framework
CONFIG_FT_LOG_LEVEL=3                      # Logging level
CONFIG_FT_RECOVERY_THREAD_PRIORITY=5       # Recovery thread priority
CONFIG_FT_RECOVERY_STACK_SIZE=2048         # Recovery thread stack
CONFIG_FT_ENABLE_STATISTICS=y              # Enable statistics
CONFIG_FT_ENABLE_ASYNC_RECOVERY=y          # Async recovery
CONFIG_FT_MAX_RECOVERY_ATTEMPTS=3          # Max recovery attempts
```

### Module Configuration

```kconfig
CONFIG_FT_ENABLE_THREAD_RECOVERY=y         # Thread recovery module
CONFIG_FT_ENABLE_SYSTEM_MONITOR=y          # System monitoring
CONFIG_FT_ENABLE_FAULT_LOGGING=y           # Persistent logging
CONFIG_FT_ENABLE_CRITICAL_FAULT_REBOOT=y   # Auto reboot on critical faults
```

## Usage Examples

### Basic Setup

```c
#include <zephyr/fault_tolerance.h>

// Initialize framework
int ret = fault_tolerance_init(NULL);
if (ret != 0) {
    printk("Failed to initialize FT framework: %d\n", ret);
}
```

### Custom Fault Handler

```c
static enum ft_handler_result my_fault_handler(
    const struct ft_fault_context *fault_ctx,
    struct ft_recovery_context *recovery_ctx,
    void *user_data)
{
    printk("Handling fault: %s\n", 
           ft_get_fault_type_name(fault_ctx->fault_type));
    
    recovery_ctx->action = FT_RECOVERY_RESTART_THREAD;
    recovery_ctx->target_thread = fault_ctx->thread_id;
    
    return FT_HANDLER_HANDLED;
}

static struct ft_handler my_handler = {
    .fault_type = FT_FAULT_STACK_OVERFLOW,
    .handler = my_fault_handler,
    .priority = 10,
    .name = "my_handler"
};

// Register handler
ft_register_handler(&my_handler);
```

### Thread Registration for Recovery

```c
// Register thread for automatic recovery
k_tid_t thread_id = k_thread_create(...);
ft_thread_register(thread_id, entry_point, p1, p2, p3,
                  stack_size, priority, options, "my_thread");
```

### Fatal Error Handler Integration

```c
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
    if (reason == K_ERR_STACK_CHK_FAIL) {
        // Create fault context
        struct ft_fault_context ctx = {
            .fault_type = FT_FAULT_STACK_OVERFLOW,
            .severity = FT_SEVERITY_ERROR,
            .timestamp = k_uptime_get(),
            .thread_id = k_current_get(),
            .error_code = reason,
            .description = "Stack overflow detected",
            .esf = esf
        };
        
        // Report to framework
        ft_report_fault(&ctx);
    }
    
    k_fatal_halt(reason);
}
```

## File Structure

```
zephyr/
├── include/zephyr/
│   ├── fault_tolerance.h              # Main include header
│   └── fault_tolerance/
│       ├── ft_core.h                  # Core framework API
│       └── ft_thread_recovery.h       # Thread recovery API
│
└── subsys/fault_tolerance/
    ├── Kconfig                        # Configuration options
    ├── CMakeLists.txt                 # Build integration
    ├── ft_core.c                      # Core implementation
    ├── ft_framework.c                 # Framework initialization
    └── ft_thread_recovery.c           # Thread recovery implementation
```

## Integration with Zephyr

The framework integrates seamlessly with Zephyr through:

1. **Subsystem Integration**: Included in `subsys/Kconfig` and `subsys/CMakeLists.txt`
2. **Fatal Error Handling**: Works with existing `k_sys_fatal_error_handler`
3. **Work Queue System**: Uses Zephyr work queues for asynchronous recovery
4. **Memory Management**: Uses Zephyr heap and memory slabs
5. **Logging**: Integrates with Zephyr logging subsystem
6. **Threading**: Uses Zephyr thread APIs and synchronization primitives

## Testing

The framework includes a comprehensive test application (`ft_stack_overflow_ai`) that:

- Triggers real stack overflow faults
- Tests fault detection and reporting
- Validates recovery mechanisms
- Demonstrates framework integration
- Provides statistics and logging examples

## Benefits

1. **Robustness**: Automatic recovery from common fault conditions
2. **Observability**: Comprehensive fault statistics and logging
3. **Modularity**: Only include needed components
4. **Performance**: Asynchronous recovery minimizes system impact
5. **Flexibility**: Configurable policies and custom handlers
6. **Safety**: Graceful degradation and system protection

## Future Enhancements

The framework can be extended with:

- System monitoring module with watchdog integration
- Persistent fault logging to flash storage
- Memory leak detection and recovery
- Deadlock detection algorithms
- Communication fault handling
- Power management integration
- Hardware fault abstraction

This fault tolerance framework provides a solid foundation for building robust, safety-critical embedded systems with Zephyr RTOS.