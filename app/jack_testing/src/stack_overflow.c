#include <zephyr/fault_tolerance.h>
#include <zephyr/logging/log.h>
#include "stack_overflow.h"

LOG_MODULE_REGISTER(stack_overflow_test, LOG_LEVEL_INF);

K_THREAD_STACK_DEFINE(stack_overflow_test_thread_stack, THREAD_STACK_SIZE);
struct k_thread stack_overflow_test_thread;

void recursive_bomb(int depth)
{
    volatile char buffer[200];
    for (int i = 0; i < 200; i++) {
        buffer[i] = (depth + i) & 0xFF;
    }

    size_t unused; 
    if (k_thread_stack_space_get(k_current_get(), &unused) == 0) {
        printk("Unused stack: %zu bytes\n", unused);
    }

    printk("Recursion depth: %d\n", depth);
    
    /* Add a limit to prevent infinite recursion for testing */
    if (depth > 10) {
        printk("Reached maximum test depth, stopping recursion\n");
        return;
    }
    
    k_yield();
    recursive_bomb(depth + 1);
}

void stack_overflow_thread_entry(void *p1, void *p2, void *p3)
{
    LOG_INF("Test thread started - triggering controlled stack overflow test...");
    printk("Stack overflow test thread starting...\n");
    recursive_bomb(0);
    printk("Stack overflow test completed successfully\n");
}