/**
 * @file main.c
 * @author Jack Ostapeic, MS ECE student at UW-Madison
 * @brief Modular Fault Tolerance API Testing Application
 *
 * This application validates stack overflow recovery using a dedicated recovery thread.
 * It logs stack usage, detects faults, and recreates faulted threads with larger stacks.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>

#define STACK_SIZE 2048
#define RECOVERY_STACK_SIZE 8192
#define THREAD_PRIORITY K_PRIO_COOP(5)

LOG_MODULE_REGISTER(ft_stack_overflow, LOG_LEVEL_INF);

// Recovery coordination
K_SEM_DEFINE(recovery_sem, 0, 1);
bool stack_recovered = false;
k_tid_t faulted_tid = NULL;

// Start fault-inducing thread
struct k_thread initial_thread;
K_THREAD_STACK_DEFINE(initial_stack, STACK_SIZE);

// Recovery thread and stack
struct k_thread recovery_thread;
K_THREAD_STACK_DEFINE(recovery_thread_stack, RECOVERY_STACK_SIZE);

// Thread recreation pool
static struct k_thread recreated_thread;
static K_THREAD_STACK_DEFINE(recreated_stack, STACK_SIZE);

// Recursive bomb to trigger stack overflow
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

// Fault-inducing thread
void test_thread_func(void *p1, void *p2, void *p3)
{
    printk("Test thread started - triggering stack overflow...\n");
    LOG_INF("Starting stack overflow test thread");
    k_sleep(K_MSEC(100));
    recursive_bomb(0);
}

// Recovery thread waits for faults and recreates test thread
void recovery_thread_func(void *p1, void *p2, void *p3)
{
    while (1) {
        k_sem_take(&recovery_sem, K_FOREVER);

        if (faulted_tid != NULL && faulted_tid != k_current_get()) {
            printk("Aborting faulted thread: %p\n", faulted_tid);
            // k_thread_abort(faulted_tid);
            faulted_tid = NULL;
        }

        if (!stack_recovered) {
            k_tid_t new_tid = k_thread_create(
                &recreated_thread,
                recreated_stack,
                STACK_SIZE,
                test_thread_func,
                NULL, NULL, NULL,
                THREAD_PRIORITY, 0, K_NO_WAIT);

            k_thread_name_set(new_tid, "recovered_test_thread");
            printk("Recovery thread recreated test thread: %p\n", new_tid);
            stack_recovered = true;
        } else {
            printk("Recovery already performed, skipping.\n");
        }
    }
}

// Custom fatal error handler
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
    printk("\n*** FATAL ERROR HANDLER CALLED ***\n");
    printk("Reason: %u\n", reason);

    if (reason == K_ERR_STACK_CHK_FAIL) {
        printk("Stack overflow detected!\n");
        faulted_tid = k_current_get();
        k_sem_give(&recovery_sem);
        k_fatal_halt(reason);  // <- this halts the faulted thread cleanly

    } else {
        printk("Unhandled fatal error, halting system.\n");
        k_fatal_halt(reason);
    }
}

int main(void)
{
    LOG_INF("Fault Tolerance Stack Overflow Test Application Starting...");

    // Start recovery thread
    k_thread_create(&recovery_thread, recovery_thread_stack, RECOVERY_STACK_SIZE,
                    recovery_thread_func, NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&recovery_thread, "ft_recovery_thread");

    k_tid_t initial_tid = k_thread_create(&initial_thread, initial_stack, STACK_SIZE,
                                          test_thread_func, NULL, NULL, NULL,
                                          THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(initial_tid, "ft_test_thread");

    while (1) {
        k_sleep(K_SECONDS(10));
    }

    LOG_INF("Main thread exiting (should not reach here)");

    return 0;
}
