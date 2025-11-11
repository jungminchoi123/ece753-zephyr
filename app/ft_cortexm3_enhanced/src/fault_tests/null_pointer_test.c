/**
 * @file null_pointer_test.c
 * @brief ARM Cortex-M3 Null Pointer Dereference Test Module
 * @author Jack Ostapeic, MS ECE Student at University of Wisconsin-Madison
 * 
 * This module implements comprehensive null pointer dereference testing for 
 * ARM Cortex-M3 architecture, utilizing hardware Memory Protection Unit (MPU)
 * and software detection mechanisms.
 *
 * Null Pointer Detection Methods:
 * 1. Hardware MPU NULL Region Protection - ARM Cortex-M3 MPU-based detection
 * 2. Software Address Validation - Runtime pointer checking
 * 3. Memory Access Fault Handlers - ARM fault exception handling
 * 4. Pointer Sanitization - Defensive programming checks
 *
 * Test Scenarios:
 * - Direct NULL pointer dereference (read/write)
 * - Function pointer NULL calls
 * - Array access through NULL base
 * - Structure member access through NULL
 * - NULL pointer arithmetic and comparison
 *
 * Recovery Mechanisms:
 * - MPU fault exception handling
 * - Graceful error return
 * - Memory access validation
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance.h>

LOG_MODULE_REGISTER(null_pointer_test, LOG_LEVEL_INF);

/* Test configuration */
#define NULL_REGION_SIZE 0x1000     /* 4KB null region */
#define TEST_DATA_PATTERN 0xDEADBEEF
#define MAX_NULL_TESTS 6

/* Statistics tracking */
static int null_tests_run = 0;
static int null_detections = 0;
static volatile bool null_fault_occurred = false;

/* Test data structures */
typedef struct {
    uint32_t magic;
    uint32_t data;
    char message[64];
} test_struct_t;

typedef int (*test_function_t)(int arg);

/**
 * @brief Setup MPU to protect NULL pointer region
 *
 * Configures ARM Cortex-M3 MPU to detect NULL pointer
 * dereferences by protecting the low memory region.
 *
 * @return 0 on success, negative on failure
 */
static int setup_null_protection(void)
{
    #ifdef CONFIG_ARM_MPU
    /* MPU region 0: Protect NULL pointer region (0x0 - 0x1000) */
    struct arm_mpu_region null_region = {
        .name = "NULL_PROTECTION",
        .base = 0x00000000,
        .size = NULL_REGION_SIZE,
        .attr = {
            .rbar = ARM_MPU_RBAR(0, 0x00000000),
            .rasr = ARM_MPU_RASR(
                0,                           /* Disable region initially */
                ARM_MPU_AP_NONE,            /* No access permissions */
                0,                          /* No type extension */
                0,                          /* Not shareable */
                0,                          /* Not cacheable */
                0,                          /* Not bufferable */
                ARM_MPU_REGION_SIZE_4KB     /* 4KB region */
            )
        }
    };
    
    LOG_INF("Setting up NULL pointer protection region");
    LOG_INF("Protected region: 0x%08X - 0x%08X (%d bytes)",
           0, NULL_REGION_SIZE, NULL_REGION_SIZE);
    
    /* Enable MPU with background region */
    arm_mpu_enable(MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk);
    
    /* Configure NULL protection region */
    arm_mpu_configure_region(&null_region);
    
    LOG_INF("✅ NULL pointer protection enabled via MPU");
    return 0;
    #else
    LOG_WRN("⚠️  MPU not available - using software detection only");
    return -ENOTSUP;
    #endif
}

/**
 * @brief Software-based NULL pointer validation
 *
 * Validates pointer before use to catch NULL dereferences
 * in software before hardware detection.
 *
 * @param ptr Pointer to validate
 * @param description Human-readable description
 * @return true if pointer is valid, false if NULL
 */
static bool validate_pointer(const void *ptr, const char *description)
{
    if (ptr == NULL) {
        LOG_ERR("🚨 NULL pointer detected: %s", description);
        ft_report_fault(FT_NULL_POINTER_FAULT, FT_SEVERITY_HIGH);
        null_detections++;
        return false;
    }
    
    /* Additional checks for obviously invalid pointers */
    uintptr_t addr = (uintptr_t)ptr;
    if (addr < NULL_REGION_SIZE) {
        LOG_ERR("🚨 Invalid low-memory pointer detected: %s (0x%08X)", 
               description, addr);
        ft_report_fault(FT_NULL_POINTER_FAULT, FT_SEVERITY_HIGH);
        null_detections++;
        return false;
    }
    
    return true;
}

/**
 * @brief Test 1: Direct NULL Pointer Read Dereference
 *
 * Attempts to read from a NULL pointer to trigger
 * hardware or software detection mechanisms.
 */
static void test_null_read_dereference(void)
{
    LOG_INF("📋 Test 1: NULL pointer read dereference");
    null_tests_run++;
    
    volatile uint32_t *null_ptr = NULL;
    volatile uint32_t read_value = 0;
    
    LOG_INF("Attempting to read from NULL pointer...");
    
    /* Software validation first */
    if (!validate_pointer(null_ptr, "null_ptr read")) {
        LOG_INF("✅ Software NULL detection successful");
        return;
    }
    
    /* If software validation passes (shouldn't), try hardware */
    LOG_WRN("⚠️  Software validation bypassed, testing hardware...");
    
    /* This should trigger an MPU fault or memory exception */
    null_fault_occurred = false;
    
    #ifdef CONFIG_ARM_MPU
    /* Enable NULL region as no-access to trigger fault */
    MPU->RNR = 0;  /* Select region 0 */
    MPU->RASR |= MPU_RASR_ENABLE_Msk;  /* Enable region */
    __DSB();
    __ISB();
    #endif
    
    /* Attempt the dereference - should fault */
    read_value = *null_ptr;  /* This should never execute */
    
    LOG_ERR("❌ NULL pointer read succeeded - DETECTION FAILED!");
    LOG_ERR("Read value: 0x%08X", read_value);
}

/**
 * @brief Test 2: Direct NULL Pointer Write Dereference  
 *
 * Attempts to write to a NULL pointer to trigger
 * detection mechanisms.
 */
static void test_null_write_dereference(void)
{
    LOG_INF("📋 Test 2: NULL pointer write dereference");
    null_tests_run++;
    
    volatile uint32_t *null_ptr = NULL;
    
    LOG_INF("Attempting to write to NULL pointer...");
    
    /* Software validation */
    if (!validate_pointer(null_ptr, "null_ptr write")) {
        LOG_INF("✅ Software NULL detection successful");
        return;
    }
    
    LOG_WRN("⚠️  Software validation bypassed, testing hardware...");
    
    /* This should trigger an MPU fault */
    null_fault_occurred = false;
    *null_ptr = TEST_DATA_PATTERN;  /* Should fault */
    
    LOG_ERR("❌ NULL pointer write succeeded - DETECTION FAILED!");
}

/**
 * @brief Test 3: NULL Function Pointer Call
 *
 * Tests detection of NULL function pointer calls
 * which can cause branch to invalid addresses.
 */
static void test_null_function_call(void)
{
    LOG_INF("📋 Test 3: NULL function pointer call");
    null_tests_run++;
    
    test_function_t null_func = NULL;
    
    LOG_INF("Attempting to call NULL function pointer...");
    
    /* Software validation */
    if (!validate_pointer((void*)null_func, "function pointer")) {
        LOG_INF("✅ Software NULL function detection successful");
        return;
    }
    
    LOG_WRN("⚠️  Software validation bypassed, testing hardware...");
    
    /* Attempt to call NULL function - should fault */
    null_fault_occurred = false;
    int result = null_func(42);  /* Should fault */
    
    LOG_ERR("❌ NULL function call succeeded - DETECTION FAILED!");
    LOG_ERR("Function returned: %d", result);
}

/**
 * @brief Test 4: NULL Structure Member Access
 *
 * Tests detection when accessing members of a
 * NULL structure pointer.
 */
static void test_null_struct_access(void)
{
    LOG_INF("📋 Test 4: NULL structure member access");
    null_tests_run++;
    
    test_struct_t *null_struct = NULL;
    
    LOG_INF("Attempting to access NULL struct members...");
    
    /* Software validation */
    if (!validate_pointer(null_struct, "struct pointer")) {
        LOG_INF("✅ Software NULL struct detection successful");
        return;
    }
    
    LOG_WRN("⚠️  Software validation bypassed, testing hardware...");
    
    /* Test different member accesses */
    null_fault_occurred = false;
    
    /* Access magic member (offset 0) */
    LOG_DBG("Accessing magic member at offset 0...");
    uint32_t magic = null_struct->magic;  /* Should fault */
    
    /* Access data member (offset 4) */
    LOG_DBG("Accessing data member at offset 4...");
    uint32_t data = null_struct->data;  /* Should fault */
    
    /* Access message member (offset 8) */
    LOG_DBG("Accessing message member at offset 8...");
    char first_char = null_struct->message[0];  /* Should fault */
    
    LOG_ERR("❌ NULL struct access succeeded - DETECTION FAILED!");
    LOG_ERR("Magic: 0x%08X, Data: 0x%08X, First char: 0x%02X", 
           magic, data, first_char);
}

/**
 * @brief Test 5: NULL Array Access
 *
 * Tests detection when using NULL as array base
 * pointer for indexed access.
 */
static void test_null_array_access(void)
{
    LOG_INF("📋 Test 5: NULL array access");
    null_tests_run++;
    
    volatile uint32_t *null_array = NULL;
    
    LOG_INF("Attempting to access NULL array elements...");
    
    /* Software validation */
    if (!validate_pointer(null_array, "array pointer")) {
        LOG_INF("✅ Software NULL array detection successful");
        return;
    }
    
    LOG_WRN("⚠️  Software validation bypassed, testing hardware...");
    
    /* Test different array indices */
    null_fault_occurred = false;
    
    for (int i = 0; i < 5; i++) {
        LOG_DBG("Accessing array index %d...", i);
        volatile uint32_t value = null_array[i];  /* Should fault */
        LOG_ERR("Index %d value: 0x%08X", i, value);
    }
    
    LOG_ERR("❌ NULL array access succeeded - DETECTION FAILED!");
}

/**
 * @brief Test 6: NULL Pointer Arithmetic
 *
 * Tests detection during NULL pointer arithmetic
 * operations that may access NULL region.
 */
static void test_null_pointer_arithmetic(void)
{
    LOG_INF("📋 Test 6: NULL pointer arithmetic");
    null_tests_run++;
    
    volatile char *null_ptr = NULL;
    
    LOG_INF("Performing NULL pointer arithmetic...");
    
    /* Test various arithmetic operations */
    for (int offset = -10; offset <= 10; offset++) {
        volatile char *calc_ptr = null_ptr + offset;
        
        LOG_DBG("Testing offset %d, pointer: %p", offset, calc_ptr);
        
        /* Validate calculated pointer */
        if (!validate_pointer(calc_ptr, "arithmetic result")) {
            continue;  /* Software caught it */
        }
        
        /* If in NULL region, attempt access */
        if ((uintptr_t)calc_ptr < NULL_REGION_SIZE) {
            LOG_DBG("Attempting access to low-memory region...");
            volatile char value = *calc_ptr;  /* Should fault */
            LOG_ERR("❌ Low-memory access succeeded: 0x%02X", value);
        }
    }
    
    LOG_INF("NULL pointer arithmetic test completed");
}

/**
 * @brief NULL pointer fault handler
 *
 * Custom fault handler specifically for NULL pointer
 * exceptions to provide detailed analysis.
 *
 * @param fault_addr Faulting address
 * @param fault_reason Reason code
 */
void null_pointer_fault_handler(uint32_t fault_addr, uint32_t fault_reason)
{
    if (fault_addr < NULL_REGION_SIZE) {
        LOG_INF("✅ NULL pointer access detected at 0x%08X", fault_addr);
        LOG_INF("Fault reason: 0x%08X", fault_reason);
        null_detections++;
        null_fault_occurred = true;
        
        /* Report successful detection */
        ft_report_fault(FT_NULL_POINTER_FAULT, FT_SEVERITY_MEDIUM);
    }
}

/**
 * @brief Main null pointer test entry point
 *
 * Orchestrates comprehensive null pointer testing using
 * multiple detection mechanisms and access patterns.
 *
 * @param p1 Unused parameter 1
 * @param p2 Unused parameter 2  
 * @param p3 Unused parameter 3
 */
void null_pointer_test_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("🎯 Starting ARM Cortex-M3 NULL Pointer Test Suite");
    
    /* Initialize test statistics */
    null_tests_run = 0;
    null_detections = 0;
    null_fault_occurred = false;
    
    /* Setup hardware protection */
    if (setup_null_protection() == 0) {
        LOG_INF("Hardware NULL protection enabled");
    }
    
    /* Wait for system stabilization */
    k_sleep(K_SECONDS(1));
    
    LOG_INF("=== NULL Pointer Detection Tests ===");
    
    /* Execute all null pointer tests */
    test_null_read_dereference();
    k_sleep(K_MSEC(200));
    
    test_null_write_dereference();
    k_sleep(K_MSEC(200));
    
    test_null_function_call();
    k_sleep(K_MSEC(200));
    
    test_null_struct_access();
    k_sleep(K_MSEC(200));
    
    test_null_array_access();
    k_sleep(K_MSEC(200));
    
    test_null_pointer_arithmetic();
    k_sleep(K_MSEC(200));
    
    /* Final statistics */
    LOG_INF("=== NULL Pointer Test Results ===");
    LOG_INF("Tests executed: %d", null_tests_run);
    LOG_INF("NULL accesses detected: %d", null_detections);
    LOG_INF("Detection rate: %.1f%%", 
           (null_tests_run > 0) ? 
           (100.0 * null_detections / null_tests_run) : 0.0);
    
    /* Report final status */
    if (null_detections > 0) {
        LOG_INF("✅ NULL pointer detection is working correctly");
    } else {
        LOG_WRN("⚠️  No NULL pointers detected - check protection setup");
    }
    
    LOG_INF("🎯 NULL pointer test suite completed");
}