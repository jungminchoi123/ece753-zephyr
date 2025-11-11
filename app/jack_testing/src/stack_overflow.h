#ifndef STACK_OVERFLOW_H
#define STACK_OVERFLOW_H

#include <zephyr/kernel.h>

#define THREAD_STACK_SIZE 1024

/* External declarations for stack overflow test */
extern k_thread_stack_t stack_overflow_test_thread_stack[K_THREAD_STACK_LEN(THREAD_STACK_SIZE)];
extern struct k_thread stack_overflow_test_thread;
extern void stack_overflow_thread_entry(void *p1, void *p2, void *p3);

#endif /* STACK_OVERFLOW_H */