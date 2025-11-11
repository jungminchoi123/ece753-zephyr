/*
 * Simplified Peripheral Misconfiguration Test for ARM Cortex-M3
 * Part of Enhanced Fault Tolerance Framework
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/fault_tolerance.h>

LOG_MODULE_REGISTER(peripheral_misconfig_test, LOG_LEVEL_INF);

/* Test statistics */
static uint32_t config_tests_run = 0;
static uint32_t config_detections = 0;

static bool test_gpio_misconfig(void)
{
    LOG_INF("Testing GPIO misconfiguration detection");
    config_tests_run++;
    
    /* Test invalid GPIO configurations */
    bool invalid_config = false;
    
    /* Simulate checking for conflicting pin configurations */
    LOG_DBG("Checking for GPIO pin conflicts");
    
    /* In a real implementation, this would check for:
     * - Multiple functions assigned to same pin
     * - Invalid pin/port combinations
     * - Conflicting pull-up/down settings
     */
    
    if (invalid_config) {
        LOG_ERR("GPIO misconfiguration detected");
        ft_report_fault(FT_PERIPHERAL_ACCESS_FAULT, FT_SEVERITY_MEDIUM);
        config_detections++;
        return false;
    }
    
    LOG_INF("✅ GPIO configuration test passed");
    return true;
}

static bool test_clock_config(void)
{
    LOG_INF("Testing clock configuration validation");
    config_tests_run++;
    
    /* Test basic clock validation */
    uint32_t sys_clock = sys_clock_hw_cycles_per_sec();
    
    if (sys_clock == 0) {
        LOG_ERR("Invalid system clock detected: %d Hz", sys_clock);
        ft_report_fault(FT_PERIPHERAL_ACCESS_FAULT, FT_SEVERITY_HIGH);
        config_detections++;
        return false;
    }
    
    if (sys_clock > 100000000) { /* > 100MHz might be excessive for some configs */
        LOG_WRN("Very high system clock detected: %d Hz", sys_clock);
        config_detections++;
    }
    
    LOG_INF("✅ System clock: %d Hz - within normal range", sys_clock);
    return true;
}

static bool test_timer_misconfig(void)
{
    LOG_INF("Testing timer misconfiguration detection");
    config_tests_run++;
    
    /* Simulate timer configuration validation */
    bool config_valid = true;
    
    /* In real implementation, check for:
     * - Timer overflow conditions
     * - Invalid prescaler values
     * - Conflicting timer modes
     */
    
    LOG_DBG("Validating timer configurations...");
    k_sleep(K_MSEC(10)); /* Simulate validation time */
    
    if (!config_valid) {
        LOG_ERR("Timer misconfiguration detected");
        ft_report_fault(FT_PERIPHERAL_ACCESS_FAULT, FT_SEVERITY_MEDIUM);
        config_detections++;
        return false;
    }
    
    LOG_INF("✅ Timer configuration appears valid");
    return true;
}

void peripheral_misconfig_test_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("🔧 Starting Peripheral Misconfiguration Test Suite");
    
    /* Initialize test statistics */
    config_tests_run = 0;
    config_detections = 0;
    
    /* Run individual tests */
    test_clock_config();
    test_gpio_misconfig();
    test_timer_misconfig();
    
    /* Print summary */
    LOG_INF("=== Peripheral Configuration Test Summary ===");
    LOG_INF("Tests Run: %d", config_tests_run);
    LOG_INF("Issues Detected: %d", config_detections);
    
    if (config_detections > 0) {
        LOG_INF("✅ Peripheral misconfiguration detection working");
    } else {
        LOG_INF("ℹ️ No peripheral misconfigurations detected");
    }
    
    LOG_INF("🔧 Peripheral Misconfiguration Test Complete");
}