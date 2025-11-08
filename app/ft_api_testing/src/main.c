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
#include <zephyr/sys/printk.h>

#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(ft_api_testing, LOG_LEVEL_INF);

// test configurations
// TODO : TEST_DURATION
#define TEST_THREAD_COUNT 4
#define TEST_STACK_SIZE 2048
#define SMALL_STACK_SIZE 512
// TODO : ITERATION_DELAY

// test result tracking
struct test_results {
    uint32_t tests_run;
    uint32_t tests_passed;
    uint32_t tests_failed; 
    uint32_t faults_injected;
    uint32_t recoveries_successful;
    uint32_t recoveries_failed;
};

// static struct test_results results = {0};
static struct k_mutex results_lock;
static volatile bool tests_running = true;

// test thread stacks
K_THREAD_STACK_ARRAY_DEFINE(test_stacks, TEST_THREAD_COUNT, TEST_STACK_SIZE);
K_THREAD_STACK_DEFINE(small_stack, SMALL_STACK_SIZE);

// static struct k_thread test_threads[TEST_THREAD_COUNT];
// static struct k_thread overflow_thread;

void do_nothing(void)
{
    return;
}

int main(void)
{
    int ret; 

    printk("Enhanced Fault Tolerance API Testing Application Starting...\n");
    printk("ECE753 Project - Safety-Critical Embedded Systems\n");

    // Initialize mutex for results tracking
    k_mutex_init(&results_lock);

    // Initialize the Fault Tolerance framework
    ret = ft_init();
    // LOG_INF("ret = %d", ret);
    // if (ret != 0) {
    //     LOG_ERR("Failed to initialize Fault Tolerance framework: %d", ret);
    //     return ret;
    // }

    
    return 0;
}