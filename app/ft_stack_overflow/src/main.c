#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ft_test_app, LOG_LEVEL_INF);

#define OVERFLOW_STACK_SIZE 1024
K_THREAD_STACK_DEFINE(overflow_stack, OVERFLOW_STACK_SIZE);
static struct k_thread overflow_thread;

__attribute__((noinline)) void recursive_overflow(int depth) {
    volatile char buffer[256];  // volatile prevents optimization
    buffer[0] = depth;
    printk("Recursion depth: %d\n", depth);
    recursive_overflow(depth + 1);
}

void overflow_entry(void *p1, void *p2, void *p3) {
    LOG_INF("Overflow thread started");
    recursive_overflow(0);
}

void main(void) {
    LOG_INF("Starting FT stack overflow test...");

    k_thread_create(&overflow_thread, overflow_stack, OVERFLOW_STACK_SIZE,
                    overflow_entry, NULL, NULL, NULL,
                    K_PRIO_COOP(3), 0, K_NO_WAIT);

    k_thread_name_set(&overflow_thread, "overflow_thread");

    while (1) {
        k_sleep(K_SECONDS(1));
    }
}
