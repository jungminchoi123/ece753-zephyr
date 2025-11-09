/*
 * Copyright (c) 2025 Jack Ostapeic
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Modular Fault Tolerance Integration Test
 * 
 * This application demonstrates the modular fault tolerance framework
 * with runtime configuration based on Kconfig selections.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance/ft_core.h>

/* Conditionally include module headers based on configuration */
#ifdef CONFIG_FT_ENABLE_STACK_OVERFLOW_PROTECTION
#include <zephyr/fault_tolerance/ft_stack_overflow.h>
#endif

#ifdef CONFIG_FT_ENABLE_BUFFER_OVERFLOW_PROTECTION
#include <zephyr/fault_tolerance/ft_buffer_overflow.h>
#endif

#ifdef CONFIG_FT_ENABLE_DEADLOCK_DETECTION
#include <zephyr/fault_tolerance/ft_deadlock.h>
#endif

#ifdef CONFIG_FT_ENABLE_RACE_CONDITION_DETECTION
#include <zephyr/fault_tolerance/ft_race_condition.h>
#endif

LOG_MODULE_REGISTER(ft_modular_test, LOG_LEVEL_INF);

/* External test function declarations */
#ifdef CONFIG_FT_ENABLE_STACK_OVERFLOW_PROTECTION
int stack_overflow_test(void);
#endif

#ifdef CONFIG_FT_ENABLE_BUFFER_OVERFLOW_PROTECTION
int buffer_overflow_test(void);
#endif

#ifdef CONFIG_FT_ENABLE_DEADLOCK_DETECTION
int deadlock_test(void);
#endif

#ifdef CONFIG_FT_ENABLE_RACE_CONDITION_DETECTION
int race_condition_test(void);
#endif

/* Test configuration display */
static void display_test_configuration(void)
{
    LOG_INF("=== Modular Fault Tolerance Integration Test ===");
    LOG_INF("Enabled modules:");

#ifdef CONFIG_FT_ENABLE_STACK_OVERFLOW_PROTECTION
    LOG_INF("  ✓ Stack Overflow Protection");
#else
    LOG_INF("  ✗ Stack Overflow Protection - DISABLED");
#endif

#ifdef CONFIG_FT_ENABLE_BUFFER_OVERFLOW_PROTECTION
    LOG_INF("  ✓ Buffer Overflow Protection");
#else
    LOG_INF("  ✗ Buffer Overflow Protection - DISABLED");
#endif

#ifdef CONFIG_FT_ENABLE_DEADLOCK_DETECTION
    LOG_INF("  ✓ Deadlock Detection");
#else
    LOG_INF("  ✗ Deadlock Detection - DISABLED");
#endif

#ifdef CONFIG_FT_ENABLE_RACE_CONDITION_DETECTION
    LOG_INF("  ✓ Race Condition Detection");
#else
    LOG_INF("  ✗ Race Condition Detection - DISABLED");
#endif

    LOG_INF("================================================");
}

/* Initialize all enabled fault tolerance modules */
static int initialize_modules(void)
{
    int ret = 0;
    
    LOG_INF("Initializing enabled fault tolerance modules...");

#ifdef CONFIG_FT_ENABLE_STACK_OVERFLOW_PROTECTION
    ret = ft_stack_overflow_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize stack overflow protection: %d", ret);
        return ret;
    }
    LOG_INF("Stack overflow protection initialized");
#endif

#ifdef CONFIG_FT_ENABLE_BUFFER_OVERFLOW_PROTECTION
    ret = ft_buffer_overflow_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize buffer overflow protection: %d", ret);
        return ret;
    }
    LOG_INF("Buffer overflow protection initialized");
#endif

#ifdef CONFIG_FT_ENABLE_DEADLOCK_DETECTION
    ret = ft_deadlock_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize deadlock detection: %d", ret);
        return ret;
    }
    LOG_INF("Deadlock detection initialized");
#endif

#ifdef CONFIG_FT_ENABLE_RACE_CONDITION_DETECTION
    ret = ft_race_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize race condition detection: %d", ret);
        return ret;
    }
    LOG_INF("Race condition detection initialized");
#endif

    LOG_INF("All enabled modules initialized successfully");
    return 0;
}

/* Run tests for all enabled modules */
static void run_module_tests(void)
{
    LOG_INF("Running tests for enabled modules...");

#ifdef CONFIG_FT_ENABLE_STACK_OVERFLOW_PROTECTION
    LOG_INF("Running stack overflow protection test...");
    if (stack_overflow_test() == 0) {
        LOG_INF("Stack overflow test: PASSED");
    } else {
        LOG_ERR("Stack overflow test: FAILED");
    }
    k_sleep(K_MSEC(1000));
#endif

#ifdef CONFIG_FT_ENABLE_BUFFER_OVERFLOW_PROTECTION
    LOG_INF("Running buffer overflow protection test...");
    if (buffer_overflow_test() == 0) {
        LOG_INF("Buffer overflow test: PASSED");
    } else {
        LOG_ERR("Buffer overflow test: FAILED");
    }
    k_sleep(K_MSEC(1000));
#endif

#ifdef CONFIG_FT_ENABLE_DEADLOCK_DETECTION
    LOG_INF("Running deadlock detection test...");
    if (deadlock_test() == 0) {
        LOG_INF("Deadlock test: PASSED");
    } else {
        LOG_ERR("Deadlock test: FAILED");
    }
    k_sleep(K_MSEC(1000));
#endif

#ifdef CONFIG_FT_ENABLE_RACE_CONDITION_DETECTION
    LOG_INF("Running race condition detection test...");
    if (race_condition_test() == 0) {
        LOG_INF("Race condition test: PASSED");
    } else {
        LOG_ERR("Race condition test: FAILED");
    }
    k_sleep(K_MSEC(1000));
#endif

    LOG_INF("All enabled module tests completed");
}

/* Display statistics from all enabled modules */
static void display_module_statistics(void)
{
    LOG_INF("=== Module Statistics ===");

#ifdef CONFIG_FT_ENABLE_STACK_OVERFLOW_PROTECTION
    struct ft_stack_overflow_stats stack_stats;
    if (ft_stack_overflow_get_stats(&stack_stats) == 0) {
        LOG_INF("Stack Overflow Protection:");
        LOG_INF("  Threads monitored: %u", stack_stats.threads_monitored);
        LOG_INF("  Violations detected: %u", stack_stats.violations_detected);
        LOG_INF("  Threads suspended: %u", stack_stats.threads_suspended);
    }
#endif

#ifdef CONFIG_FT_ENABLE_BUFFER_OVERFLOW_PROTECTION
    struct ft_buffer_overflow_stats buffer_stats;
    if (ft_buffer_overflow_get_stats(&buffer_stats) == 0) {
        LOG_INF("Buffer Overflow Protection:");
        LOG_INF("  Buffers protected: %u", buffer_stats.buffers_protected);
        LOG_INF("  Violations detected: %u", buffer_stats.violations_detected);
        LOG_INF("  Repairs performed: %u", buffer_stats.repairs_performed);
    }
#endif

#ifdef CONFIG_FT_ENABLE_DEADLOCK_DETECTION
    struct ft_deadlock_stats deadlock_stats;
    if (ft_deadlock_get_stats(&deadlock_stats) == 0) {
        LOG_INF("Deadlock Detection:");
        LOG_INF("  Resources monitored: %u", deadlock_stats.resources_monitored);
        LOG_INF("  Deadlocks detected: %u", deadlock_stats.deadlocks_detected);
        LOG_INF("  Deadlocks resolved: %u", deadlock_stats.deadlocks_resolved);
    }
#endif

#ifdef CONFIG_FT_ENABLE_RACE_CONDITION_DETECTION
    struct ft_race_stats race_stats;
    if (ft_race_get_stats(&race_stats) == 0) {
        LOG_INF("Race Condition Detection:");
        LOG_INF("  Accesses monitored: %u", race_stats.total_accesses_monitored);
        LOG_INF("  Race conditions detected: %u", race_stats.race_conditions_detected);
        LOG_INF("  Race conditions prevented: %u", race_stats.race_conditions_prevented);
    }
#endif

    LOG_INF("========================");
}

/* Main application function */
int main(void)
{
    int ret;

    LOG_INF("Starting Modular Fault Tolerance Integration Test");
    
    /* Initialize fault tolerance framework */
    struct ft_config config = {0};
    ret = ft_init(&config);
    if (ret < 0) {
        LOG_ERR("Failed to initialize fault tolerance framework: %d", ret);
        return ret;
    }

    /* Display test configuration */
    display_test_configuration();
    
    /* Initialize all enabled modules */
    ret = initialize_modules();
    if (ret < 0) {
        LOG_ERR("Module initialization failed");
        return ret;
    }

    /* Let modules stabilize */
    k_sleep(K_MSEC(2000));

    /* Run tests for enabled modules */
    run_module_tests();

    /* Display final statistics */
    k_sleep(K_MSEC(1000));
    display_module_statistics();

    LOG_INF("Modular Fault Tolerance Integration Test completed");
    LOG_INF("System will continue running for monitoring...");

    /* Keep system running for continuous monitoring */
    while (1) {
        k_sleep(K_MSEC(10000));
        LOG_INF("System operational - fault tolerance active");
    }

    return 0;
}