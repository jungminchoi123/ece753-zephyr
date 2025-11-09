/*
 * Copyright (c) 2025 Jack Ostapeic
 *
 * Deadlock Detection Test Module
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance/ft_deadlock.h>

LOG_MODULE_REGISTER(deadlock_test, LOG_LEVEL_INF);

/* Test mutexes */
static struct k_mutex mutex_a;
static struct k_mutex mutex_b;

/* Thread stacks and data */
#define THREAD_STACK_SIZE 1024
K_THREAD_STACK_DEFINE(thread1_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(thread2_stack, THREAD_STACK_SIZE);
static struct k_thread thread1_data, thread2_data;
static k_tid_t thread1_id, thread2_id;

/* Thread 1: acquires mutex_a, then tries to acquire mutex_b */
static void deadlock_thread1(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("Thread 1: Starting");

    /* Report wait for mutex_a */
    ft_deadlock_report_wait(k_current_get(), &mutex_a);
    
    LOG_INF("Thread 1: Acquiring mutex A");
    k_mutex_lock(&mutex_a, K_FOREVER);
    
    /* Report successful acquisition */
    ft_deadlock_report_acquire(k_current_get(), &mutex_a);
    LOG_INF("Thread 1: Acquired mutex A");

    /* Delay to let thread 2 acquire mutex_b */
    k_sleep(K_MSEC(100));

    /* Now try to acquire mutex_b (potential deadlock) */
    LOG_INF("Thread 1: Trying to acquire mutex B");
    ft_deadlock_report_wait(k_current_get(), &mutex_b);
    
    int ret = k_mutex_lock(&mutex_b, K_MSEC(2000));
    if (ret == 0) {
        ft_deadlock_report_acquire(k_current_get(), &mutex_b);
        LOG_INF("Thread 1: Acquired mutex B");
        k_mutex_unlock(&mutex_b);
        ft_deadlock_report_release(k_current_get(), &mutex_b);
    } else {
        LOG_INF("Thread 1: Failed to acquire mutex B (timeout)");
    }

    k_mutex_unlock(&mutex_a);
    ft_deadlock_report_release(k_current_get(), &mutex_a);
    LOG_INF("Thread 1: Released mutex A");
}

/* Thread 2: acquires mutex_b, then tries to acquire mutex_a */
static void deadlock_thread2(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("Thread 2: Starting");

    /* Small delay to let thread 1 start first */
    k_sleep(K_MSEC(50));

    /* Report wait for mutex_b */
    ft_deadlock_report_wait(k_current_get(), &mutex_b);
    
    LOG_INF("Thread 2: Acquiring mutex B");
    k_mutex_lock(&mutex_b, K_FOREVER);
    
    /* Report successful acquisition */
    ft_deadlock_report_acquire(k_current_get(), &mutex_b);
    LOG_INF("Thread 2: Acquired mutex B");

    /* Delay to create deadlock scenario */
    k_sleep(K_MSEC(100));

    /* Now try to acquire mutex_a (potential deadlock) */
    LOG_INF("Thread 2: Trying to acquire mutex A");
    ft_deadlock_report_wait(k_current_get(), &mutex_a);
    
    int ret = k_mutex_lock(&mutex_a, K_MSEC(2000));
    if (ret == 0) {
        ft_deadlock_report_acquire(k_current_get(), &mutex_a);
        LOG_INF("Thread 2: Acquired mutex A");
        k_mutex_unlock(&mutex_a);
        ft_deadlock_report_release(k_current_get(), &mutex_a);
    } else {
        LOG_INF("Thread 2: Failed to acquire mutex A (timeout)");
    }

    k_mutex_unlock(&mutex_b);
    ft_deadlock_report_release(k_current_get(), &mutex_b);
    LOG_INF("Thread 2: Released mutex B");
}

/* Test the deadlock detection module */
int deadlock_test(void)
{
    int ret;
    struct ft_deadlock_stats stats;

    LOG_INF("=== Deadlock Detection Test ===");

    /* Initialize mutexes */
    k_mutex_init(&mutex_a);
    k_mutex_init(&mutex_b);

    /* Register resources for monitoring */
    ret = ft_deadlock_register_resource(&mutex_a, "mutex_a");
    if (ret < 0) {
        LOG_ERR("Failed to register mutex A: %d", ret);
        return ret;
    }

    ret = ft_deadlock_register_resource(&mutex_b, "mutex_b");
    if (ret < 0) {
        LOG_ERR("Failed to register mutex B: %d", ret);
        return ret;
    }

    /* Create threads that will potentially deadlock */
    thread1_id = k_thread_create(&thread1_data, thread1_stack,
                                K_THREAD_STACK_SIZEOF(thread1_stack),
                                deadlock_thread1, NULL, NULL, NULL,
                                K_PRIO_PREEMPT(5), 0, K_NO_WAIT);

    thread2_id = k_thread_create(&thread2_data, thread2_stack,
                                K_THREAD_STACK_SIZEOF(thread2_stack),
                                deadlock_thread2, NULL, NULL, NULL,
                                K_PRIO_PREEMPT(5), 0, K_NO_WAIT);

    /* Register threads for monitoring */
    ret = ft_deadlock_register_thread(thread1_id, "deadlock_thread_1");
    if (ret < 0) {
        LOG_ERR("Failed to register thread 1: %d", ret);
    }

    ret = ft_deadlock_register_thread(thread2_id, "deadlock_thread_2");
    if (ret < 0) {
        LOG_ERR("Failed to register thread 2: %d", ret);
    }

    /* Let threads run and monitor for deadlocks */
    LOG_INF("Monitoring for deadlocks...");
    
    for (int i = 0; i < 30; i++) {  /* Monitor for 3 seconds */
        k_sleep(K_MSEC(100));
        
        /* Force deadlock check */
        uint32_t deadlocks = ft_deadlock_force_check();
        if (deadlocks > 0) {
            LOG_INF("Deadlock detection found %u potential deadlocks", deadlocks);
        }
    }

    /* Wait for threads to complete */
    k_thread_join(thread1_id, K_MSEC(5000));
    k_thread_join(thread2_id, K_MSEC(5000));

    /* Check for circular dependencies */
    uint32_t circular = ft_deadlock_check_circular_dependencies();
    LOG_INF("Circular dependency check found %u instances", circular);

    /* Check for timeout-based deadlocks */
    uint32_t timeouts = ft_deadlock_check_timeouts();
    LOG_INF("Timeout-based deadlock check found %u instances", timeouts);

    /* Get statistics */
    ret = ft_deadlock_get_stats(&stats);
    if (ret == 0) {
        LOG_INF("Deadlock detection statistics:");
        LOG_INF("  Resources monitored: %u", stats.resources_monitored);
        LOG_INF("  Threads monitored: %u", stats.threads_monitored);
        LOG_INF("  Deadlocks detected: %u", stats.deadlocks_detected);
        LOG_INF("  Deadlocks resolved: %u", stats.deadlocks_resolved);
        LOG_INF("  Resource preemptions: %u", stats.resource_preemptions);
    }

    /* Clean up */
    ft_deadlock_unregister_resource(&mutex_a);
    ft_deadlock_unregister_resource(&mutex_b);
    ft_deadlock_unregister_thread(thread1_id);
    ft_deadlock_unregister_thread(thread2_id);

    LOG_INF("Deadlock detection test completed");
    return 0;
}