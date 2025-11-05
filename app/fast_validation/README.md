# Fast Fault Tolerance Validation Test

This application provides a quick validation of the enhanced fault tolerance framework for Zephyr RTOS. It systematically tests various fault detection and recovery mechanisms.

## Test Coverage

### 1. Stack Overflow Detection
- Creates a thread with limited stack size
- Intentionally overflows the stack with large local variables
- Validates stack overflow detection and recovery

### 2. Memory Leak Detection
- Simulates memory allocations without corresponding deallocations
- Tests memory leak tracking and detection
- Validates memory usage monitoring

### 3. Resource Exhaustion
- Simulates resource usage approaching limits
- Tests resource exhaustion detection
- Validates appropriate recovery actions

### 4. Race Condition Detection
- Simulates race condition scenarios
- Tests detection mechanisms
- Validates recovery procedures

### 5. Data Corruption Detection
- Simulates data integrity violations
- Tests corruption detection
- Validates data recovery mechanisms

### 6. Timing Violation Detection
- Simulates deadline misses
- Tests timing constraint monitoring
- Validates temporal fault handling

## Test Duration
- Default: 30 seconds
- Configurable via TEST_DURATION_MS

## Expected Outputs
- Real-time test progress reporting
- Fault injection and recovery statistics
- Memory usage monitoring
- System health metrics
- Final test results summary

## Success Criteria
- All injected faults should be detected
- Recovery mechanisms should execute successfully
- System should remain stable throughout testing
- No unhandled exceptions or crashes

## Usage
```bash
west build -p auto -b qemu_x86 app/fast_validation
west build -t run
```

## Configuration
- Adjust test parameters in main.c
- Modify fault sensitivity in prj.conf
- Enable/disable specific fault types as needed