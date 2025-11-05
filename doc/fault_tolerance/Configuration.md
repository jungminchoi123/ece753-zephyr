# Fault Tolerance Framework Configuration Guide

**ECE753 Project - Configuration Documentation**  
**Version:** 1.0

## Table of Contents

1. [Basic Configuration](#basic-configuration)
2. [Kconfig Options](#kconfig-options)
3. [Build Integration](#build-integration)
4. [Resource Configuration](#resource-configuration)
5. [Feature-Specific Settings](#feature-specific-settings)
6. [Performance Tuning](#performance-tuning)
7. [Platform-Specific Notes](#platform-specific-notes)

## Basic Configuration

### Minimum Configuration

To enable the fault tolerance framework, add these lines to your `prj.conf`:

```kconfig
# Enable fault tolerance framework
CONFIG_FAULT_TOLERANCE=y

# Enable basic features
CONFIG_FAULT_TOLERANCE_STATISTICS=y
CONFIG_FAULT_TOLERANCE_AUTO_RECOVERY=y

# Enable logging
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
```

### Recommended Configuration

For production systems, use this comprehensive configuration:

```kconfig
# Core framework
CONFIG_FAULT_TOLERANCE=y
CONFIG_FAULT_TOLERANCE_STATISTICS=y
CONFIG_FAULT_TOLERANCE_AUTO_RECOVERY=y

# Monitoring features
CONFIG_FAULT_TOLERANCE_STACK_MONITORING=y
CONFIG_FAULT_TOLERANCE_MEMORY_MONITORING=y
CONFIG_FAULT_TOLERANCE_TIMING_MONITORING=y
CONFIG_FAULT_TOLERANCE_DEADLOCK_DETECTION=y

# Integration features
CONFIG_FAULT_TOLERANCE_WATCHDOG_INTEGRATION=y

# Resource limits (adjust based on available memory)
CONFIG_FAULT_TOLERANCE_MAX_LOG_ENTRIES=128
CONFIG_FAULT_TOLERANCE_MAX_HANDLERS_PER_TYPE=4
CONFIG_FAULT_TOLERANCE_MAX_RECOVERY_CALLBACKS=8

# System requirements
CONFIG_MULTITHREADING=y
CONFIG_HEAP_MEM_POOL_SIZE=8192
CONFIG_MAIN_STACK_SIZE=4096
```

---

## Kconfig Options

### Core Framework Options

#### `CONFIG_FAULT_TOLERANCE`
**Type**: bool  
**Default**: n  
**Description**: Enable the fault tolerance framework subsystem.

**Dependencies**: 
- `CONFIG_MULTITHREADING=y`
- `CONFIG_HEAP_MEM_POOL_SIZE >= 4096`

**Usage**:
```kconfig
CONFIG_FAULT_TOLERANCE=y
```

#### `CONFIG_FAULT_TOLERANCE_STATISTICS`
**Type**: bool  
**Default**: n  
**Description**: Enable collection of fault and recovery statistics.

**Dependencies**: `CONFIG_FAULT_TOLERANCE=y`

**Benefits**:
- Runtime monitoring of system health
- Performance analysis capabilities
- Debugging and diagnostic information

#### `CONFIG_FAULT_TOLERANCE_AUTO_RECOVERY`
**Type**: bool  
**Default**: y  
**Description**: Enable automatic recovery actions for detected faults.

**Note**: When disabled, faults are logged but no recovery actions are taken automatically.

### Monitoring Features

#### `CONFIG_FAULT_TOLERANCE_STACK_MONITORING`
**Type**: bool  
**Default**: y if `CONFIG_THREAD_STACK_INFO=y`  
**Description**: Enable thread stack overflow monitoring.

**Resource Impact**: 
- Minimal CPU overhead
- No additional memory usage

**Configuration Example**:
```kconfig
CONFIG_FAULT_TOLERANCE_STACK_MONITORING=y
CONFIG_THREAD_STACK_INFO=y  # Required dependency
```

#### `CONFIG_FAULT_TOLERANCE_MEMORY_MONITORING`
**Type**: bool  
**Default**: y if `CONFIG_HEAP_MEM_POOL_SIZE > 0`  
**Description**: Enable memory leak detection and heap monitoring.

**Features**:
- Allocation tracking
- Leak detection
- Heap exhaustion monitoring
- Memory corruption detection

#### `CONFIG_FAULT_TOLERANCE_TIMING_MONITORING`
**Type**: bool  
**Default**: n  
**Description**: Enable real-time timing constraint monitoring.

**Use Cases**:
- Real-time systems
- Deadline-sensitive applications
- Performance monitoring

#### `CONFIG_FAULT_TOLERANCE_DEADLOCK_DETECTION`
**Type**: bool  
**Default**: n  
**Description**: Enable deadlock detection algorithms.

**Resource Impact**: 
- Moderate CPU overhead during mutex operations
- Additional memory for dependency tracking

### Integration Options

#### `CONFIG_FAULT_TOLERANCE_WATCHDOG_INTEGRATION`
**Type**: bool  
**Default**: y if `CONFIG_WATCHDOG=y`  
**Description**: Integrate with system watchdog for fault recovery.

**Benefits**:
- Hardware-level fault recovery
- System reset capability
- Enhanced reliability

### Resource Configuration

#### `CONFIG_FAULT_TOLERANCE_MAX_LOG_ENTRIES`
**Type**: int  
**Range**: 16-512  
**Default**: 64  
**Description**: Maximum number of fault log entries to maintain.

**Memory Impact**: ~32 bytes per entry

**Sizing Guidelines**:
- **Small systems (< 64KB RAM)**: 16-32 entries
- **Medium systems (64-512KB RAM)**: 64-128 entries  
- **Large systems (> 512KB RAM)**: 128-512 entries

#### `CONFIG_FAULT_TOLERANCE_MAX_HANDLERS_PER_TYPE`
**Type**: int  
**Range**: 1-16  
**Default**: 4  
**Description**: Maximum fault handlers per fault type.

**Usage**:
```kconfig
# Allow multiple handlers per fault type for complex recovery
CONFIG_FAULT_TOLERANCE_MAX_HANDLERS_PER_TYPE=8
```

#### `CONFIG_FAULT_TOLERANCE_MAX_RECOVERY_CALLBACKS`
**Type**: int  
**Range**: 1-32  
**Default**: 8  
**Description**: Maximum number of recovery completion callbacks.

---

## Build Integration

### CMakeLists.txt Integration

The framework automatically integrates with Zephyr's build system. No manual CMake configuration is required.

```cmake
# Your application CMakeLists.txt
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(my_application)

target_sources(app PRIVATE src/main.c)

# Framework automatically included when CONFIG_FAULT_TOLERANCE=y
```

### Manual Integration (Advanced)

For custom integration or standalone builds:

```cmake
# Include fault tolerance subsystem
target_include_directories(app PRIVATE 
    ${ZEPHYR_BASE}/include/fault_tolerance
)

target_sources(app PRIVATE
    ${ZEPHYR_BASE}/subsys/fault_tolerance/fault_tolerance.c
)

target_compile_definitions(app PRIVATE
    -DCONFIG_FAULT_TOLERANCE=1
    -DCONFIG_FAULT_TOLERANCE_STATISTICS=1
)
```

---

## Feature-Specific Settings

### Memory-Constrained Systems

For systems with limited memory (< 32KB RAM):

```kconfig
# Minimal configuration
CONFIG_FAULT_TOLERANCE=y
CONFIG_FAULT_TOLERANCE_STATISTICS=n
CONFIG_FAULT_TOLERANCE_AUTO_RECOVERY=y

# Reduced limits
CONFIG_FAULT_TOLERANCE_MAX_LOG_ENTRIES=16
CONFIG_FAULT_TOLERANCE_MAX_HANDLERS_PER_TYPE=2
CONFIG_FAULT_TOLERANCE_MAX_RECOVERY_CALLBACKS=4

# Disable memory-intensive features
CONFIG_FAULT_TOLERANCE_MEMORY_MONITORING=n
CONFIG_FAULT_TOLERANCE_DEADLOCK_DETECTION=n

# System settings
CONFIG_HEAP_MEM_POOL_SIZE=2048
CONFIG_MAIN_STACK_SIZE=1024
```

### Real-Time Systems

For hard real-time applications:

```kconfig
# Enable timing features
CONFIG_FAULT_TOLERANCE=y
CONFIG_FAULT_TOLERANCE_TIMING_MONITORING=y
CONFIG_FAULT_TOLERANCE_AUTO_RECOVERY=y

# Optimize for performance
CONFIG_FAULT_TOLERANCE_STATISTICS=y
CONFIG_FAULT_TOLERANCE_MAX_LOG_ENTRIES=32

# Real-time kernel settings
CONFIG_PREEMPT_ENABLED=y
CONFIG_TIMESLICING=n
CONFIG_THREAD_RUNTIME_STATS=y
```

### Safety-Critical Systems

For safety-critical applications requiring maximum reliability:

```kconfig
# Enable all monitoring features
CONFIG_FAULT_TOLERANCE=y
CONFIG_FAULT_TOLERANCE_STATISTICS=y
CONFIG_FAULT_TOLERANCE_AUTO_RECOVERY=y
CONFIG_FAULT_TOLERANCE_STACK_MONITORING=y
CONFIG_FAULT_TOLERANCE_MEMORY_MONITORING=y
CONFIG_FAULT_TOLERANCE_TIMING_MONITORING=y
CONFIG_FAULT_TOLERANCE_DEADLOCK_DETECTION=y
CONFIG_FAULT_TOLERANCE_WATCHDOG_INTEGRATION=y

# Maximum logging and handlers
CONFIG_FAULT_TOLERANCE_MAX_LOG_ENTRIES=256
CONFIG_FAULT_TOLERANCE_MAX_HANDLERS_PER_TYPE=8
CONFIG_FAULT_TOLERANCE_MAX_RECOVERY_CALLBACKS=16

# Enhanced system monitoring
CONFIG_THREAD_STACK_INFO=y
CONFIG_THREAD_RUNTIME_STATS=y
CONFIG_KERNEL_DEBUG_INFO=y
CONFIG_STACK_SENTINEL=y
```

---

## Performance Tuning

### CPU Usage Optimization

#### Monitoring Frequency
```kconfig
# Reduce monitoring overhead for performance-critical applications
CONFIG_FAULT_TOLERANCE_MONITOR_INTERVAL_MS=1000  # Check every second instead of 100ms
```

#### Handler Optimization
- Register only essential fault handlers
- Use lightweight recovery actions where possible
- Avoid complex operations in fault handlers

### Memory Usage Optimization

#### Log Entry Management
```kconfig
# Circular buffer for log entries (overwrites oldest)
CONFIG_FAULT_TOLERANCE_CIRCULAR_LOG=y

# Reduce memory footprint
CONFIG_FAULT_TOLERANCE_MAX_LOG_ENTRIES=32
CONFIG_FAULT_TOLERANCE_COMPACT_LOGS=y
```

#### Stack Usage
```kconfig
# Optimize stack sizes for fault handling
CONFIG_FAULT_TOLERANCE_HANDLER_STACK_SIZE=1024
CONFIG_FAULT_TOLERANCE_MONITOR_STACK_SIZE=512
```

---

## Platform-Specific Notes

### QEMU x86 (Development/Testing)
```kconfig
# Development configuration
CONFIG_FAULT_TOLERANCE=y
CONFIG_FAULT_TOLERANCE_STATISTICS=y
CONFIG_FAULT_TOLERANCE_AUTO_RECOVERY=y

# Enable all monitoring for testing
CONFIG_FAULT_TOLERANCE_STACK_MONITORING=y
CONFIG_FAULT_TOLERANCE_MEMORY_MONITORING=y
CONFIG_FAULT_TOLERANCE_TIMING_MONITORING=y
CONFIG_FAULT_TOLERANCE_DEADLOCK_DETECTION=y

# Generous resource allocation for development
CONFIG_HEAP_MEM_POOL_SIZE=32768
CONFIG_MAIN_STACK_SIZE=8192
CONFIG_FAULT_TOLERANCE_MAX_LOG_ENTRIES=128
```

### ARM Cortex-M (Embedded)
```kconfig
# Optimized for embedded ARM platforms
CONFIG_FAULT_TOLERANCE=y
CONFIG_FAULT_TOLERANCE_AUTO_RECOVERY=y
CONFIG_FAULT_TOLERANCE_STACK_MONITORING=y
CONFIG_FAULT_TOLERANCE_WATCHDOG_INTEGRATION=y

# ARM-specific optimizations
CONFIG_ARM_MPU=y  # Memory protection unit
CONFIG_HW_STACK_PROTECTION=y
CONFIG_USERSPACE=n  # Disable for simpler configuration

# Conservative resource allocation
CONFIG_HEAP_MEM_POOL_SIZE=8192
CONFIG_FAULT_TOLERANCE_MAX_LOG_ENTRIES=64
```

### ESP32 (Wi-Fi/Bluetooth Applications)
```kconfig
# ESP32-specific configuration
CONFIG_FAULT_TOLERANCE=y
CONFIG_FAULT_TOLERANCE_AUTO_RECOVERY=y
CONFIG_FAULT_TOLERANCE_MEMORY_MONITORING=y

# ESP32 has dual core - enable SMP features
CONFIG_SMP=y
CONFIG_MP_NUM_CPUS=2

# Wi-Fi/BT applications need more memory
CONFIG_HEAP_MEM_POOL_SIZE=16384
CONFIG_MAIN_STACK_SIZE=4096
```

---

## Validation Configuration

### For Testing and Validation
```kconfig
# Enable comprehensive testing features
CONFIG_FAULT_TOLERANCE=y
CONFIG_FAULT_TOLERANCE_STATISTICS=y
CONFIG_FAULT_TOLERANCE_AUTO_RECOVERY=y

# Enable all monitoring for complete coverage
CONFIG_FAULT_TOLERANCE_STACK_MONITORING=y
CONFIG_FAULT_TOLERANCE_MEMORY_MONITORING=y
CONFIG_FAULT_TOLERANCE_TIMING_MONITORING=y
CONFIG_FAULT_TOLERANCE_DEADLOCK_DETECTION=y
CONFIG_FAULT_TOLERANCE_WATCHDOG_INTEGRATION=y

# Maximum logging for analysis
CONFIG_FAULT_TOLERANCE_MAX_LOG_ENTRIES=256
CONFIG_FAULT_TOLERANCE_MAX_HANDLERS_PER_TYPE=8
CONFIG_FAULT_TOLERANCE_MAX_RECOVERY_CALLBACKS=16

# Enhanced debugging
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=4  # Debug level
CONFIG_LOG_MODE_IMMEDIATE=y
CONFIG_LOG_BUFFER_SIZE=4096

# System monitoring
CONFIG_THREAD_STACK_INFO=y
CONFIG_THREAD_RUNTIME_STATS=y
CONFIG_KERNEL_DEBUG_INFO=y
CONFIG_ASSERT=y
```

---

## Troubleshooting Configuration Issues

### Common Build Errors

#### "fault_tolerance.h: No such file or directory"
**Solution**: Ensure `CONFIG_FAULT_TOLERANCE=y` is set in `prj.conf`

#### "undefined reference to ft_init"
**Solutions**:
1. Verify framework is enabled: `CONFIG_FAULT_TOLERANCE=y`
2. Check include path: `#include <fault_tolerance/fault_tolerance.h>`
3. Ensure proper linking in CMakeLists.txt

#### Memory allocation failures during runtime
**Solutions**:
1. Increase heap size: `CONFIG_HEAP_MEM_POOL_SIZE=16384`
2. Reduce log entries: `CONFIG_FAULT_TOLERANCE_MAX_LOG_ENTRIES=32`
3. Check stack sizes: `CONFIG_MAIN_STACK_SIZE=4096`

### Runtime Configuration Issues

#### Framework initialization fails
**Debug Steps**:
```c
int ret = ft_init();
if (ret != 0) {
    printk("FT init failed: %d\n", ret);
    // Check return code meaning:
    // -ENOTSUP: Framework not enabled in Kconfig
    // -ENOMEM: Insufficient memory
}
```

#### No fault detection occurring
**Verification**:
1. Ensure monitoring features are enabled
2. Check that handlers are registered
3. Verify test mode is disabled for production faults

#### High memory usage
**Solutions**:
1. Reduce `CONFIG_FAULT_TOLERANCE_MAX_LOG_ENTRIES`
2. Disable unused monitoring features
3. Use compact logging: `CONFIG_FAULT_TOLERANCE_COMPACT_LOGS=y`

---

## Best Practices

### Configuration Management
1. **Use separate configurations** for development, testing, and production
2. **Document configuration choices** and their rationale
3. **Test configuration changes** thoroughly before deployment
4. **Monitor resource usage** with different configurations

### Performance Considerations
1. **Enable only needed features** to minimize overhead
2. **Tune monitoring intervals** based on application requirements
3. **Use appropriate log levels** for production systems
4. **Consider platform limitations** when setting resource limits

### Safety and Reliability
1. **Enable watchdog integration** for critical systems
2. **Use comprehensive monitoring** during development and testing
3. **Implement custom handlers** for application-specific faults
4. **Regular validation** of configuration effectiveness

---

## Configuration Examples

### Example 1: Minimal IoT Device
```kconfig
# 32KB RAM, battery-powered sensor node
CONFIG_FAULT_TOLERANCE=y
CONFIG_FAULT_TOLERANCE_AUTO_RECOVERY=y
CONFIG_FAULT_TOLERANCE_STATISTICS=n
CONFIG_FAULT_TOLERANCE_STACK_MONITORING=y
CONFIG_FAULT_TOLERANCE_WATCHDOG_INTEGRATION=y

CONFIG_HEAP_MEM_POOL_SIZE=2048
CONFIG_FAULT_TOLERANCE_MAX_LOG_ENTRIES=16
CONFIG_LOG=n  # Save power
```

### Example 2: Industrial Controller
```kconfig
# 256KB RAM, real-time industrial application
CONFIG_FAULT_TOLERANCE=y
CONFIG_FAULT_TOLERANCE_STATISTICS=y
CONFIG_FAULT_TOLERANCE_AUTO_RECOVERY=y
CONFIG_FAULT_TOLERANCE_STACK_MONITORING=y
CONFIG_FAULT_TOLERANCE_MEMORY_MONITORING=y
CONFIG_FAULT_TOLERANCE_TIMING_MONITORING=y
CONFIG_FAULT_TOLERANCE_WATCHDOG_INTEGRATION=y

CONFIG_HEAP_MEM_POOL_SIZE=16384
CONFIG_FAULT_TOLERANCE_MAX_LOG_ENTRIES=128
CONFIG_PREEMPT_ENABLED=y
CONFIG_TIMESLICING=n
```

### Example 3: Automotive ECU
```kconfig
# 1MB RAM, safety-critical automotive application
CONFIG_FAULT_TOLERANCE=y
CONFIG_FAULT_TOLERANCE_STATISTICS=y
CONFIG_FAULT_TOLERANCE_AUTO_RECOVERY=y
CONFIG_FAULT_TOLERANCE_STACK_MONITORING=y
CONFIG_FAULT_TOLERANCE_MEMORY_MONITORING=y
CONFIG_FAULT_TOLERANCE_TIMING_MONITORING=y
CONFIG_FAULT_TOLERANCE_DEADLOCK_DETECTION=y
CONFIG_FAULT_TOLERANCE_WATCHDOG_INTEGRATION=y

CONFIG_HEAP_MEM_POOL_SIZE=65536
CONFIG_FAULT_TOLERANCE_MAX_LOG_ENTRIES=256
CONFIG_FAULT_TOLERANCE_MAX_HANDLERS_PER_TYPE=8
CONFIG_HW_STACK_PROTECTION=y
CONFIG_ARM_MPU=y
```