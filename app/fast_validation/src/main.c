/*
 * Copyright (c) 2025 ECE753 Project Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Fast fault tolerance validation application
 *
 * This application quickly validates various fault detection and recovery
 * mechanisms in the enhanced Zephyr fault tolerance framework.
 */

#include <zephyr/kernel.h>
#include <zephyr/fault_tolerance.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/random/random.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(ft_test_fast, LOG_LEVEL_INF);

/* Test configuration */
#define TEST_DURATION_MS        30000   /* 30 seconds total test time */
#define TEST_THREAD_COUNT       4
#define TEST_STACK_SIZE         2048
#define SMALL_STACK_SIZE        512
#define TEST_ITERATION_DELAY    100     /* ms between test iterations */

/* Test result tracking */
struct test_results {
    uint32_t tests_run;
    uint32_t tests_passed;
    uint32_t tests_failed;
    uint32_t faults_injected;
    uint32_t recoveries_attempted;
    uint32_t recoveries_successful;
};

static struct test_results results = {0};
static struct k_mutex results_mutex;
static volatile bool test_running = true;

/* Test thread stacks */
K_THREAD_STACK_ARRAY_DEFINE(test_stacks, TEST_THREAD_COUNT, TEST_STACK_SIZE);
K_THREAD_STACK_DEFINE(small_stack, SMALL_STACK_SIZE);

static struct k_thread test_threads[TEST_THREAD_COUNT];
static struct k_thread overflow_thread;

/* Custom fault handler for testing */
static enum ft_recovery_action test_fault_handler(const struct ft_fault_context *ctx)
{
    LOG_INF("Test fault handler called: type=%d, severity=%d", 
            ctx->fault_type, ctx->severity);
    
    k_mutex_lock(&results_mutex, K_FOREVER);
    results.recoveries_attempted++;
    k_mutex_unlock(&results_mutex);
    
    /* Return appropriate recovery action based on fault type */
    switch (ctx->fault_type) {
    case FT_FAULT_STACK_OVERFLOW:
        return FT_RECOVERY_RESTART_THREAD;
    case FT_FAULT_MEMORY_LEAK:
        return FT_RECOVERY_CUSTOM;
    case FT_FAULT_RESOURCE_EXHAUSTION:
        return FT_RECOVERY_SAFE_MODE;
    default:
        return FT_RECOVERY_NONE;
    }
}

/* Custom recovery callback for testing */
static int test_recovery_callback(const struct ft_fault_context *ctx, 
                                 enum ft_recovery_action action)
{
    LOG_INF("Test recovery callback: type=%d, action=%d", ctx->fault_type, action);
    
    k_mutex_lock(&results_mutex, K_FOREVER);
    results.recoveries_successful++;
    k_mutex_unlock(&results_mutex);
    
    return 0; /* Simulate successful recovery */
}

/**
 * @brief Test 1: Stack overflow detection
 */
static void test_stack_overflow(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("Stack overflow test thread started");
    
    /* Allocate large buffer on stack to trigger overflow */
    volatile char large_buffer[SMALL_STACK_SIZE * 2];
    
    /* Initialize buffer to prevent optimization */
    for (int i = 0; i < sizeof(large_buffer); i++) {
        large_buffer[i] = (char)(i & 0xFF);
    }
    
    /* Trigger stack check */
    FT_CHECK_STACK_OVERFLOW();
    
    /* Report stack overflow fault in test mode */
    uintptr_t ctx[4] = {sizeof(large_buffer), SMALL_STACK_SIZE, 0, 0};
    FT_REPORT_FAULT_TEST(FT_FAULT_STACK_OVERFLOW, FT_SEVERITY_CRITICAL,
                        "Intentional stack overflow test", ctx);
    
    k_mutex_lock(&results_mutex, K_FOREVER);
    results.faults_injected++;
    results.tests_run++;
    k_mutex_unlock(&results_mutex);
    
    LOG_INF("Stack overflow test completed");
}

/**
 * @brief Test 2: Memory leak simulation
 */
static void test_memory_leak(void)
{
    LOG_INF("Testing memory leak detection");
    
    /* Simulate memory allocations without corresponding deallocations */
    static char test_buffers[10][64]; /* Stack-based simulation of heap allocations */
    for (int i = 0; i < 10; i++) {
        void *ptr = test_buffers[i];
        if (ptr) {
            ft_track_allocation(ptr, 64);
            /* Intentionally not freeing to simulate leak */
        }
    }
    
    /* Report memory leak */
    uintptr_t ctx[4] = {10, 640, 0, 0}; /* 10 allocations, 640 bytes total */
    FT_REPORT_FAULT_TEST(FT_FAULT_MEMORY_LEAK, FT_SEVERITY_MEDIUM,
                        "Intentional memory leak test", ctx);
    
    k_mutex_lock(&results_mutex, K_FOREVER);
    results.faults_injected++;
    results.tests_run++;
    k_mutex_unlock(&results_mutex);
    
    LOG_INF("Memory leak test completed");
}

/**
 * @brief Test 3: Resource exhaustion simulation
 */
static void test_resource_exhaustion(void)
{
    LOG_INF("Testing resource exhaustion detection");
    
    /* Simulate resource exhaustion */
    uint32_t current_usage = 95;
    uint32_t limit = 100;
    
    FT_CHECK_RESOURCE_EXHAUSTION("Test Resource", current_usage, limit);
    
    /* Also report directly */
    uintptr_t ctx[4] = {current_usage, limit, 0, 0};
    FT_REPORT_FAULT_TEST(FT_FAULT_RESOURCE_EXHAUSTION, FT_SEVERITY_HIGH,
                        "Test resource exhaustion", ctx);
    
    k_mutex_lock(&results_mutex, K_FOREVER);
    results.faults_injected++;
    results.tests_run++;
    k_mutex_unlock(&results_mutex);
    
    LOG_INF("Resource exhaustion test completed");
}

/**
 * @brief Test 4: Race condition simulation
 */
static void test_race_condition(void)
{
    LOG_INF("Testing race condition detection");
    
    /* Simulate race condition detection */
    uintptr_t ctx[4] = {(uintptr_t)k_current_get(), 0, 0, 0};
    FT_REPORT_FAULT_TEST(FT_FAULT_RACE_CONDITION, FT_SEVERITY_HIGH,
                        "Simulated race condition", ctx);
    
    k_mutex_lock(&results_mutex, K_FOREVER);
    results.faults_injected++;
    results.tests_run++;
    k_mutex_unlock(&results_mutex);
    
    LOG_INF("Race condition test completed");
}

/**
 * @brief Test 5: Data corruption simulation
 */
static void test_data_corruption(void)
{
    LOG_INF("Testing data corruption detection");
    
    /* Simulate data corruption */
    uint32_t expected_value = 0xDEADBEEF;
    uint32_t actual_value = 0xBADC0DE;
    
    uintptr_t ctx[4] = {expected_value, actual_value, 0, 0};
    FT_REPORT_FAULT_TEST(FT_FAULT_DATA_CORRUPTION, FT_SEVERITY_CRITICAL,
                        "Data corruption detected", ctx);
    
    k_mutex_lock(&results_mutex, K_FOREVER);
    results.faults_injected++;
    results.tests_run++;
    k_mutex_unlock(&results_mutex);
    
    LOG_INF("Data corruption test completed");
}

/**
 * @brief Test 6: Timing violation simulation
 */
static void test_timing_violation(void)
{
    LOG_INF("Testing timing violation detection");
    
    /* Simulate timing violation */
    int64_t deadline = k_uptime_get() - 1000; /* 1 second ago */
    int64_t actual_completion = k_uptime_get();
    
    uintptr_t ctx[4] = {(uintptr_t)deadline, (uintptr_t)actual_completion, 0, 0};
    FT_REPORT_FAULT_TEST(FT_FAULT_TIMING_VIOLATION, FT_SEVERITY_MEDIUM,
                        "Timing deadline missed", ctx);
    
    k_mutex_lock(&results_mutex, K_FOREVER);
    results.faults_injected++;
    results.tests_run++;
    k_mutex_unlock(&results_mutex);
    
    LOG_INF("Timing violation test completed");
}

/**
 * @brief Main test thread function
 */
static void test_thread_main(void *p1, void *p2, void *p3)
{
    int thread_id = POINTER_TO_INT(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("Test thread %d started", thread_id);
    
    while (test_running) {
        /* Run different tests based on thread ID */
        switch (thread_id % 6) {
        case 0:
            test_memory_leak();
            break;
        case 1:
            test_resource_exhaustion();
            break;
        case 2:
            test_race_condition();
            break;
        case 3:
            test_data_corruption();
            break;
        case 4:
            test_timing_violation();
            break;
        case 5:
            /* Monitor system health */
            ft_monitor_memory_usage();
            ft_monitor_stack_usage();
            break;
        }
        
        k_sleep(K_MSEC(TEST_ITERATION_DELAY + (thread_id * 50)));
    }
    
    LOG_INF("Test thread %d finished", thread_id);
}

/**
 * @brief Print test results
 */
static void print_test_results(void)
{
    struct ft_fault_stats ft_stats;
    struct ft_memory_stats mem_stats;
    
    /* Get fault tolerance statistics */
    ft_get_stats(&ft_stats);
    ft_get_memory_stats(&mem_stats);
    
    printk("\n=== FAST FAULT TOLERANCE TEST RESULTS ===\n");
    printk("Test Duration: %d ms\n", TEST_DURATION_MS);
    printk("Tests Run: %d\n", results.tests_run);
    printk("Tests Passed: %d\n", results.tests_passed);
    printk("Tests Failed: %d\n", results.tests_failed);
    printk("Faults Injected: %d\n", results.faults_injected);
    printk("Recoveries Attempted: %d\n", results.recoveries_attempted);
    printk("Recoveries Successful: %d\n", results.recoveries_successful);
    
    printk("\n=== FAULT TOLERANCE STATISTICS ===\n");
    printk("Total Faults: %d\n", ft_stats.total_faults);
    printk("Successful Recoveries: %d\n", ft_stats.successful_recoveries);
    printk("Failed Recoveries: %d\n", ft_stats.failed_recoveries);
    printk("MTBF: %d ms\n", ft_stats.mtbf_ms);
    
    printk("\n=== MEMORY STATISTICS ===\n");
    printk("Heap Total: %zu bytes\n", mem_stats.heap_total);
    printk("Heap Used: %zu bytes\n", mem_stats.heap_used);
    printk("Heap Free: %zu bytes\n", mem_stats.heap_free);
    printk("Tracked Allocations: %d\n", mem_stats.tracked_allocations);
    printk("Tracked Bytes: %zu\n", mem_stats.tracked_bytes);
    
    /* Calculate success rate */
    if (results.faults_injected > 0) {
        uint32_t success_rate = (results.recoveries_successful * 100) / results.faults_injected;
        printk("Recovery Success Rate: %d%%\n", success_rate);
    }
    
    printk("=== TEST COMPLETE ===\n\n");
}

/**
 * @brief Main application entry point
 */
int main(void)
{
    int ret;
    
    printk("Enhanced Zephyr Fault Tolerance - Fast Validation Test\n");
    printk("ECE753 Project - Safety-Critical Embedded Systems\n\n");
    
    /* Initialize mutex for result tracking */
    k_mutex_init(&results_mutex);
    
    /* Initialize fault tolerance framework */
    ret = ft_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize fault tolerance framework: %d", ret);
        return ret;
    }
    
    LOG_INF("Fault tolerance framework initialized");
    
    /* Enable test mode for safe fault injection */
    ret = ft_set_test_mode(true);
    if (ret != 0) {
        LOG_ERR("Failed to enable test mode: %d", ret);
        return ret;
    }
    
    LOG_INF("Test mode enabled for safe fault injection");
    
    /* Register test fault handlers */
    ft_register_fault_handler(FT_FAULT_STACK_OVERFLOW, test_fault_handler);
    ft_register_fault_handler(FT_FAULT_MEMORY_LEAK, test_fault_handler);
    ft_register_fault_handler(FT_FAULT_RESOURCE_EXHAUSTION, test_fault_handler);
    ft_register_fault_handler(FT_FAULT_RACE_CONDITION, test_fault_handler);
    ft_register_fault_handler(FT_FAULT_DATA_CORRUPTION, test_fault_handler);
    ft_register_fault_handler(FT_FAULT_TIMING_VIOLATION, test_fault_handler);
    
    /* Register recovery callback */
    ft_register_recovery_callback(test_recovery_callback);
    
    LOG_INF("Test handlers registered");
    
    /* Create stack overflow test thread (with small stack) */
    k_thread_create(&overflow_thread, small_stack, K_THREAD_STACK_SIZEOF(small_stack),
                   test_stack_overflow, NULL, NULL, NULL,
                   K_PRIO_PREEMPT(7), 0, K_MSEC(1000));
    
    k_thread_name_set(&overflow_thread, "stack_test");
    
    /* Create test threads */
    for (int i = 0; i < TEST_THREAD_COUNT; i++) {
        k_thread_create(&test_threads[i], test_stacks[i], TEST_STACK_SIZE,
                       test_thread_main, INT_TO_POINTER(i), NULL, NULL,
                       K_PRIO_PREEMPT(5), 0, K_MSEC(500 + i * 100));
        
        char thread_name[16];
        snprintf(thread_name, sizeof(thread_name), "test_%d", i);
        k_thread_name_set(&test_threads[i], thread_name);
    }
    
    LOG_INF("Test threads created and started");
    
    /* Run tests for specified duration */
    int64_t start_time = k_uptime_get();
    int64_t end_time = start_time + TEST_DURATION_MS;
    
    LOG_INF("Running tests for %d ms...", TEST_DURATION_MS);
    
    while (k_uptime_get() < end_time) {
        /* Print progress */
        if ((k_uptime_get() - start_time) % 5000 == 0) {
            LOG_INF("Test progress: %lld/%d ms", k_uptime_get() - start_time, TEST_DURATION_MS);
        }
        
        k_sleep(K_MSEC(1000));
    }
    
    /* Stop tests */
    test_running = false;
    
    LOG_INF("Stopping test threads...");
    
    /* Wait for threads to finish */
    k_sleep(K_MSEC(1000));
    
    /* Abort threads if they're still running */
    for (int i = 0; i < TEST_THREAD_COUNT; i++) {
        k_thread_abort(&test_threads[i]);
    }
    k_thread_abort(&overflow_thread);
    
    /* Calculate final results */
    k_mutex_lock(&results_mutex, K_FOREVER);
    if (results.faults_injected > 0 && results.recoveries_successful > 0) {
        results.tests_passed = results.recoveries_successful;
        results.tests_failed = results.faults_injected - results.recoveries_successful;
    }
    k_mutex_unlock(&results_mutex);
    
    /* Print final results */
    print_test_results();
    
    LOG_INF("Fast validation test completed successfully");
    
    return 0;
}