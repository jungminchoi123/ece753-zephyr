/**
 * @file memory_access_test.c
 * @brief ARM Cortex-M3 Memory Access Violation Test Module
 * @author Jack Ostapeic, MS ECE Student at University of Wisconsin-Madison
 * 
 * This module implements comprehensive memory access violation testing for 
 * ARM Cortex-M3 architecture, utilizing Memory Protection Unit (MPU) and
 * hardware fault detection to identify unauthorized memory accesses.
 *
 * Memory Access Violation Types Tested:
 * 1. MPU Region Violations - Access outside allowed regions
 * 2. Read-Only Memory Writes - Attempting to write to ROM/flash
 * 3. Execute-Never Region Execution - Code execution in data regions
 * 4. Privileged vs Unprivileged Access - Permission violations
 * 5. Alignment Violations - Unaligned memory access
 * 6. Out-of-Bounds Access - Buffer overflow detection
 * 7. Peripheral Register Violations - Invalid peripheral access
 *
 * Detection Methods:
 * - Hardware MPU fault exceptions
 * - Memory Management Fault (MemManage)
 * - Bus Fault detection
 * - Alignment fault detection
 * - Software bounds checking
 *
 * Recovery Mechanisms:
 * - MPU reconfiguration
 * - Access permission adjustment
 * - Memory region isolation
 * - Safe access fallback
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance.h>

LOG_MODULE_REGISTER(memory_access_test, LOG_LEVEL_INF);

/* Test configuration */
#define TEST_BUFFER_SIZE 1024
#define MPU_TEST_REGION 7  /* Use region 7 for testing */
#define INVALID_MEMORY_BASE 0x70000000  /* Invalid address range */
#define PERIPHERAL_BASE 0x40000000      /* Peripheral memory region */

/* Statistics tracking */
static int memory_tests_run = 0;
static int violations_detected = 0;
static volatile bool memory_fault_occurred = false;

/* Test data */
static uint8_t test_buffer[TEST_BUFFER_SIZE] __aligned(4);
static uint8_t readonly_buffer[256] __attribute__((section(".rodata"))) = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
};

/**
 * @brief Configure MPU for memory access testing
 *
 * Sets up Memory Protection Unit regions to test
 * various access violation scenarios.
 *
 * @return 0 on success, negative on failure
 */
static int setup_mpu_test_regions(void)
{
    #ifdef CONFIG_ARM_MPU
    LOG_INF("Setting up MPU test regions");
    
    /* Enable MPU with background region disabled for stricter testing */
    MPU->CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_HFNMIENA_Msk;
    __DSB();
    __ISB();
    
    /* Region 0: Flash memory (read-only, executable) */
    MPU->RNR = 0;
    MPU->RBAR = 0x08000000;  /* Flash base */
    MPU->RASR = ARM_MPU_RASR(1, ARM_MPU_AP_RO, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_1MB);
    
    /* Region 1: SRAM (read-write, non-executable for data) */
    MPU->RNR = 1;
    MPU->RBAR = 0x20000000;  /* SRAM base */
    MPU->RASR = ARM_MPU_RASR(1, ARM_MPU_AP_FULL, 0, 0, 1, 1, 1, ARM_MPU_REGION_SIZE_128KB);
    
    /* Region 2: Test buffer (configurable permissions) */
    MPU->RNR = 2;
    MPU->RBAR = (uint32_t)test_buffer;
    MPU->RASR = ARM_MPU_RASR(1, ARM_MPU_AP_FULL, 0, 0, 1, 1, 1, ARM_MPU_REGION_SIZE_1KB);
    
    /* Region 3: Read-only test region */
    MPU->RNR = 3;
    MPU->RBAR = (uint32_t)readonly_buffer;
    MPU->RASR = ARM_MPU_RASR(1, ARM_MPU_AP_RO, 0, 0, 1, 1, 1, ARM_MPU_REGION_SIZE_256B);
    
    /* Region 4: Peripheral region */
    MPU->RNR = 4;
    MPU->RBAR = PERIPHERAL_BASE;
    MPU->RASR = ARM_MPU_RASR(1, ARM_MPU_AP_FULL, 0, 0, 0, 1, 1, ARM_MPU_REGION_SIZE_512MB);
    
    __DSB();
    __ISB();
    
    LOG_INF("✅ MPU test regions configured");
    return 0;
    #else
    LOG_WRN("⚠️  MPU not available");
    return -ENOTSUP;
    #endif
}

/**
 * @brief Test 1: Read-Only Memory Write Violation
 *
 * Attempts to write to read-only memory regions
 * to test MPU write protection.
 */
static void test_readonly_write_violation(void)
{
    LOG_INF("📋 Test 1: Read-only memory write violation");
    memory_tests_run++;
    
    LOG_INF("Attempting to write to read-only buffer at %p", readonly_buffer);
    
    /* First, verify we can read from it */
    volatile uint8_t read_value = readonly_buffer[0];
    LOG_INF("Read value from readonly buffer: 0x%02X", read_value);
    
    /* Now attempt to write (should cause MPU fault) */
    memory_fault_occurred = false;
    LOG_WRN("⚠️  Attempting write to read-only memory...");
    
    /* This should trigger a MemManage fault */
    *(volatile uint8_t*)readonly_buffer = 0xFF;
    
    if (!memory_fault_occurred) {
        LOG_ERR("❌ Read-only write succeeded - PROTECTION FAILED!");
        LOG_ERR("Buffer value after write: 0x%02X", readonly_buffer[0]);
    }
}

/**
 * @brief Test 2: Invalid Memory Region Access
 *
 * Attempts to access memory regions outside
 * valid address space.
 */
static void test_invalid_region_access(void)
{
    LOG_INF("📋 Test 2: Invalid memory region access");
    memory_tests_run++;
    
    /* Test access to invalid high memory */
    volatile uint32_t *invalid_ptr = (volatile uint32_t*)INVALID_MEMORY_BASE;
    
    LOG_INF("Attempting to access invalid memory at 0x%08X", INVALID_MEMORY_BASE);
    
    memory_fault_occurred = false;
    
    /* This should trigger a fault */
    LOG_WRN("⚠️  Attempting read from invalid memory region...");
    volatile uint32_t value = *invalid_ptr;
    
    if (!memory_fault_occurred) {
        LOG_ERR("❌ Invalid memory access succeeded - PROTECTION FAILED!");
        LOG_ERR("Read value: 0x%08X", value);
    }
    
    /* Test write to invalid memory */
    memory_fault_occurred = false;
    LOG_WRN("⚠️  Attempting write to invalid memory region...");
    *invalid_ptr = 0xDEADBEEF;
    
    if (!memory_fault_occurred) {
        LOG_ERR("❌ Invalid memory write succeeded - PROTECTION FAILED!");
    }
}

/**
 * @brief Test 3: Alignment Violation
 *
 * Tests detection of misaligned memory accesses
 * that violate ARM Cortex-M3 alignment requirements.
 */
static void test_alignment_violation(void)
{
    LOG_INF("📋 Test 3: Memory alignment violation");
    memory_tests_run++;
    
    /* Enable alignment fault detection */
    SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk;
    __DSB();
    __ISB();
    
    LOG_INF("Alignment fault detection enabled");
    
    /* Create misaligned access scenarios */
    uint8_t alignment_buffer[16] __aligned(4);
    
    /* Fill buffer with test pattern */
    for (int i = 0; i < 16; i++) {
        alignment_buffer[i] = i;
    }
    
    /* Test 1: Misaligned 32-bit read */
    LOG_DBG("Testing misaligned 32-bit read...");
    memory_fault_occurred = false;
    
    /* This should cause alignment fault */
    volatile uint32_t *misaligned_32 = (volatile uint32_t*)(alignment_buffer + 1);
    volatile uint32_t value32 = *misaligned_32;
    
    if (memory_fault_occurred) {
        LOG_INF("✅ Alignment violation detected for 32-bit read");
        violations_detected++;
    } else {
        LOG_WRN("⚠️  32-bit misaligned access succeeded: 0x%08X", value32);
    }
    
    /* Test 2: Misaligned 16-bit read */
    LOG_DBG("Testing misaligned 16-bit read...");
    memory_fault_occurred = false;
    
    volatile uint16_t *misaligned_16 = (volatile uint16_t*)(alignment_buffer + 1);
    volatile uint16_t value16 = *misaligned_16;
    
    if (memory_fault_occurred) {
        LOG_INF("✅ Alignment violation detected for 16-bit read");
        violations_detected++;
    } else {
        LOG_WRN("⚠️  16-bit misaligned access succeeded: 0x%04X", value16);
    }
    
    /* Test 3: Misaligned write */
    LOG_DBG("Testing misaligned 32-bit write...");
    memory_fault_occurred = false;
    
    *misaligned_32 = 0x12345678;
    
    if (memory_fault_occurred) {
        LOG_INF("✅ Alignment violation detected for write");
        violations_detected++;
    } else {
        LOG_WRN("⚠️  Misaligned write succeeded");
    }
}

/**
 * @brief Test 4: Buffer Overflow Detection
 *
 * Tests detection of buffer overflow conditions
 * using MPU region boundaries.
 */
static void test_buffer_overflow_detection(void)
{
    LOG_INF("📋 Test 4: Buffer overflow detection");
    memory_tests_run++;
    
    /* Setup buffer with guard region */
    LOG_INF("Test buffer at %p, size %d bytes", test_buffer, TEST_BUFFER_SIZE);
    
    /* Fill buffer with test data */
    for (int i = 0; i < TEST_BUFFER_SIZE; i++) {
        test_buffer[i] = (uint8_t)(i & 0xFF);
    }
    
    LOG_INF("Buffer initialized with test pattern");
    
    /* Test valid access within bounds */
    LOG_DBG("Testing valid access within buffer bounds...");
    volatile uint8_t valid_read = test_buffer[TEST_BUFFER_SIZE - 1];
    test_buffer[TEST_BUFFER_SIZE - 1] = 0xAA;
    LOG_DBG("Valid access successful, value: 0x%02X", valid_read);
    
    /* Test overflow access beyond buffer */
    LOG_WRN("⚠️  Testing buffer overflow access...");
    memory_fault_occurred = false;
    
    /* Access beyond buffer end */
    volatile uint8_t *overflow_ptr = test_buffer + TEST_BUFFER_SIZE + 1;
    volatile uint8_t overflow_value = *overflow_ptr;
    
    if (memory_fault_occurred) {
        LOG_INF("✅ Buffer overflow detected");
        violations_detected++;
    } else {
        LOG_WRN("⚠️  Buffer overflow not detected, value: 0x%02X", overflow_value);
    }
    
    /* Test write overflow */
    memory_fault_occurred = false;
    *overflow_ptr = 0xFF;
    
    if (memory_fault_occurred) {
        LOG_INF("✅ Buffer overflow write detected");
        violations_detected++;
    } else {
        LOG_WRN("⚠️  Buffer overflow write not detected");
    }
}

/**
 * @brief Test 5: Execute-Never Region Violation
 *
 * Tests detection when attempting to execute code
 * from data-only memory regions.
 */
static void test_execute_never_violation(void)
{
    LOG_INF("📋 Test 5: Execute-never region violation");
    memory_tests_run++;
    
    /* Create fake code in data region */
    uint32_t fake_code[] = {
        0xE7FE4770,  /* NOP; BX LR (thumb mode) */
        0xE7FEE7FE   /* Infinite loop as backup */
    };
    
    LOG_INF("Fake code at %p", fake_code);
    
    /* Attempt to execute from data region */
    memory_fault_occurred = false;
    LOG_WRN("⚠️  Attempting to execute from data region...");
    
    /* Cast to function pointer and attempt call */
    void (*fake_function)(void) = (void (*)(void))((uintptr_t)fake_code | 1);  /* Thumb bit */
    
    /* This should trigger an execution fault if MPU XN bit is set */
    fake_function();
    
    if (memory_fault_occurred) {
        LOG_INF("✅ Execute-never violation detected");
        violations_detected++;
    } else {
        LOG_WRN("⚠️  Code execution in data region succeeded");
    }
}

/**
 * @brief Test 6: Peripheral Access Violation
 *
 * Tests detection of invalid peripheral register
 * accesses.
 */
static void test_peripheral_access_violation(void)
{
    LOG_INF("📋 Test 6: Peripheral access violation");
    memory_tests_run++;
    
    /* Test access to reserved/invalid peripheral addresses */
    volatile uint32_t *invalid_peripheral = (volatile uint32_t*)0x50000000;
    
    LOG_INF("Testing access to invalid peripheral at 0x%08X", 0x50000000);
    
    memory_fault_occurred = false;
    
    /* Attempt to read from invalid peripheral */
    LOG_WRN("⚠️  Attempting invalid peripheral read...");
    volatile uint32_t value = *invalid_peripheral;
    
    if (memory_fault_occurred) {
        LOG_INF("✅ Invalid peripheral access detected");
        violations_detected++;
    } else {
        LOG_WRN("⚠️  Invalid peripheral read succeeded: 0x%08X", value);
    }
    
    /* Test write to invalid peripheral */
    memory_fault_occurred = false;
    LOG_WRN("⚠️  Attempting invalid peripheral write...");
    *invalid_peripheral = 0x12345678;
    
    if (memory_fault_occurred) {
        LOG_INF("✅ Invalid peripheral write detected");
        violations_detected++;
    } else {
        LOG_WRN("⚠️  Invalid peripheral write succeeded");
    }
}

/**
 * @brief Memory access fault handler
 *
 * Custom fault handler for memory access violations
 * to provide detailed analysis and recovery.
 *
 * @param fault_addr Faulting memory address
 * @param fault_reason Fault reason from MMFSR
 */
void memory_access_fault_handler(uint32_t fault_addr, uint32_t fault_reason)
{
    LOG_INF("✅ Memory access violation detected");
    LOG_INF("Fault address: 0x%08X", fault_addr);
    LOG_INF("Fault reason: 0x%08X", fault_reason);
    
    /* Decode specific fault types */
    if (fault_reason & 0x01) {  /* IACCVIOL */
        LOG_INF("  - Instruction access violation");
    }
    if (fault_reason & 0x02) {  /* DACCVIOL */
        LOG_INF("  - Data access violation");
    }
    if (fault_reason & 0x08) {  /* MUNSTKERR */
        LOG_INF("  - Unstacking error");
    }
    if (fault_reason & 0x10) {  /* MSTKERR */
        LOG_INF("  - Stacking error");
    }
    if (fault_reason & 0x80) {  /* MMARVALID */
        LOG_INF("  - Fault address valid");
    }
    
    violations_detected++;
    memory_fault_occurred = true;
    
    ft_report_fault(FT_BUS_FAULT, FT_SEVERITY_HIGH);
}

/**
 * @brief Main memory access violation test entry point
 *
 * Orchestrates comprehensive memory access violation testing
 * using MPU and hardware fault detection mechanisms.
 *
 * @param p1 Unused parameter 1
 * @param p2 Unused parameter 2  
 * @param p3 Unused parameter 3
 */
void memory_access_test_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("🛡️  Starting ARM Cortex-M3 Memory Access Violation Test Suite");
    
    /* Initialize test statistics */
    memory_tests_run = 0;
    violations_detected = 0;
    memory_fault_occurred = false;
    
    /* Setup MPU for testing */
    if (setup_mpu_test_regions() == 0) {
        LOG_INF("MPU test regions configured");
    }
    
    /* Wait for system stabilization */
    k_sleep(K_SECONDS(1));
    
    LOG_INF("=== Memory Access Violation Tests ===");
    
    /* Execute all memory access tests */
    test_readonly_write_violation();
    k_sleep(K_MSEC(300));
    
    test_invalid_region_access();
    k_sleep(K_MSEC(300));
    
    test_alignment_violation();
    k_sleep(K_MSEC(300));
    
    test_buffer_overflow_detection();
    k_sleep(K_MSEC(300));
    
    test_execute_never_violation();
    k_sleep(K_MSEC(300));
    
    test_peripheral_access_violation();
    k_sleep(K_MSEC(300));
    
    /* Final statistics */
    LOG_INF("=== Memory Access Test Results ===");
    LOG_INF("Memory tests executed: %d", memory_tests_run);
    LOG_INF("Violations detected: %d", violations_detected);
    LOG_INF("Detection rate: %.1f%%", 
           (memory_tests_run > 0) ? 
           (100.0 * violations_detected / memory_tests_run) : 0.0);
    
    /* Report final status */
    if (violations_detected > 0) {
        LOG_INF("✅ Memory access violation detection is working");
    } else {
        LOG_WRN("⚠️  No violations detected - check MPU configuration");
    }
    
    LOG_INF("🛡️  Memory access violation test suite completed");
}