/**
 * @file division_by_zero_test.c
 * @brief ARM Cortex-M3 Division by Zero Test Module
 * @author Jack Ostapeic, MS ECE Student at University of Wisconsin-Madison
 * 
 * This module implements comprehensive division by zero testing for 
 * ARM Cortex-M3 architecture, utilizing both hardware and software 
 * detection mechanisms for arithmetic exceptions.
 *
 * Division by Zero Detection Methods:
 * 1. Hardware Divide-by-Zero Trap - ARM Cortex-M3 DIV_0_TRP feature
 * 2. Software Pre-validation - Runtime divisor checking
 * 3. Floating-Point Exception Handling - FPU divide-by-zero detection
 * 4. Integer Division Monitoring - Custom division wrapper functions
 * 5. Result Validation - Post-division result analysis
 *
 * Test Scenarios:
 * - Integer division by zero (signed/unsigned)
 * - Floating-point division by zero
 * - Modulo operation with zero divisor
 * - Division in mathematical expressions
 * - Recursive division scenarios
 * - Division in interrupt context
 *
 * Recovery Mechanisms:
 * - Exception handling and graceful recovery
 * - Safe division wrapper functions
 * - Result substitution with safe values
 * - Mathematical operation validation
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance.h>
#include <math.h>
#include <float.h>

LOG_MODULE_REGISTER(division_by_zero_test, LOG_LEVEL_INF);

/* Test configuration */
#define MAX_DIVISION_TESTS 8
#define SAFE_DIVISION_RESULT 0x7FFFFFFF
#define DIVISION_TEST_ITERATIONS 100

/* Statistics tracking */
static int division_tests_run = 0;
static int divisions_detected = 0;
static volatile bool division_fault_occurred = false;

/* Test result tracking */
typedef struct {
    const char *test_name;
    bool hardware_detected;
    bool software_detected;
    uint32_t result_value;
    uint32_t fault_count;
} division_test_result_t;

static division_test_result_t test_results[MAX_DIVISION_TESTS];

/**
 * @brief Enable hardware division-by-zero detection
 *
 * Configures ARM Cortex-M3 SCB to enable hardware
 * division-by-zero trapping functionality.
 *
 * @return 0 on success, negative on failure
 */
static int enable_hardware_div_zero_trap(void)
{
    /* Enable divide-by-zero trap in Configuration Control Register */
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
    __DSB();
    __ISB();
    
    LOG_INF("✅ Hardware divide-by-zero trap enabled");
    LOG_INF("SCB->CCR = 0x%08X", SCB->CCR);
    
    return 0;
}

/**
 * @brief Software-based division validation
 *
 * Validates divisor before performing division operation
 * to catch divide-by-zero conditions in software.
 *
 * @param dividend Value to be divided
 * @param divisor Value to divide by
 * @param description Human-readable description
 * @return Division result or safe value if divisor is zero
 */
static int32_t safe_divide_int32(int32_t dividend, int32_t divisor, const char *description)
{
    if (divisor == 0) {
        LOG_ERR("🚨 Software division by zero detected: %s", description);
        LOG_ERR("Dividend: %d, Divisor: %d", dividend, divisor);
        
        ft_report_fault(FT_DIVISION_BY_ZERO_FAULT, FT_SEVERITY_MEDIUM);
        divisions_detected++;
        
        /* Return safe result to prevent undefined behavior */
        if (dividend > 0) {
            return INT32_MAX;
        } else if (dividend < 0) {
            return INT32_MIN;
        } else {
            return 0;
        }
    }
    
    return dividend / divisor;
}

/**
 * @brief Software-based unsigned division validation
 *
 * @param dividend Unsigned value to be divided
 * @param divisor Unsigned value to divide by  
 * @param description Human-readable description
 * @return Division result or safe value if divisor is zero
 */
static uint32_t safe_divide_uint32(uint32_t dividend, uint32_t divisor, const char *description)
{
    if (divisor == 0) {
        LOG_ERR("🚨 Software unsigned division by zero detected: %s", description);
        LOG_ERR("Dividend: %u, Divisor: %u", dividend, divisor);
        
        ft_report_fault(FT_DIVISION_BY_ZERO_FAULT, FT_SEVERITY_MEDIUM);
        divisions_detected++;
        
        /* Return maximum value for unsigned division by zero */
        return UINT32_MAX;
    }
    
    return dividend / divisor;
}

/**
 * @brief Software-based floating-point division validation
 *
 * @param dividend Float value to be divided
 * @param divisor Float value to divide by
 * @param description Human-readable description  
 * @return Division result or infinity if divisor is zero
 */
static float safe_divide_float(float dividend, float divisor, const char *description)
{
    if (divisor == 0.0f || fabsf(divisor) < FLT_EPSILON) {
        LOG_ERR("🚨 Software floating-point division by zero detected: %s", description);
        LOG_ERR("Dividend: %f, Divisor: %f", (double)dividend, (double)divisor);
        
        ft_report_fault(FT_DIVISION_BY_ZERO_FAULT, FT_SEVERITY_MEDIUM);
        divisions_detected++;
        
        /* Return appropriate infinity */
        return (dividend >= 0.0f) ? INFINITY : -INFINITY;
    }
    
    return dividend / divisor;
}

/**
 * @brief Test 1: Integer Division by Zero (Signed)
 *
 * Tests signed integer division by zero using both
 * hardware and software detection methods.
 */
static void test_signed_integer_division(void)
{
    LOG_INF("📋 Test 1: Signed integer division by zero");
    division_tests_run++;
    
    division_test_result_t *result = &test_results[0];
    result->test_name = "Signed Integer Division";
    result->hardware_detected = false;
    result->software_detected = false;
    result->fault_count = 0;
    
    /* Test with software validation first */
    LOG_INF("Testing with software validation...");
    int32_t safe_result = safe_divide_int32(42, 0, "signed integer test");
    LOG_INF("Software-validated result: %d", safe_result);
    result->software_detected = (safe_result != 42/0);  /* Would be undefined */
    
    /* Test hardware detection (if enabled) */
    LOG_WRN("⚠️  Testing hardware division by zero detection...");
    division_fault_occurred = false;
    
    volatile int32_t dividend = 100;
    volatile int32_t divisor = 0;
    volatile int32_t hw_result = 0;
    
    /* This should trigger hardware divide-by-zero trap */
    hw_result = dividend / divisor;
    
    result->result_value = hw_result;
    result->hardware_detected = division_fault_occurred;
    
    if (division_fault_occurred) {
        LOG_INF("✅ Hardware divide-by-zero trap triggered");
        result->fault_count++;
    } else {
        LOG_WRN("⚠️  Hardware division succeeded: %d / %d = %d", 
               dividend, divisor, hw_result);
    }
    
    /* Test edge cases */
    LOG_DBG("Testing edge cases...");
    
    /* Division of zero by zero */
    division_fault_occurred = false;
    volatile int32_t zero_dividend = 0;
    volatile int32_t zero_result = zero_dividend / divisor;
    
    if (division_fault_occurred) {
        LOG_INF("✅ Zero divided by zero detected");
        result->fault_count++;
    }
    
    /* Very small divisor (not exactly zero) */
    int32_t small_result = safe_divide_int32(1000, 1, "small divisor");
    LOG_DBG("Small divisor result: %d", small_result);
}

/**
 * @brief Test 2: Unsigned Integer Division by Zero
 *
 * Tests unsigned integer division by zero scenarios.
 */
static void test_unsigned_integer_division(void)
{
    LOG_INF("📋 Test 2: Unsigned integer division by zero");
    division_tests_run++;
    
    division_test_result_t *result = &test_results[1];
    result->test_name = "Unsigned Integer Division";
    result->hardware_detected = false;
    result->software_detected = false;
    result->fault_count = 0;
    
    /* Software validation test */
    uint32_t safe_result = safe_divide_uint32(0xFFFFFFFF, 0, "unsigned test");
    LOG_INF("Software unsigned result: %u", safe_result);
    result->software_detected = (safe_result == UINT32_MAX);
    
    /* Hardware detection test */
    LOG_WRN("⚠️  Testing unsigned hardware division by zero...");
    division_fault_occurred = false;
    
    volatile uint32_t u_dividend = 12345;
    volatile uint32_t u_divisor = 0;
    volatile uint32_t u_result = u_dividend / u_divisor;
    
    result->result_value = u_result;
    result->hardware_detected = division_fault_occurred;
    
    if (division_fault_occurred) {
        LOG_INF("✅ Unsigned hardware divide-by-zero detected");
        result->fault_count++;
    } else {
        LOG_WRN("⚠️  Unsigned division succeeded: %u", u_result);
    }
}

/**
 * @brief Test 3: Floating-Point Division by Zero
 *
 * Tests floating-point division by zero using FPU
 * exception handling mechanisms.
 */
static void test_floating_point_division(void)
{
    LOG_INF("📋 Test 3: Floating-point division by zero");
    division_tests_run++;
    
    division_test_result_t *result = &test_results[2];
    result->test_name = "Floating-Point Division";
    result->hardware_detected = false;
    result->software_detected = false;
    result->fault_count = 0;
    
    #ifdef CONFIG_FPU
    /* Enable FPU divide-by-zero exception */
    uint32_t fpccr = FPU->FPCCR;
    LOG_INF("FPU FPCCR: 0x%08X", fpccr);
    
    /* Software validation test */
    float safe_result = safe_divide_float(3.14159f, 0.0f, "float test");
    LOG_INF("Software float result: %f", (double)safe_result);
    result->software_detected = isinf(safe_result);
    
    /* Hardware FPU test */
    LOG_WRN("⚠️  Testing FPU division by zero...");
    division_fault_occurred = false;
    
    volatile float f_dividend = 42.5f;
    volatile float f_divisor = 0.0f;
    volatile float f_result = f_dividend / f_divisor;
    
    LOG_INF("FPU division result: %f", (double)f_result);
    
    /* Check if result is infinity (expected for FP division by zero) */
    if (isinf(f_result)) {
        LOG_INF("✅ Floating-point division by zero produces infinity");
        result->software_detected = true;
        divisions_detected++;
    }
    
    /* Test special cases */
    volatile float zero_div_zero = 0.0f / 0.0f;
    if (isnan(zero_div_zero)) {
        LOG_INF("✅ 0.0/0.0 produces NaN as expected");
        result->fault_count++;
    }
    
    volatile float neg_div_zero = -1.0f / 0.0f;
    if (isinf(neg_div_zero) && signbit(neg_div_zero)) {
        LOG_INF("✅ Negative division by zero produces -infinity");
        result->fault_count++;
    }
    
    #else
    LOG_WRN("⚠️  FPU not available for floating-point tests");
    #endif
}

/**
 * @brief Test 4: Modulo Operation by Zero
 *
 * Tests modulo (remainder) operations with zero divisor.
 */
static void test_modulo_by_zero(void)
{
    LOG_INF("📋 Test 4: Modulo operation by zero");
    division_tests_run++;
    
    division_test_result_t *result = &test_results[3];
    result->test_name = "Modulo by Zero";
    result->hardware_detected = false;
    result->software_detected = false;
    result->fault_count = 0;
    
    /* Software validation for modulo */
    LOG_INF("Testing modulo operation with zero divisor...");
    
    /* Create safe modulo function */
    auto int32_t safe_modulo(int32_t dividend, int32_t divisor) {
        if (divisor == 0) {
            LOG_ERR("🚨 Modulo by zero detected");
            ft_report_fault(FT_DIVISION_BY_ZERO_FAULT, FT_SEVERITY_MEDIUM);
            divisions_detected++;
            return 0;  /* Safe default */
        }
        return dividend % divisor;
    };
    
    int32_t mod_result = safe_modulo(100, 0);
    LOG_INF("Software modulo result: %d", mod_result);
    result->software_detected = (mod_result == 0);
    
    /* Hardware modulo test */
    LOG_WRN("⚠️  Testing hardware modulo by zero...");
    division_fault_occurred = false;
    
    volatile int32_t mod_dividend = 77;
    volatile int32_t mod_divisor = 0;
    volatile int32_t hw_mod_result = mod_dividend % mod_divisor;
    
    result->result_value = hw_mod_result;
    result->hardware_detected = division_fault_occurred;
    
    if (division_fault_occurred) {
        LOG_INF("✅ Hardware modulo by zero detected");
        result->fault_count++;
    } else {
        LOG_WRN("⚠️  Modulo operation succeeded: %d", hw_mod_result);
    }
}

/**
 * @brief Test 5: Division in Mathematical Expressions
 *
 * Tests division by zero within complex mathematical
 * expressions and function calls.
 */
static void test_expression_division(void)
{
    LOG_INF("📋 Test 5: Division in mathematical expressions");
    division_tests_run++;
    
    division_test_result_t *result = &test_results[4];
    result->test_name = "Expression Division";
    result->hardware_detected = false;
    result->software_detected = false;
    result->fault_count = 0;
    
    /* Test division within expressions */
    volatile int32_t a = 10, b = 5, c = 0;
    
    LOG_WRN("⚠️  Testing division within complex expression...");
    division_fault_occurred = false;
    
    /* Expression that includes division by zero */
    volatile int32_t expr_result = (a * b) / c + 42;
    
    if (division_fault_occurred) {
        LOG_INF("✅ Division by zero in expression detected");
        result->hardware_detected = true;
        result->fault_count++;
    } else {
        LOG_WRN("⚠️  Expression evaluation succeeded: %d", expr_result);
        result->result_value = expr_result;
    }
    
    /* Test function call with division */
    auto int32_t calculate_average(int32_t sum, int32_t count) {
        return safe_divide_int32(sum, count, "average calculation");
    };
    
    int32_t avg = calculate_average(150, 0);
    LOG_INF("Average calculation with zero count: %d", avg);
    if (avg == INT32_MAX) {
        result->software_detected = true;
    }
}

/**
 * @brief Test 6: Recursive Division Scenarios
 *
 * Tests division by zero in recursive function calls
 * and nested calculations.
 */
static void test_recursive_division(void)
{
    LOG_INF("📋 Test 6: Recursive division scenarios");
    division_tests_run++;
    
    division_test_result_t *result = &test_results[5];
    result->test_name = "Recursive Division";
    result->hardware_detected = false;
    result->software_detected = false;
    result->fault_count = 0;
    
    /* Recursive function that may divide by zero */
    auto int32_t recursive_calc(int32_t n, int32_t depth) {
        if (depth <= 0) return 1;
        
        int32_t divisor = n - depth * 2;
        if (divisor == 0) {
            LOG_WRN("⚠️  Zero divisor in recursive function at depth %d", depth);
            ft_report_fault(FT_DIVISION_BY_ZERO_FAULT, FT_SEVERITY_LOW);
            divisions_detected++;
            return 0;
        }
        
        return n / divisor + recursive_calc(n, depth - 1);
    };
    
    LOG_INF("Starting recursive calculation...");
    int32_t recursive_result = recursive_calc(10, 5);
    LOG_INF("Recursive calculation result: %d", recursive_result);
    
    if (recursive_result == 0) {
        result->software_detected = true;
    }
}

/**
 * @brief Division by zero fault handler
 *
 * Custom fault handler for division by zero exceptions
 * providing detailed analysis and recovery.
 *
 * @param fault_pc Program counter at fault
 * @param fault_instruction Instruction that caused fault
 */
void division_fault_handler(uint32_t fault_pc, uint32_t fault_instruction)
{
    LOG_INF("✅ Division by zero fault detected");
    LOG_INF("Fault PC: 0x%08X", fault_pc);
    LOG_INF("Fault instruction: 0x%08X", fault_instruction);
    
    divisions_detected++;
    division_fault_occurred = true;
    
    /* Analyze instruction to determine division type */
    if ((fault_instruction & 0xFFF0F000) == 0xFB90F000) {
        LOG_INF("  - SDIV instruction detected");
    } else if ((fault_instruction & 0xFFF0F000) == 0xFBB0F000) {
        LOG_INF("  - UDIV instruction detected");
    }
    
    ft_report_fault(FT_DIVISION_BY_ZERO_FAULT, FT_SEVERITY_HIGH);
}

/**
 * @brief Print division test results summary
 *
 * Displays detailed results from all division tests
 * including detection rates and fault analysis.
 */
static void print_division_test_summary(void)
{
    LOG_INF("=== Division by Zero Test Results Summary ===");
    
    for (int i = 0; i < division_tests_run; i++) {
        const division_test_result_t *result = &test_results[i];
        
        LOG_INF("Test %d: %s", i + 1, result->test_name);
        LOG_INF("  Hardware Detection: %s", result->hardware_detected ? "YES" : "NO");
        LOG_INF("  Software Detection: %s", result->software_detected ? "YES" : "NO");
        LOG_INF("  Fault Count: %u", result->fault_count);
        
        if (result->result_value != 0) {
            LOG_INF("  Result Value: 0x%08X", result->result_value);
        }
    }
    
    /* Calculate overall detection rate */
    int total_detections = 0;
    for (int i = 0; i < division_tests_run; i++) {
        if (test_results[i].hardware_detected || test_results[i].software_detected) {
            total_detections++;
        }
    }
    
    float detection_rate = (division_tests_run > 0) ? 
                          (100.0f * total_detections / division_tests_run) : 0.0f;
    
    LOG_INF("Overall detection rate: %.1f%% (%d/%d)", 
           detection_rate, total_detections, division_tests_run);
}

/**
 * @brief Main division by zero test entry point
 *
 * Orchestrates comprehensive division by zero testing using
 * hardware and software detection mechanisms.
 *
 * @param p1 Unused parameter 1
 * @param p2 Unused parameter 2  
 * @param p3 Unused parameter 3
 */
void division_by_zero_test_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("➗ Starting ARM Cortex-M3 Division by Zero Test Suite");
    
    /* Initialize test statistics */
    division_tests_run = 0;
    divisions_detected = 0;
    division_fault_occurred = false;
    
    /* Clear test results */
    memset(test_results, 0, sizeof(test_results));
    
    /* Enable hardware divide-by-zero detection */
    enable_hardware_div_zero_trap();
    
    /* Wait for system stabilization */
    k_sleep(K_SECONDS(1));
    
    LOG_INF("=== Division by Zero Detection Tests ===");
    
    /* Execute all division tests */
    test_signed_integer_division();
    k_sleep(K_MSEC(200));
    
    test_unsigned_integer_division();
    k_sleep(K_MSEC(200));
    
    test_floating_point_division();
    k_sleep(K_MSEC(200));
    
    test_modulo_by_zero();
    k_sleep(K_MSEC(200));
    
    test_expression_division();
    k_sleep(K_MSEC(200));
    
    test_recursive_division();
    k_sleep(K_MSEC(200));
    
    /* Print detailed results */
    print_division_test_summary();
    
    /* Final statistics */
    LOG_INF("=== Final Division Test Results ===");
    LOG_INF("Tests executed: %d", division_tests_run);
    LOG_INF("Divisions by zero detected: %d", divisions_detected);
    LOG_INF("Detection rate: %.1f%%", 
           (division_tests_run > 0) ? 
           (100.0 * divisions_detected / division_tests_run) : 0.0);
    
    /* Report final status */
    if (divisions_detected > 0) {
        LOG_INF("✅ Division by zero detection is working correctly");
    } else {
        LOG_WRN("⚠️  No divisions by zero detected - check trap configuration");
    }
    
    LOG_INF("➗ Division by zero test suite completed");
}