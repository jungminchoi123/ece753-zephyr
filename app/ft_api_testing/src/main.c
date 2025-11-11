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
#include <zephyr/logging/log.h>
// #include <logging/log.h>

#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(ft_api_testing, LOG_LEVEL_INF);

#define THREAD_STACK_SIZE 1024

K_THREAD_STACK_DEFINE(test_thread_stack, THREAD_STACK_SIZE);
static struct k_thread test_thread;

void thread_entry(void *p1, void *p2, void *p3)
{
    LOG_INF("Test thread started - triggering stack overflow...\n");
    recursive_bomb(0);
}

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
    k_yield();
    recursive_bomb(depth + 1);
}

int main(void)
{
    int ret;

    printk("Enhanced Fault Tolerance API Testing Application Starting...\n");

    ret = ft_init();
    printk("Fault Tolerance subsystem initialization returned: %d\n", ret);

    ret = ft_init();
    printk("Fault Tolerance subsystem re-initialization returned: %d\n", ret);

    k_thread_create(&test_thread, test_thread_stack, THREAD_STACK_SIZE,
                    (k_thread_entry_t)thread_entry, (void *)0, NULL, NULL,
                    K_PRIO_COOP(5), 0, K_NO_WAIT);

    k_thread_name_set(&test_thread, "ft_test_thread");

    while (1) {
        k_sleep(K_MSEC(1000));
    }

    return 0;
}