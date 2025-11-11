#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance.h>

LOG_MODULE_REGISTER(deadlock_test, LOG_LEVEL_INF);

/* Shared resources for deadlock scenario */
static struct k_mutex mutex_a;
static struct k_mutex mutex_b;
static bool deadlock_detected = false;

/* Thread stacks for deadlock scenario */
#define DEADLOCK_THREAD_STACK_SIZE 1024
K_THREAD_STACK_DEFINE(deadlock_thread1_stack, DEADLOCK_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(deadlock_thread2_stack, DEADLOCK_THREAD_STACK_SIZE);

static struct k_thread deadlock_thread1;
static struct k_thread deadlock_thread2;

/* Deadlock detection timer */
static struct k_timer deadlock_timer;

static void deadlock_timer_handler(struct k_timer *timer) {
    ARG_UNUSED(timer);
    
    if (!deadlock_detected) {
        LOG_ERR("🔒 DEADLOCK DETECTED! Threads blocked for too long");
        deadlock_detected = true;
        
        /* Report deadlock fault */
        ft_report_fault(FT_DEADLOCK_FAULT, FT_SEVERITY_HIGH);
        
        /* Recovery: Abort both threads to break deadlock */
        LOG_INF("🔧 Recovery: Terminating deadlocked threads");
        k_thread_abort(&deadlock_thread1);
        k_thread_abort(&deadlock_thread2);
        
        LOG_INF("✅ Deadlock recovery completed");
    }
}

static void deadlock_thread1_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("Deadlock Thread 1: Starting");
    
    /* Acquire mutex A first */
    LOG_INF("Deadlock Thread 1: Acquiring mutex A");
    k_mutex_lock(&mutex_a, K_FOREVER);
    LOG_INF("Deadlock Thread 1: Got mutex A");
    
    /* Small delay to increase chance of deadlock */
    k_sleep(K_MSEC(100));
    
    /* Try to acquire mutex B (will block if thread 2 has it) */
    LOG_INF("Deadlock Thread 1: Trying to acquire mutex B");
    k_mutex_lock(&mutex_b, K_FOREVER);
    LOG_INF("Deadlock Thread 1: Got mutex B - NO DEADLOCK");
    
    /* This should not be reached in our deadlock scenario */
    k_mutex_unlock(&mutex_b);
    k_mutex_unlock(&mutex_a);
    LOG_INF("Deadlock Thread 1: Released all mutexes");
}

static void deadlock_thread2_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("Deadlock Thread 2: Starting");
    
    /* Acquire mutex B first */
    LOG_INF("Deadlock Thread 2: Acquiring mutex B");
    k_mutex_lock(&mutex_b, K_FOREVER);
    LOG_INF("Deadlock Thread 2: Got mutex B");
    
    /* Small delay to increase chance of deadlock */
    k_sleep(K_MSEC(100));
    
    /* Try to acquire mutex A (will block if thread 1 has it) */
    LOG_INF("Deadlock Thread 2: Trying to acquire mutex A");
    k_mutex_lock(&mutex_a, K_FOREVER);
    LOG_INF("Deadlock Thread 2: Got mutex A - NO DEADLOCK");
    
    /* This should not be reached in our deadlock scenario */
    k_mutex_unlock(&mutex_a);
    k_mutex_unlock(&mutex_b);
    LOG_INF("Deadlock Thread 2: Released all mutexes");
}

void deadlock_test_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("🔒 Starting deadlock detection test");
    
    /* Initialize mutexes */
    k_mutex_init(&mutex_a);
    k_mutex_init(&mutex_b);
    
    /* Initialize deadlock detection timer */
    k_timer_init(&deadlock_timer, deadlock_timer_handler, NULL);
    
    LOG_INF("📝 Scenario: Classic AB-BA deadlock pattern");
    LOG_INF("   Thread 1: Lock A → Lock B");
    LOG_INF("   Thread 2: Lock B → Lock A");
    
    /* Start deadlock detection timer */
    k_timer_start(&deadlock_timer, K_MSEC(CONFIG_FT_DEADLOCK_TIMEOUT_MS), K_NO_WAIT);
    LOG_INF("⏰ Deadlock timer started (%d ms timeout)", CONFIG_FT_DEADLOCK_TIMEOUT_MS);
    
    /* Create the two threads that will deadlock */
    k_thread_create(&deadlock_thread1, deadlock_thread1_stack, 
                   DEADLOCK_THREAD_STACK_SIZE,
                   deadlock_thread1_entry, NULL, NULL, NULL,
                   K_PRIO_COOP(8), 0, K_NO_WAIT);
    k_thread_name_set(&deadlock_thread1, "deadlock_t1");
    
    k_thread_create(&deadlock_thread2, deadlock_thread2_stack, 
                   DEADLOCK_THREAD_STACK_SIZE,
                   deadlock_thread2_entry, NULL, NULL, NULL,
                   K_PRIO_COOP(8), 0, K_NO_WAIT);
    k_thread_name_set(&deadlock_thread2, "deadlock_t2");
    
    /* Wait for deadlock detection or completion */
    int timeout_cycles = 0;
    while (!deadlock_detected && timeout_cycles < 20) {
        k_sleep(K_MSEC(500));
        timeout_cycles++;
        
        /* Check if threads completed normally (unlikely) */
        if (k_thread_join(&deadlock_thread1, K_NO_WAIT) == 0 &&
            k_thread_join(&deadlock_thread2, K_NO_WAIT) == 0) {
            LOG_INF("🤔 Threads completed without deadlock - unexpected!");
            break;
        }
    }
    
    /* Stop the timer */
    k_timer_stop(&deadlock_timer);
    
    if (deadlock_detected) {
        LOG_INF("✅ Deadlock test completed - fault detected and recovered");
    } else {
        LOG_WRN("⚠️  Deadlock test timed out without detection");
    }
    
    LOG_INF("🔒 Deadlock test finished");
}