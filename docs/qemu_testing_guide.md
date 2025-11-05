# QEMU Testing Environment Setup
## ECE753 Fault Tolerance Framework Evaluation

### Overview

This guide provides instructions for setting up QEMU emulation environments to test the enhanced Zephyr fault tolerance framework across different hardware architectures. QEMU allows comprehensive testing without requiring physical hardware, enabling evaluation of fault tolerance mechanisms on various processor architectures.

### Prerequisites

1. **Ubuntu 24.04.3 Desktop** (or compatible Linux distribution)
2. **Zephyr SDK** installed and configured
3. **West build system** properly set up
4. **QEMU** virtualization platform

### QEMU Installation

#### Install QEMU and Dependencies

```bash
# Update package manager
sudo apt update

# Install QEMU and related packages
sudo apt install qemu-system qemu-system-x86 qemu-system-arm qemu-system-misc
sudo apt install qemu-user-static binfmt-support

# Install additional tools
sudo apt install gdb-multiarch
sudo apt install bridge-utils net-tools

# Verify installation
qemu-system-x86_64 --version
qemu-system-arm --version
```

#### Install Zephyr-specific QEMU Requirements

```bash
# Install Python dependencies for Zephyr
pip3 install --user pyelftools
pip3 install --user pykwalify
pip3 install --user colorama

# Install additional build tools
sudo apt install device-tree-compiler
sudo apt install ninja-build
```

### Supported Architectures

The fault tolerance framework has been tested on the following QEMU-emulated architectures:

#### 1. x86 (32-bit and 64-bit)
- **Board**: `qemu_x86`, `qemu_x86_64`
- **Use Case**: General-purpose testing, baseline validation
- **Features**: Full fault tolerance feature set, debugging support

#### 2. ARM Cortex-M
- **Board**: `qemu_cortex_m3`, `qemu_cortex_m0plus`
- **Use Case**: Embedded microcontroller simulation
- **Features**: Stack monitoring, memory management, real-time testing

#### 3. ARM Cortex-A
- **Board**: `qemu_cortex_a9`
- **Use Case**: Application processor simulation
- **Features**: Multi-core support, advanced memory management

#### 4. RISC-V
- **Board**: `qemu_riscv32`, `qemu_riscv64`
- **Use Case**: Emerging architecture evaluation
- **Features**: Open architecture testing, custom instruction support

### Building for Different Architectures

#### x86 Architecture (Recommended for Development)

```bash
cd /home/jack/ece753-project/zephyr

# Build fast validation test for x86
west build -p auto -b qemu_x86 app/fast_validation

# Run in QEMU
west build -t run

# Build with debugging enabled
west build -p auto -b qemu_x86 app/fast_validation -- -DCONFIG_DEBUG=y

# Run with GDB debugging
west build -t debugserver
# In another terminal:
gdb build/zephyr/zephyr.elf
(gdb) target remote :1234
```

#### x86-64 Architecture (High Performance)

```bash
# Build for 64-bit x86
west build -p auto -b qemu_x86_64 app/stress_endurance

# Run stress test
west build -t run

# Monitor with enhanced logging
west build -t run | tee test_output.log
```

#### ARM Cortex-M3 (Embedded Systems)

```bash
# Build for Cortex-M3
west build -p auto -b qemu_cortex_m3 app/fast_validation

# Run with limited resources
west build -t run

# Custom QEMU parameters for memory constraints
west build -t run -- -m 64M
```

#### RISC-V (Emerging Architecture)

```bash
# Build for RISC-V 32-bit
west build -p auto -b qemu_riscv32 app/fast_validation

# Run RISC-V emulation
west build -t run

# Build for RISC-V 64-bit (if supported)
west build -p auto -b qemu_riscv64 app/fast_validation
```

### Custom QEMU Configurations

#### Memory Stress Testing Configuration

Create a custom board configuration for enhanced memory testing:

```bash
# Create custom overlay
cat > memory_stress_overlay.conf << EOF
CONFIG_HEAP_MEM_POOL_SIZE=65536
CONFIG_MAIN_STACK_SIZE=16384
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=8192
EOF

# Build with overlay
west build -p auto -b qemu_x86 app/stress_endurance -- -DOVERLAY_CONFIG=memory_stress_overlay.conf
```

#### Multi-Core Testing (where supported)

```bash
# Enable SMP for multi-core testing
cat > smp_overlay.conf << EOF
CONFIG_SMP=y
CONFIG_MP_MAX_NUM_CPUS=4
CONFIG_FAULT_TOLERANCE_SMP_SUPPORT=y
EOF

west build -p auto -b qemu_x86_64 app/stress_endurance -- -DOVERLAY_CONFIG=smp_overlay.conf
```

#### Real-Time Testing Configuration

```bash
# Enhanced real-time configuration
cat > realtime_overlay.conf << EOF
CONFIG_PREEMPT_ENABLED=y
CONFIG_TIMESLICING=y
CONFIG_TIMESLICE_SIZE=1
CONFIG_FAULT_TOLERANCE_TIMING_MONITORING=y
CONFIG_FAULT_TOLERANCE_MONITOR_INTERVAL_MS=10
EOF

west build -p auto -b qemu_cortex_m3 app/fast_validation -- -DOVERLAY_CONFIG=realtime_overlay.conf
```

### Test Execution Scripts

#### Automated Testing Script

```bash
#!/bin/bash
# automated_ft_test.sh

ARCHITECTURES=("qemu_x86" "qemu_x86_64" "qemu_cortex_m3" "qemu_riscv32")
APPLICATIONS=("fast_validation" "stress_endurance")

echo "=== Enhanced Zephyr Fault Tolerance Test Suite ==="
echo "Starting automated testing across multiple architectures"

for arch in "${ARCHITECTURES[@]}"; do
    echo ""
    echo "=== Testing Architecture: $arch ==="
    
    for app in "${APPLICATIONS[@]}"; do
        echo "--- Testing Application: $app ---"
        
        # Build application
        if west build -p auto -b $arch app/$app; then
            echo "Build successful for $arch/$app"
            
            # Run test with timeout (30 minutes max)
            timeout 1800 west build -t run > "test_${arch}_${app}.log" 2>&1
            
            if [ $? -eq 0 ]; then
                echo "Test completed successfully: $arch/$app"
            elif [ $? -eq 124 ]; then
                echo "Test timed out: $arch/$app (this is expected for stress tests)"
            else
                echo "Test failed: $arch/$app"
            fi
        else
            echo "Build failed for $arch/$app"
        fi
    done
done

echo ""
echo "=== Test Suite Complete ==="
echo "Results saved in test_*.log files"
```

#### Performance Monitoring Script

```bash
#!/bin/bash
# performance_monitor.sh

echo "=== Performance Monitoring for Fault Tolerance Framework ==="

# Build with performance monitoring
west build -p auto -b qemu_x86_64 app/stress_endurance -- \
    -DCONFIG_THREAD_RUNTIME_STATS=y \
    -DCONFIG_FAULT_TOLERANCE_DEBUG=y

# Run with monitoring
echo "Starting performance monitoring..."
timeout 600 west build -t run | tee performance_log.txt

# Extract performance metrics
echo ""
echo "=== Performance Summary ==="
grep -E "(CPU|Memory|Runtime|MTBF)" performance_log.txt | tail -20
```

### Debugging and Analysis

#### GDB Debugging Setup

```bash
# Terminal 1: Start GDB server
west build -t debugserver

# Terminal 2: Connect GDB
arm-none-eabi-gdb build/zephyr/zephyr.elf  # For ARM targets
# OR
gdb build/zephyr/zephyr.elf                # For x86 targets

# GDB commands for fault tolerance debugging
(gdb) target remote :1234
(gdb) break ft_report_fault
(gdb) break ft_execute_recovery
(gdb) continue
```

#### Log Analysis

```bash
# Extract fault statistics from logs
grep -E "(FAULT|Recovery|Statistics)" test_output.log > fault_analysis.txt

# Count fault types
grep "FAULT DETECTED" test_output.log | cut -d: -f3 | sort | uniq -c

# Calculate recovery success rate
TOTAL_FAULTS=$(grep -c "FAULT DETECTED" test_output.log)
SUCCESSFUL_RECOVERIES=$(grep -c "Recovery successful" test_output.log)
echo "Recovery Rate: $((SUCCESSFUL_RECOVERIES * 100 / TOTAL_FAULTS))%"
```

### Hardware-in-the-Loop Testing

For validation on real hardware, the framework supports these development boards:

#### ARM-based Boards
- **STM32 Discovery boards** (STM32F4, STM32F7, STM32H7)
- **Nordic nRF52 series** (nRF52840, nRF52833)
- **NXP i.MX RT series** (MIMXRT1060, MIMXRT1064)

#### x86-based Boards
- **UP Squared** (Intel Atom x7-E3950)
- **Intel Edison** (Dual-core Intel Atom)

#### RISC-V Boards
- **SiFive HiFive1** (FE310-G000)
- **Kendryte K210** (RISC-V 64-bit dual-core)

### Troubleshooting

#### Common Issues and Solutions

1. **QEMU fails to start**
   ```bash
   # Check QEMU installation
   which qemu-system-x86_64
   
   # Verify KVM acceleration (optional)
   sudo modprobe kvm-intel  # For Intel CPUs
   sudo modprobe kvm-amd    # For AMD CPUs
   ```

2. **Build failures**
   ```bash
   # Clean build directory
   west build -t pristine
   
   # Update Zephyr and modules
   west update
   
   # Check toolchain
   west zephyr-export
   ```

3. **Memory allocation failures in tests**
   ```bash
   # Increase QEMU memory
   export QEMU_EXTRA_FLAGS="-m 512M"
   west build -t run
   ```

4. **Timing test failures**
   ```bash
   # Disable host system sleep/suspend
   sudo systemctl mask sleep.target suspend.target hibernate.target
   
   # Use dedicated CPU cores
   sudo cpupower frequency-set -g performance
   ```

### Results Collection

#### Automated Results Gathering

```bash
#!/bin/bash
# collect_results.sh

RESULTS_DIR="fault_tolerance_results_$(date +%Y%m%d_%H%M%S)"
mkdir -p $RESULTS_DIR

echo "Collecting fault tolerance test results..."

# Copy log files
cp test_*.log $RESULTS_DIR/ 2>/dev/null

# Generate summary report
cat > $RESULTS_DIR/summary.txt << EOF
Fault Tolerance Framework Test Results
Generated: $(date)
Host System: $(uname -a)
Zephyr Version: $(west list zephyr | grep zephyr)

Architecture Test Summary:
$(for log in test_*.log; do
    arch=$(echo $log | cut -d_ -f2)
    app=$(echo $log | cut -d_ -f3 | cut -d. -f1)
    if grep -q "TEST COMPLETE" $log; then
        echo "$arch/$app: PASS"
    else
        echo "$arch/$app: FAIL"
    fi
done)

Performance Metrics:
$(grep -h "Recovery Success Rate\|MTBF\|Memory" test_*.log | sort | uniq)
EOF

echo "Results collected in $RESULTS_DIR/"
```

### Conclusion

This QEMU testing environment provides comprehensive evaluation capabilities for the enhanced Zephyr fault tolerance framework. By testing across multiple architectures and configurations, developers can validate the framework's effectiveness in diverse embedded system scenarios.

The automated testing scripts and monitoring tools enable systematic evaluation and performance analysis, supporting the research objectives of demonstrating improved fault tolerance in safety-critical embedded systems.