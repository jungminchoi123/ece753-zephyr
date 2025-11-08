/**
    * @file main.c
    * @author Jack Ostapeic, MS ECE student at UW-Madison
    * @brief Fault Tolerance API Testing Application
    *
    * This application validates the functionality of the Fault Tolerance (FT) API
    * in the Zephyr RTOS. It includes tests for fault reporting, handler registration,
    * recovery actions, and logging mechanisms.
 */

#include <zephyr/kernel.h>
#include <zephyr/fault_tolerance.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include <string.h>
#include <stdio.h>

#define STACK_SIZE 2048  // Smaller stack to trigger overflow faster
#define RECOVERY_STACK_SIZE 8192
#define THREAD_PRIORITY K_PRIO_COOP(5)

LOG_MODULE_REGISTER(ft_stack_overflow, LOG_LEVEL_INF);

struct k_thread test_thread;
struct k_thread recovery_thread;
K_THREAD_STACK_DEFINE(test_thread_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(recovery_thread_stack, RECOVERY_STACK_SIZE);

bool stack_recovered = false;

// Simple recursive function that will definitely overflow
void recursive_bomb(int depth)
{

    volatile char buffer[200];  // Large local buffer to consume stack quickly
    
    // Fill buffer to prevent optimization
    for (int i = 0; i < 200; i++) {
        buffer[i] = (depth + i) & 0xFF;
    }

    size_t unused;
    if (k_thread_stack_space_get(k_current_get(), &unused) == 0) {
        printk("Unused stack: %zu bytes\n", unused);
    }

    
    printk("Recursion depth: %d\n", depth);
    k_yield();
    
    // Just keep recursing until we overflow
    recursive_bomb(depth + 1);
}

void test_thread_func(void *p1, void *p2, void *p3)
{
    printk("Test thread started - triggering stack overflow...\n");
    LOG_INF("Starting stack overflow test thread");
    
    // Give a moment for logging
    k_sleep(K_MSEC(100));
    
    // This will cause a real stack overflow that should trigger k_sys_fatal_error_handler
    recursive_bomb(0);
}

// Custom fatal error handler that uses the FT API
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
    printk("\n*** FATAL ERROR HANDLER CALLED ***\n");
    printk("K_ERR_STACK_CHK_FAIL = %u\n", K_ERR_STACK_CHK_FAIL);
    printk("Reason: %u\n", reason);
    
    // Check if this is a stack overflow
    if (reason == K_ERR_STACK_CHK_FAIL) {
        printk("Stack overflow detected!\n");

        k_tid_t faulted_tid = k_current_get();
        k_thread_abort(faulted_tid);

        if (!stack_recovered) {
            k_tid_t new_tid = k_thread_create(
                &recovery_thread,
                recovery_thread_stack,
                RECOVERY_STACK_SIZE,
                test_thread_func,
                NULL, NULL, NULL,
                THREAD_PRIORITY, 0, K_NO_WAIT);

            k_thread_name_set(new_tid, "recovered_test_thread");
            printk("Recovery thread created: %p\n", new_tid);

            stack_recovered = true;
            sys_reboot(SYS_REBOOT_COLD);

        } else {
            printk("Stack overflow already handled once, not recovering again.\n");
            k_fatal_halt(reason);
        }
    } else {
        printk("Non-stack overflow fatal error, halting system.\n");
        k_fatal_halt(reason);
    }

    
    printk("Fatal error handler complete\n");
}

int main(void)
{
    LOG_INF("Fault Tolerance Stack Overflow Test Application Starting...");

    // Create a thread with a small stack to trigger overflow
    k_thread_create(&test_thread, test_thread_stack, STACK_SIZE,
                    test_thread_func, NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    // Let the test run
    k_sleep(K_SECONDS(10));

    LOG_INF("Test completed (this shouldn't be reached)");
    return 0;
}