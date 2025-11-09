/*
 * Copyright (c) 2025 Jack Ostapeic
 *
 * Race Condition Detection Test Module
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance/ft_race_condition.h>

LOG_MODULE_REGISTER(race_condition_test, LOG_LEVEL_INF);

/* Shared resource for testing */
static volatile int shared_counter = 0;
static volatile bool test_running = false;

/* Thread stacks and data */
#define THREAD_STACK_SIZE 1024
K_THREAD_STACK_DEFINE(reader1_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(reader2_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(writer1_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(writer2_stack, THREAD_STACK_SIZE);

static struct k_thread reader1_data, reader2_data;
static struct k_thread writer1_data, writer2_data;
static k_tid_t reader1_id, reader2_id, writer1_id, writer2_id;

/* Reader thread 1 */
static void race_reader1(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("Reader 1: Starting");

    while (test_running) {
        /* Report read access */
        FT_RACE_READ(&shared_counter, sizeof(shared_counter));
        
        volatile int local_copy = shared_counter;
        LOG_DBG("Reader 1: Counter = %d", local_copy);
        
        k_sleep(K_MSEC(50));
    }
    
    LOG_INF("Reader 1: Finished");
}

/* Reader thread 2 */
static void race_reader2(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("Reader 2: Starting");

    while (test_running) {
        /* Report read access */
        FT_RACE_READ(&shared_counter, sizeof(shared_counter));
        
        volatile int local_copy = shared_counter;
        LOG_DBG("Reader 2: Counter = %d", local_copy);
        
        k_sleep(K_MSEC(75));
    }
    
    LOG_INF("Reader 2: Finished");
}

/* Writer thread 1 */
static void race_writer1(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("Writer 1: Starting");
    
    int write_count = 0;

    while (test_running && write_count < 10) {
        /* Report write access */
        FT_RACE_WRITE(&shared_counter, sizeof(shared_counter));
        
        /* Simulate non-atomic operation */
        volatile int temp = shared_counter;
        k_sleep(K_USEC(100));  /* Small delay to increase race condition probability */
        shared_counter = temp + 1;
        
        write_count++;
        LOG_DBG("Writer 1: Incremented counter to %d", shared_counter);
        
        k_sleep(K_MSEC(100));
    }
    
    LOG_INF("Writer 1: Finished (%d writes)", write_count);
}

/* Writer thread 2 */
static void race_writer2(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("Writer 2: Starting");
    
    int write_count = 0;

    while (test_running && write_count < 10) {
        /* Report write access */
        FT_RACE_WRITE(&shared_counter, sizeof(shared_counter));
        
        /* Simulate non-atomic operation */
        volatile int temp = shared_counter;
        k_sleep(K_USEC(150));  /* Different timing to create race conditions */
        shared_counter = temp + 2;
        
        write_count++;
        LOG_DBG("Writer 2: Incremented counter to %d", shared_counter);
        
        k_sleep(K_MSEC(120));
    }
    
    LOG_INF("Writer 2: Finished (%d writes)", write_count);
}

/* Test the race condition detection module */
int race_condition_test(void)
{
    int ret;
    struct ft_race_stats stats;
    struct ft_race_detection detection;

    LOG_INF("=== Race Condition Detection Test ===");

    /* Register shared resource for monitoring */
    ret = ft_race_register_resource(&shared_counter, sizeof(shared_counter), "shared_counter");
    if (ret < 0) {
        LOG_ERR("Failed to register shared resource: %d", ret);
        return ret;
    }

    /* Initialize shared counter */
    shared_counter = 0;
    test_running = true;

    /* Create threads that will access shared resource */
    reader1_id = k_thread_create(&reader1_data, reader1_stack,
                                K_THREAD_STACK_SIZEOF(reader1_stack),
                                race_reader1, NULL, NULL, NULL,
                                K_PRIO_PREEMPT(6), 0, K_NO_WAIT);

    reader2_id = k_thread_create(&reader2_data, reader2_stack,
                                K_THREAD_STACK_SIZEOF(reader2_stack),
                                race_reader2, NULL, NULL, NULL,
                                K_PRIO_PREEMPT(6), 0, K_NO_WAIT);

    writer1_id = k_thread_create(&writer1_data, writer1_stack,
                                K_THREAD_STACK_SIZEOF(writer1_stack),
                                race_writer1, NULL, NULL, NULL,
                                K_PRIO_PREEMPT(5), 0, K_NO_WAIT);

    writer2_id = k_thread_create(&writer2_data, writer2_stack,
                                K_THREAD_STACK_SIZEOF(writer2_stack),
                                race_writer2, NULL, NULL, NULL,
                                K_PRIO_PREEMPT(5), 0, K_NO_WAIT);

    /* Register threads for monitoring */
    ft_race_register_thread(reader1_id, "reader_1");
    ft_race_register_thread(reader2_id, "reader_2");
    ft_race_register_thread(writer1_id, "writer_1");
    ft_race_register_thread(writer2_id, "writer_2");

    /* Let threads run and monitor for race conditions */
    LOG_INF("Monitoring for race conditions...");
    
    for (int i = 0; i < 50; i++) {  /* Monitor for 5 seconds */
        k_sleep(K_MSEC(100));
        
        /* Check for concurrent access patterns */
        ret = ft_race_check_concurrent_access(&detection);
        if (ret > 0) {
            LOG_INF("Race condition detected! Type: %d, Time diff: %llu us", 
                   detection.conflict_type, detection.time_difference);
        }
        
        /* Force race condition check */
        uint32_t races = ft_race_force_check();
        if (races > 0) {
            LOG_INF("Force check detected %u race conditions", races);
        }

        /* Update Lamport clock periodically */
        ft_race_update_lamport_clock(ft_race_get_lamport_clock() + 1);
    }

    /* Stop test threads */
    test_running = false;

    /* Wait for threads to complete */
    k_thread_join(reader1_id, K_MSEC(2000));
    k_thread_join(reader2_id, K_MSEC(2000));
    k_thread_join(writer1_id, K_MSEC(2000));
    k_thread_join(writer2_id, K_MSEC(2000));

    LOG_INF("Final counter value: %d", shared_counter);

    /* Test synchronization */
    LOG_INF("Testing synchronization...");
    ret = ft_race_synchronize_access(&shared_counter, 1000);
    if (ret == 0) {
        LOG_INF("Synchronization successful");
        
        /* Perform synchronized access */
        FT_RACE_WRITE(&shared_counter, sizeof(shared_counter));
        shared_counter = 1000;
        LOG_INF("Synchronized write: counter = %d", shared_counter);
    } else {
        LOG_ERR("Synchronization failed: %d", ret);
    }

    /* Get statistics */
    ret = ft_race_get_stats(&stats);
    if (ret == 0) {
        LOG_INF("Race condition detection statistics:");
        LOG_INF("  Accesses monitored: %u", stats.total_accesses_monitored);
        LOG_INF("  Concurrent accesses: %u", stats.concurrent_accesses_detected);
        LOG_INF("  Race conditions detected: %u", stats.race_conditions_detected);
        LOG_INF("  Race conditions prevented: %u", stats.race_conditions_prevented);
        LOG_INF("  Synchronizations performed: %u", stats.synchronizations_performed);
        LOG_INF("  Protected resources: %u", stats.protected_resources);
    }

    /* Clean up */
    ft_race_unregister_resource(&shared_counter);

    LOG_INF("Race condition detection test completed");
    return 0;
}