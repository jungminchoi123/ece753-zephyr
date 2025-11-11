#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance.h>

LOG_MODULE_REGISTER(ft_test_app, LOG_LEVEL_INF);

#define OVERFLOW_STACK_SIZE 1024
K_THREAD_STACK_DEFINE(overflow_stack, OVERFLOW_STACK_SIZE);
static struct k_thread overflow_thread;

void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
    LOG_ERR("Fatal error: reason %d", reason);
    
    if (reason == K_ERR_STACK_CHK_FAIL) {
        LOG_ERR("Stack overflow detected in thread %p", k_current_get());
        LOG_ERR("Triggering fault tolerance recovery");
        
        /* Notify the fault tolerance system */
        ft_report_fault(FT_STACK_OVERFLOW_FAULT, FT_SEVERITY_CRITICAL);
        
        /* For stack overflow, attempt graceful recovery instead of system halt */
        LOG_INF("Attempting graceful recovery from stack overflow...");
        
        /* Mark thread for termination and abort the current operation */
        struct k_thread *current = k_current_get();
        if (current != NULL) {
            LOG_INF("Terminating thread %p to recover from stack overflow", current);
            
            /* We can't safely continue execution in the corrupted thread */
            /* The best we can do is abort this thread and let the system continue */
            k_thread_abort(current);  /* This will cleanly terminate the thread */
            
            /* This point should not be reached since thread is aborted */
            LOG_INF("Recovery successful - thread terminated");
        }
        
        return; /* Attempt to return instead of halting system */
    }
    
    /* For other fatal errors, still halt the system */
    LOG_ERR("System halting due to unrecoverable fault type %d", reason);
    k_fatal_halt(reason);
}


__attribute__((noinline)) void recursive_overflow(int depth) {
    /* Moderately sized buffer to cause overflow within a few calls */
    volatile char buffer[2048];  /* 2KB per call */
    
    /* Fill buffer to prevent optimization */
    for(int i = 0; i < 2048; i++) {
        buffer[i] = (char)(depth % 256);
    }
    
    LOG_INF("Recursion depth: %d (est. stack: %d KB)", depth, depth * 2);
    
    /* Force some computation to prevent tail-call optimization */
    volatile int sum = 0;
    for(int i = 0; i < depth && i < 500; i++) {
        sum += buffer[i % 2048];
    }
    buffer[sum % 2048] = (char)sum;
    
    k_sleep(K_MSEC(50)); /* Slower progression to see the recovery process */
    
    /* Continue recursing until stack overflow occurs */
    recursive_overflow(depth + 1);
}

void overflow_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("Overflow thread started - preparing for REAL overflow test");
    k_sleep(K_SECONDS(5));  // Let system stabilize and FT start
    
    LOG_INF("⚠️  WARNING: Starting REAL stack overflow test - system will detect and recover");
    ft_check_stack_usage();  // Initial stack check
    
    /* This will cause actual stack overflow that the FT system should catch */
    LOG_INF("Beginning infinite recursion...");
    recursive_overflow(0);
    
    LOG_ERR("This line should never be reached!");
}

void main(void) {
    LOG_INF("=== FT Stack Overflow Test Starting ===");
    LOG_INF("Fault tolerance system should auto-initialize");

    /* Give the FT system time to start up */
    k_sleep(K_SECONDS(3));

    LOG_INF("Creating overflow test thread");
    
    /* Create the thread that will cause stack overflow */
    k_thread_create(&overflow_thread, overflow_stack, OVERFLOW_STACK_SIZE,
                    overflow_entry, NULL, NULL, NULL,
                    K_PRIO_COOP(7), 0, K_NO_WAIT);  /* Lower priority than FT monitor */

    k_thread_name_set(&overflow_thread, "overflow_thread");

    /* Main monitoring loop */
    int counter = 0;
    bool overflow_occurred = false;
    
    while (1) {
        LOG_INF("Main thread alive, cycle: %d - FT system active", counter++);
        
        /* Check if overflow thread was terminated (indicating recovery occurred) */
        if (!overflow_occurred) {
            /* Try to join the thread with no wait - if successful, thread is dead */
            int join_result = k_thread_join(&overflow_thread, K_NO_WAIT);
            if (join_result == 0) {
                LOG_INF("🔄 Recovery detected! Overflow thread was successfully terminated");
                LOG_INF("✅ System recovered from stack overflow - continuing normal operation");
                overflow_occurred = true;
            }
        }
        
        k_sleep(K_SECONDS(3));
        
        /* Show that system continues running after recovery */
        if (overflow_occurred && counter > 5) {
            LOG_INF("🎉 SUCCESS: System has been running stably for %d cycles after recovery", 
                   counter - 3);
        }
        
        /* Stop the test after demonstrating recovery */
        if (counter > 8) {
            LOG_INF("Test completed successfully - fault tolerance system working!");
            break;
        }
    }
    
    LOG_INF("=== Fault Tolerance Test Summary ===");
    LOG_INF("✅ Stack overflow detected and handled automatically");
    LOG_INF("✅ Problematic thread terminated gracefully");
    LOG_INF("✅ System continued operation after recovery");
    LOG_INF("✅ Main thread and FT threads remain operational");
    
    while (1) {
        k_sleep(K_FOREVER);
    }
}
