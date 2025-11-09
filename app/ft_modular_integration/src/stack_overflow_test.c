/*
 * Copyright (c) 2025 Jack Ostapeic
 *
 * Stack Overflow Protection Test Module
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance/ft_stack_overflow.h>

LOG_MODULE_REGISTER(stack_overflow_test, LOG_LEVEL_INF);

/* Test thread that will consume stack space */
static void stack_consuming_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    char large_buffer[1024];
    volatile int counter = 0;

    LOG_INF("Stack consuming thread started");

    /* Fill buffer to use stack space */
    for (int i = 0; i < sizeof(large_buffer); i++) {
        large_buffer[i] = (char)(i & 0xFF);
        counter++;
        
        /* Let monitoring happen */
        if (counter % 100 == 0) {
            k_sleep(K_MSEC(10));
        }
    }

    LOG_INF("Stack consuming thread completed normally");
}

/* Thread stack and ID */
#define STACK_SIZE 2048
K_THREAD_STACK_DEFINE(test_thread_stack, STACK_SIZE);
static struct k_thread test_thread_data;
static k_tid_t test_thread_id;

/* Test the stack overflow protection module */
int stack_overflow_test(void)
{
    int ret;
    struct ft_stack_overflow_stats stats;

    LOG_INF("=== Stack Overflow Protection Test ===");

    /* Register current thread for monitoring */
    ret = ft_stack_overflow_register_thread(k_current_get(), "main_thread");
    if (ret < 0) {
        LOG_ERR("Failed to register main thread: %d", ret);
        return ret;
    }

    /* Create test thread */
    test_thread_id = k_thread_create(&test_thread_data, test_thread_stack,
                                   K_THREAD_STACK_SIZEOF(test_thread_stack),
                                   stack_consuming_thread,
                                   NULL, NULL, NULL,
                                   K_PRIO_PREEMPT(7), 0, K_NO_WAIT);

    /* Register test thread for monitoring */
    ret = ft_stack_overflow_register_thread(test_thread_id, "stack_test_thread");
    if (ret < 0) {
        LOG_ERR("Failed to register test thread: %d", ret);
        k_thread_abort(test_thread_id);
        return ret;
    }

    /* Let test thread run and be monitored */
    k_sleep(K_MSEC(3000));

    /* Get statistics */
    ret = ft_stack_overflow_get_stats(&stats);
    if (ret == 0) {
        LOG_INF("Stack overflow statistics:");
        LOG_INF("  Threads monitored: %u", stats.threads_monitored);
        LOG_INF("  Checks performed: %u", stats.checks_performed);
        LOG_INF("  Violations detected: %u", stats.violations_detected);
        LOG_INF("  Threads suspended: %u", stats.threads_suspended);
        LOG_INF("  False positives: %u", stats.false_positives);
    }

    /* Clean up */
    k_thread_abort(test_thread_id);
    ft_stack_overflow_unregister_thread(test_thread_id);
    ft_stack_overflow_unregister_thread(k_current_get());

    LOG_INF("Stack overflow protection test completed");
    return 0;
}