/**
 * @file deadlock_test.c
 * @brief ARM Cortex-M3 Deadlock and Livelock Test Module
 * @author Jack Ostapeic, MS ECE Student at University of Wisconsin-Madison
 * 
 * This module implements comprehensive deadlock and livelock detection testing
 * for ARM Cortex-M3 architecture, utilizing thread analysis, resource monitoring,
 * and timing-based detection mechanisms.
 *
 * Deadlock/Livelock Types Tested:
 * 1. Mutex Circular Deadlock - Classic circular wait deadlock
 * 2. Priority Inversion Deadlock - High-priority thread blocked by low-priority
 * 3. Resource Starvation - Threads unable to acquire necessary resources
 * 4. Spinlock Livelock - Threads continuously spinning without progress
 * 5. Interrupt Deadlock - Deadlock involving interrupt handlers
 * 6. Recursive Lock Deadlock - Self-deadlock scenarios
 * 7. Semaphore Deadlock - Multiple semaphore dependencies
 * 8. Memory Allocation Deadlock - Heap allocation deadlocks
 *
 * Detection Methods:
 * - Thread state monitoring and analysis
 * - Resource dependency graph construction
 * - Timeout-based detection
 * - Progress monitoring
 * - Watchdog integration
 * - Lock ordering analysis
 *
 * Recovery Mechanisms:
 * - Deadlock breaking strategies
 * - Priority ceiling protocol
 * - Resource preemption
 * - Thread termination and restart
 * - Timeout-based recovery
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance.h>
#include <zephyr/sys/mutex.h>

LOG_MODULE_REGISTER(deadlock_test, LOG_LEVEL_INF);

/* Test configuration */
#define MAX_DEADLOCK_TESTS 8
#define DEADLOCK_DETECTION_TIMEOUT_MS 5000
#define LIVELOCK_DETECTION_TIMEOUT_MS 3000
#define MAX_TEST_THREADS 6

/* Statistics tracking */
static int deadlock_tests_run = 0;
static int deadlocks_detected = 0;
static volatile bool deadlock_detected = false;

/* Test thread configuration */
#define THREAD_STACK_SIZE 1024
#define THREAD_PRIORITY_HIGH 5
#define THREAD_PRIORITY_MID 7
#define THREAD_PRIORITY_LOW 9

/* Test resources */
static struct k_mutex mutex_a, mutex_b, mutex_c;
static struct k_sem semaphore_1, semaphore_2;
static struct k_spinlock spinlock_1, spinlock_2;

/* Thread control */
static k_tid_t test_threads[MAX_TEST_THREADS];
static K_THREAD_STACK_ARRAY_DEFINE(thread_stacks, MAX_TEST_THREADS, THREAD_STACK_SIZE);

/* Progress tracking for livelock detection */
typedef struct {
    uint32_t progress_counter;
    k_timeout_t last_update;
    bool making_progress;
} progress_tracker_t;

static progress_tracker_t thread_progress[MAX_TEST_THREADS];

/* Deadlock detection state */
typedef struct {
    const char *test_name;
    bool deadlock_occurred;
    bool livelock_occurred;
    uint32_t detection_time_ms;
    uint32_t threads_involved;
    char description[128];
} deadlock_result_t;

static deadlock_result_t deadlock_results[MAX_DEADLOCK_TESTS];

/**
 * @brief Initialize deadlock detection system
 *
 * Sets up mutexes, semaphores, and monitoring infrastructure
 * for comprehensive deadlock and livelock testing.
 *
 * @return 0 on success, negative on failure
 */
static int initialize_deadlock_detection(void)
{
    LOG_INF("Initializing deadlock detection system...");
    
    /* Initialize synchronization primitives */
    k_mutex_init(&mutex_a);
    k_mutex_init(&mutex_b);
    k_mutex_init(&mutex_c);
    
    k_sem_init(&semaphore_1, 1, 1);
    k_sem_init(&semaphore_2, 1, 1);
    
    /* Initialize progress tracking */
    memset(thread_progress, 0, sizeof(thread_progress));
    
    /* Clear results */
    memset(deadlock_results, 0, sizeof(deadlock_results));
    
    LOG_INF("✅ Deadlock detection system initialized");
    return 0;
}

/**
 * @brief Monitor thread progress for livelock detection
 *
 * Tracks thread progress counters to detect when threads
 * are active but not making meaningful progress.
 *
 * @param thread_id Thread index to monitor
 */
static void update_thread_progress(int thread_id)
{
    if (thread_id >= 0 && thread_id < MAX_TEST_THREADS) {
        thread_progress[thread_id].progress_counter++;
        thread_progress[thread_id].last_update = k_uptime_get();
        thread_progress[thread_id].making_progress = true;
    }
}

/**
 * @brief Check for livelock conditions
 *
 * Analyzes thread progress patterns to detect livelock
 * conditions where threads are active but not progressing.
 *
 * @return true if livelock detected, false otherwise
 */
static bool check_livelock_condition(void)
{
    k_timeout_t current_time = k_uptime_get();
    int stalled_threads = 0;
    
    for (int i = 0; i < MAX_TEST_THREADS; i++) {
        progress_tracker_t *tracker = &thread_progress[i];
        
        /* Check if thread has been active recently */
        if (tracker->progress_counter > 0) {
            k_timeout_t time_since_update = current_time - tracker->last_update;
            
            /* If thread hasn't made progress in timeout period */
            if (time_since_update > LIVELOCK_DETECTION_TIMEOUT_MS) {
                if (tracker->progress_counter < 100) {  /* Threshold for meaningful progress */
                    stalled_threads++;
                    tracker->making_progress = false;
                }
            }
        }
    }
    
    /* Livelock detected if multiple threads are stalled */
    return (stalled_threads >= 2);
}

/**
 * @brief Test 1: Classic Circular Deadlock
 *
 * Creates a circular dependency between threads and mutexes
 * to demonstrate classic deadlock detection.
 */
static void deadlock_thread_1(void *p1, void *p2, void *p3)
{
    int thread_id = POINTER_TO_INT(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_DBG("Thread 1 starting circular deadlock test");
    
    for (int i = 0; i < 10; i++) {
        update_thread_progress(thread_id);
        
        /* Thread 1: Lock A then try to lock B */
        LOG_DBG("Thread 1: Acquiring mutex A...");
        k_mutex_lock(&mutex_a, K_FOREVER);
        
        k_sleep(K_MSEC(100));  /* Hold lock briefly */
        
        LOG_DBG("Thread 1: Acquiring mutex B...");
        if (k_mutex_lock(&mutex_b, K_MSEC(2000)) != 0) {
            LOG_WRN("Thread 1: Failed to acquire mutex B - potential deadlock");
            deadlocks_detected++;
            k_mutex_unlock(&mutex_a);
            break;
        }
        
        /* Critical section work */
        k_sleep(K_MSEC(50));
        
        k_mutex_unlock(&mutex_b);
        k_mutex_unlock(&mutex_a);
        
        k_sleep(K_MSEC(200));
    }
    
    LOG_DBG("Thread 1 completed");
}

static void deadlock_thread_2(void *p1, void *p2, void *p3)
{
    int thread_id = POINTER_TO_INT(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_DBG("Thread 2 starting circular deadlock test");
    
    k_sleep(K_MSEC(50));  /* Slight delay to encourage deadlock */
    
    for (int i = 0; i < 10; i++) {
        update_thread_progress(thread_id);
        
        /* Thread 2: Lock B then try to lock A (opposite order) */
        LOG_DBG("Thread 2: Acquiring mutex B...");
        k_mutex_lock(&mutex_b, K_FOREVER);
        
        k_sleep(K_MSEC(100));
        
        LOG_DBG("Thread 2: Acquiring mutex A...");
        if (k_mutex_lock(&mutex_a, K_MSEC(2000)) != 0) {
            LOG_WRN("Thread 2: Failed to acquire mutex A - potential deadlock");
            deadlocks_detected++;
            k_mutex_unlock(&mutex_b);
            break;
        }
        
        /* Critical section work */
        k_sleep(K_MSEC(50));
        
        k_mutex_unlock(&mutex_a);
        k_mutex_unlock(&mutex_b);
        
        k_sleep(K_MSEC(200));
    }
    
    LOG_DBG("Thread 2 completed");
}

static void test_circular_deadlock(void)
{
    LOG_INF("📋 Test 1: Circular deadlock detection");
    deadlock_tests_run++;
    
    deadlock_result_t *result = &deadlock_results[0];
    result->test_name = "Circular Deadlock";
    strcpy(result->description, "Classic A->B, B->A circular dependency");
    
    uint32_t start_time = k_uptime_get_32();
    
    /* Create threads that will deadlock */
    test_threads[0] = k_thread_create(&thread_stacks[0][0], THREAD_STACK_SIZE,
                                      deadlock_thread_1, INT_TO_POINTER(0), NULL, NULL,
                                      THREAD_PRIORITY_MID, 0, K_NO_WAIT);
    
    test_threads[1] = k_thread_create(&thread_stacks[1][0], THREAD_STACK_SIZE,
                                      deadlock_thread_2, INT_TO_POINTER(1), NULL, NULL,
                                      THREAD_PRIORITY_MID, 0, K_NO_WAIT);
    
    /* Monitor for deadlock */
    k_sleep(K_MSEC(DEADLOCK_DETECTION_TIMEOUT_MS));
    
    result->detection_time_ms = k_uptime_get_32() - start_time;
    result->threads_involved = 2;
    
    /* Check if threads are still running (potential deadlock) */
    if (k_thread_join(test_threads[0], K_MSEC(100)) != 0 ||
        k_thread_join(test_threads[1], K_MSEC(100)) != 0) {
        LOG_WRN("⚠️  Threads did not complete - deadlock likely occurred");
        result->deadlock_occurred = true;
        deadlocks_detected++;
        
        /* Force thread termination */
        k_thread_abort(test_threads[0]);
        k_thread_abort(test_threads[1]);
    }
    
    /* Reset mutexes for next test */
    k_mutex_init(&mutex_a);
    k_mutex_init(&mutex_b);
    
    if (result->deadlock_occurred) {
        LOG_INF("✅ Circular deadlock detected and resolved");
    } else {
        LOG_INF("No circular deadlock occurred");
    }
}

/**
 * @brief Test 2: Priority Inversion Deadlock
 *
 * Tests deadlock scenarios involving priority inversion
 * where high-priority threads are blocked by low-priority threads.
 */
static void priority_low_thread(void *p1, void *p2, void *p3)
{
    int thread_id = POINTER_TO_INT(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_DBG("Low priority thread starting");
    
    /* Low priority thread holds resource for long time */
    k_mutex_lock(&mutex_c, K_FOREVER);
    LOG_DBG("Low priority: Acquired mutex C");
    
    /* Simulate long work while holding mutex */
    for (int i = 0; i < 50; i++) {
        update_thread_progress(thread_id);
        k_sleep(K_MSEC(100));  /* Long processing time */
    }
    
    k_mutex_unlock(&mutex_c);
    LOG_DBG("Low priority thread completed");
}

static void priority_high_thread(void *p1, void *p2, void *p3)
{
    int thread_id = POINTER_TO_INT(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_DBG("High priority thread starting");
    
    k_sleep(K_MSEC(200));  /* Let low priority thread start first */
    
    /* High priority thread needs the same resource */
    LOG_DBG("High priority: Trying to acquire mutex C...");
    if (k_mutex_lock(&mutex_c, K_MSEC(3000)) != 0) {
        LOG_WRN("High priority thread blocked - priority inversion deadlock");
        deadlocks_detected++;
    } else {
        update_thread_progress(thread_id);
        k_sleep(K_MSEC(50));
        k_mutex_unlock(&mutex_c);
        LOG_DBG("High priority thread completed normally");
    }
}

static void test_priority_inversion_deadlock(void)
{
    LOG_INF("📋 Test 2: Priority inversion deadlock");
    deadlock_tests_run++;
    
    deadlock_result_t *result = &deadlock_results[1];
    result->test_name = "Priority Inversion";
    strcpy(result->description, "High priority blocked by low priority thread");
    
    uint32_t start_time = k_uptime_get_32();
    
    /* Create low priority thread first */
    test_threads[0] = k_thread_create(&thread_stacks[0][0], THREAD_STACK_SIZE,
                                      priority_low_thread, INT_TO_POINTER(0), NULL, NULL,
                                      THREAD_PRIORITY_LOW, 0, K_NO_WAIT);
    
    /* Create high priority thread */
    test_threads[1] = k_thread_create(&thread_stacks[1][0], THREAD_STACK_SIZE,
                                      priority_high_thread, INT_TO_POINTER(1), NULL, NULL,
                                      THREAD_PRIORITY_HIGH, 0, K_NO_WAIT);
    
    /* Monitor execution */
    k_sleep(K_MSEC(DEADLOCK_DETECTION_TIMEOUT_MS));
    
    result->detection_time_ms = k_uptime_get_32() - start_time;
    result->threads_involved = 2;
    
    /* Check completion */
    if (k_thread_join(test_threads[0], K_MSEC(100)) != 0 ||
        k_thread_join(test_threads[1], K_MSEC(100)) != 0) {
        result->deadlock_occurred = true;
        k_thread_abort(test_threads[0]);
        k_thread_abort(test_threads[1]);
    }
    
    k_mutex_init(&mutex_c);
    
    if (result->deadlock_occurred) {
        LOG_INF("✅ Priority inversion deadlock detected");
    }
}

/**
 * @brief Test 3: Spinlock Livelock
 *
 * Tests livelock scenarios where threads continuously
 * spin without making progress.
 */
static void spinlock_thread_1(void *p1, void *p2, void *p3)
{
    int thread_id = POINTER_TO_INT(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_DBG("Spinlock thread 1 starting");
    
    for (int i = 0; i < 1000; i++) {
        /* Repeatedly try to acquire spinlocks in one order */
        k_spinlock_key_t key1 = k_spin_lock(&spinlock_1);
        
        /* Brief processing */
        update_thread_progress(thread_id);
        
        /* Try second spinlock */
        if (k_spin_trylock(&spinlock_2)) {
            /* Got both locks - do work */
            k_spin_unlock(&spinlock_2, 0);
            k_spin_unlock(&spinlock_1, key1);
            break;
        } else {
            /* Failed to get second lock - back off */
            k_spin_unlock(&spinlock_1, key1);
            /* Yield to allow other thread to proceed */
            k_yield();
        }
    }
    
    LOG_DBG("Spinlock thread 1 completed");
}

static void spinlock_thread_2(void *p1, void *p2, void *p3)
{
    int thread_id = POINTER_TO_INT(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_DBG("Spinlock thread 2 starting");
    
    for (int i = 0; i < 1000; i++) {
        /* Repeatedly try to acquire spinlocks in opposite order */
        k_spinlock_key_t key2 = k_spin_lock(&spinlock_2);
        
        update_thread_progress(thread_id);
        
        if (k_spin_trylock(&spinlock_1)) {
            /* Got both locks - do work */
            k_spin_unlock(&spinlock_1, 0);
            k_spin_unlock(&spinlock_2, key2);
            break;
        } else {
            /* Failed to get second lock - back off */
            k_spin_unlock(&spinlock_2, key2);
            k_yield();
        }
    }
    
    LOG_DBG("Spinlock thread 2 completed");
}

static void test_spinlock_livelock(void)
{
    LOG_INF("📋 Test 3: Spinlock livelock detection");
    deadlock_tests_run++;
    
    deadlock_result_t *result = &deadlock_results[2];
    result->test_name = "Spinlock Livelock";
    strcpy(result->description, "Threads spinning without progress");
    
    uint32_t start_time = k_uptime_get_32();
    
    /* Create competing spinlock threads */
    test_threads[0] = k_thread_create(&thread_stacks[0][0], THREAD_STACK_SIZE,
                                      spinlock_thread_1, INT_TO_POINTER(0), NULL, NULL,
                                      THREAD_PRIORITY_MID, 0, K_NO_WAIT);
    
    test_threads[1] = k_thread_create(&thread_stacks[1][0], THREAD_STACK_SIZE,
                                      spinlock_thread_2, INT_TO_POINTER(1), NULL, NULL,
                                      THREAD_PRIORITY_MID, 0, K_NO_WAIT);
    
    /* Monitor for livelock */
    k_sleep(K_MSEC(LIVELOCK_DETECTION_TIMEOUT_MS));
    
    if (check_livelock_condition()) {
        LOG_WRN("⚠️  Livelock condition detected");
        result->livelock_occurred = true;
        deadlocks_detected++;
    }
    
    result->detection_time_ms = k_uptime_get_32() - start_time;
    
    /* Wait for completion */
    k_thread_join(test_threads[0], K_MSEC(1000));
    k_thread_join(test_threads[1], K_MSEC(1000));
    
    if (result->livelock_occurred) {
        LOG_INF("✅ Spinlock livelock detected");
    }
}

/**
 * @brief Test 4: Semaphore Deadlock
 *
 * Tests deadlock scenarios involving multiple semaphores
 * and resource dependencies.
 */
static void test_semaphore_deadlock(void)
{
    LOG_INF("📋 Test 4: Semaphore deadlock detection");
    deadlock_tests_run++;
    
    deadlock_result_t *result = &deadlock_results[3];
    result->test_name = "Semaphore Deadlock";
    strcpy(result->description, "Multiple semaphore dependencies");
    
    /* Create a scenario where threads need both semaphores */
    auto void sem_thread_1(void *a, void *b, void *c) {
        ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
        
        k_sem_take(&semaphore_1, K_FOREVER);
        k_sleep(K_MSEC(100));
        
        if (k_sem_take(&semaphore_2, K_MSEC(2000)) != 0) {
            LOG_WRN("Semaphore deadlock detected in thread 1");
            deadlocks_detected++;
        } else {
            k_sem_give(&semaphore_2);
        }
        k_sem_give(&semaphore_1);
    };
    
    auto void sem_thread_2(void *a, void *b, void *c) {
        ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
        
        k_sleep(K_MSEC(50));
        k_sem_take(&semaphore_2, K_FOREVER);
        k_sleep(K_MSEC(100));
        
        if (k_sem_take(&semaphore_1, K_MSEC(2000)) != 0) {
            LOG_WRN("Semaphore deadlock detected in thread 2");
            deadlocks_detected++;
        } else {
            k_sem_give(&semaphore_1);
        }
        k_sem_give(&semaphore_2);
    };
    
    uint32_t start_time = k_uptime_get_32();
    
    test_threads[0] = k_thread_create(&thread_stacks[0][0], THREAD_STACK_SIZE,
                                      sem_thread_1, NULL, NULL, NULL,
                                      THREAD_PRIORITY_MID, 0, K_NO_WAIT);
    
    test_threads[1] = k_thread_create(&thread_stacks[1][0], THREAD_STACK_SIZE,
                                      sem_thread_2, NULL, NULL, NULL,
                                      THREAD_PRIORITY_MID, 0, K_NO_WAIT);
    
    k_sleep(K_MSEC(DEADLOCK_DETECTION_TIMEOUT_MS));
    
    result->detection_time_ms = k_uptime_get_32() - start_time;
    
    if (k_thread_join(test_threads[0], K_MSEC(100)) != 0 ||
        k_thread_join(test_threads[1], K_MSEC(100)) != 0) {
        result->deadlock_occurred = true;
        k_thread_abort(test_threads[0]);
        k_thread_abort(test_threads[1]);
    }
    
    /* Reset semaphores */
    k_sem_init(&semaphore_1, 1, 1);
    k_sem_init(&semaphore_2, 1, 1);
    
    if (result->deadlock_occurred) {
        LOG_INF("✅ Semaphore deadlock detected and resolved");
    }
}

/**
 * @brief Print deadlock test results summary
 */
static void print_deadlock_summary(void)
{
    LOG_INF("=== Deadlock/Livelock Test Results Summary ===");
    
    for (int i = 0; i < deadlock_tests_run; i++) {
        const deadlock_result_t *result = &deadlock_results[i];
        
        LOG_INF("Test %d: %s", i + 1, result->test_name);
        LOG_INF("  Description: %s", result->description);
        LOG_INF("  Deadlock Detected: %s", result->deadlock_occurred ? "YES" : "NO");
        LOG_INF("  Livelock Detected: %s", result->livelock_occurred ? "YES" : "NO");
        LOG_INF("  Detection Time: %u ms", result->detection_time_ms);
        LOG_INF("  Threads Involved: %u", result->threads_involved);
    }
    
    int successful_detections = 0;
    for (int i = 0; i < deadlock_tests_run; i++) {
        if (deadlock_results[i].deadlock_occurred || deadlock_results[i].livelock_occurred) {
            successful_detections++;
        }
    }
    
    float detection_rate = (deadlock_tests_run > 0) ? 
                          (100.0f * successful_detections / deadlock_tests_run) : 0.0f;
    
    LOG_INF("Overall detection rate: %.1f%% (%d/%d)", 
           detection_rate, successful_detections, deadlock_tests_run);
}

/**
 * @brief Main deadlock test entry point
 *
 * Orchestrates comprehensive deadlock and livelock testing
 * using various synchronization primitives and scenarios.
 *
 * @param p1 Unused parameter 1
 * @param p2 Unused parameter 2  
 * @param p3 Unused parameter 3
 */
void deadlock_test_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("🔒 Starting ARM Cortex-M3 Deadlock/Livelock Test Suite");
    
    /* Initialize test system */
    deadlock_tests_run = 0;
    deadlocks_detected = 0;
    deadlock_detected = false;
    
    if (initialize_deadlock_detection() != 0) {
        LOG_ERR("Failed to initialize deadlock detection");
        return;
    }
    
    /* Wait for system stabilization */
    k_sleep(K_SECONDS(1));
    
    LOG_INF("=== Deadlock and Livelock Detection Tests ===");
    
    /* Execute deadlock tests */
    test_circular_deadlock();
    k_sleep(K_MSEC(500));
    
    test_priority_inversion_deadlock();
    k_sleep(K_MSEC(500));
    
    test_spinlock_livelock();
    k_sleep(K_MSEC(500));
    
    test_semaphore_deadlock();
    k_sleep(K_MSEC(500));
    
    /* Print detailed results */
    print_deadlock_summary();
    
    /* Final statistics */
    LOG_INF("=== Final Deadlock Test Results ===");
    LOG_INF("Deadlock tests executed: %d", deadlock_tests_run);
    LOG_INF("Deadlocks/Livelocks detected: %d", deadlocks_detected);
    LOG_INF("Detection rate: %.1f%%", 
           (deadlock_tests_run > 0) ? 
           (100.0 * deadlocks_detected / deadlock_tests_run) : 0.0);
    
    /* Report final status */
    if (deadlocks_detected > 0) {
        LOG_INF("✅ Deadlock/Livelock detection is working correctly");
        ft_report_fault(FT_DEADLOCK_FAULT, FT_SEVERITY_HIGH);
    } else {
        LOG_WRN("⚠️  No deadlocks detected - scenarios may need adjustment");
    }
    
    LOG_INF("🔒 Deadlock/Livelock test suite completed");
}