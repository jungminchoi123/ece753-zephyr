/**
 * @file main.c
 * @author Jack Ostapeic, MS ECE student at UW-Madison
 * @brief Fault Tolerance API Testing Application
 *
 * This application validates the functionality of the Fault Tolerance (FT) API
 * in the Zephyr RTOS. It includes tests for fault reporting, handler registration,
 * recovery actions, and logging mechanisms.
 */

#include <zephyr/fault_tolerance.h>
#include <zephyr/kernel.h>
#include <string.h>
#include <stdio.h>
#include "stack_overflow.h"

LOG_MODULE_REGISTER(jack_testing_main, LOG_LEVEL_INF);

int main(void)
{
    int ret;

    printk("Enhanced Fault Tolerance API Testing Application Starting...\n");

    ret = ft_init();
    printk("Fault Tolerance subsystem initialization returned: %d\n", ret);

    ret = ft_init();
    printk("Fault Tolerance subsystem re-initialization returned: %d\n", ret);

    printk("Creating stack overflow test thread...\n");
    k_thread_create(&stack_overflow_test_thread, stack_overflow_test_thread_stack,
                    THREAD_STACK_SIZE,
                    (k_thread_entry_t)stack_overflow_thread_entry,
                    (void *)0, NULL, NULL,
                    K_PRIO_COOP(10), 0, K_NO_WAIT);
    k_thread_name_set(&stack_overflow_test_thread, "ft_stack_overflow_test_thread");
    
    printk("Thread created, waiting for completion...\n");
    
    /* Wait for the thread to complete or keep the system alive */
    k_thread_join(&stack_overflow_test_thread, K_FOREVER);
    
    printk("Test thread completed. Application finished.\n");
    
    return 0;
}


