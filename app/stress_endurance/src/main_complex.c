/*
 * Copyright (c) 2025 ECE753 Project Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Long-running stress test for fault tolerance evaluation
 *
 * This application performs sustained stress testing of the fault tolerance
 * framework with resource-intensive operations, memory fragmentation,
 * thread exhaustion, and endurance testing scenarios.
 */

#include <zephyr/kernel.h>
#include <zephyr/fault_tolerance.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/random/random.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(ft_stress_test, LOG_LEVEL_INF);

/* Test configuration */
#define STRESS_TEST_DURATION_HOURS  24      /* 24 hour stress test */
#define STRESS_TEST_DURATION_MS     (STRESS_TEST_DURATION_HOURS * 60 * 60 * 1000)
#define MAX_STRESS_THREADS          16
#define STRESS_THREAD_STACK_SIZE    4096
#define MEMORY_STRESS_ITERATIONS    1000
#define FRAGMENT_ALLOCATION_SIZE    64
#define MAX_ALLOCATIONS             512
#define FAULT_INJECTION_INTERVAL_MS 5000    /* Inject faults every 5 seconds */
#define STATISTICS_REPORT_INTERVAL  60000   /* Report stats every minute */
#define PHASE_TRANSITION_DELAY      10000   /* 10 seconds between phases */

/* Test phases */
enum stress_test_phase {
    PHASE_INITIALIZATION = 0,
    PHASE_MEMORY_STRESS,
    PHASE_THREAD_STRESS,
    PHASE_IO_STRESS,
    PHASE_TIMING_STRESS,
    PHASE_COMBINED_STRESS,
    PHASE_RECOVERY_VALIDATION,
    PHASE_ENDURANCE_TEST,
    PHASE_COMPLETE
};

/* Stress test state */
struct stress_test_state {
    enum stress_test_phase current_phase;
    int64_t test_start_time;
    int64_t phase_start_time;
    uint32_t phase_duration_ms;
    bool test_running;
    bool fault_injection_enabled;
    
    /* Extended statistics */
    uint32_t memory_allocations;
    uint32_t memory_deallocations;
    uint32_t memory_failures;
    uint32_t memory_fragmentation_events;
    uint32_t threads_created;
    uint32_t threads_destroyed;
    uint32_t thread_failures;
    uint32_t io_operations;
    uint32_t io_failures;
    uint32_t timing_violations;
    uint32_t fault_injections;
    uint32_t recoveries_attempted;
    uint32_t recoveries_successful;
    uint32_t recoveries_failed;
    uint32_t system_resets;
    uint32_t watchdog_timeouts;
    
    /* Performance metrics */
    uint32_t peak_memory_usage;
    uint32_t peak_thread_count;
    uint32_t total_uptime_seconds;
    uint32_t fault_free_periods;
    uint32_t max_fault_free_period_seconds;
    
    /* Resource tracking */
    void *allocations[MAX_ALLOCATIONS];
    size_t allocation_sizes[MAX_ALLOCATIONS];
    uint32_t allocation_count;
    
    /* Synchronization */
    struct k_mutex state_mutex;
    struct k_sem phase_sem;
    struct k_work_q stress_workq;
    struct k_work fault_injection_work;
    struct k_work statistics_work;
};

static struct stress_test_state stress_state = {0};

/* Thread management */
K_THREAD_STACK_ARRAY_DEFINE(stress_thread_stacks, MAX_STRESS_THREADS, STRESS_THREAD_STACK_SIZE);
static struct k_thread stress_threads[MAX_STRESS_THREADS];
static uint32_t active_thread_count = 0;

/* Phase configurations - Enhanced for comprehensive testing */
static const struct {
    enum stress_test_phase phase;
    uint32_t duration_ms;
    const char *name;
    const char *description;
    bool enable_fault_injection;
    uint32_t fault_injection_rate_ms;
} phase_configs[] = {
    {PHASE_INITIALIZATION, 60000, "Initialization", "System baseline and calibration", false, 0},
    {PHASE_MEMORY_STRESS, 1800000, "Memory Stress", "Memory allocation/fragmentation stress", true, 10000},
    {PHASE_THREAD_STRESS, 1800000, "Thread Stress", "Thread lifecycle and concurrency stress", true, 8000},
    {PHASE_IO_STRESS, 1800000, "I/O Stress", "I/O operations and peripheral stress", true, 12000},
    {PHASE_TIMING_STRESS, 1800000, "Timing Stress", "Real-time constraints and deadlines", true, 6000},
    {PHASE_COMBINED_STRESS, 7200000, "Combined Stress", "Multi-dimensional stress testing", true, 5000},
    {PHASE_RECOVERY_VALIDATION, 3600000, "Recovery Validation", "Fault recovery and resilience", true, 3000},
    {PHASE_ENDURANCE_TEST, 0, "Endurance Test", "Long-term stability validation", true, 15000} /* Remaining time */
};

/* Forward declarations */
static void memory_stress_thread(void *p1, void *p2, void *p3);
static void thread_stress_controller(void *p1, void *p2, void *p3);
static void io_stress_thread(void *p1, void *p2, void *p3);
static void timing_stress_thread(void *p1, void *p2, void *p3);
static void fault_injection_thread(void *p1, void *p2, void *p3);
static void system_monitor_thread(void *p1, void *p2, void *p3);
static void phase_controller_thread(void *p1, void *p2, void *p3);
static void statistics_reporter_thread(void *p1, void *p2, void *p3);
static void fault_injection_work_handler(struct k_work *work);
static void statistics_work_handler(struct k_work *work);
static void inject_random_fault(void);
static void advance_to_next_phase(void);
static void print_comprehensive_statistics(void);
static void validate_system_health(void);

/**
 * @brief Enhanced stress test fault handler
 */
static enum ft_recovery_action stress_fault_handler(const struct ft_fault_context *ctx)
{
    LOG_WRN("Stress fault detected: type=%d, severity=%d, phase=%s", 
            ctx->fault_type, ctx->severity, 
            phase_configs[stress_state.current_phase].name);
    
    k_mutex_lock(&stress_state.state_mutex, K_FOREVER);
    stress_state.fault_injections++;
    
    /* Track fault patterns and recovery attempts */
    enum ft_recovery_action action = FT_RECOVERY_NONE;
    
    /* Adaptive recovery strategy based on context and current phase */
    switch (ctx->fault_type) {
    case FT_FAULT_MEMORY_LEAK:
        action = (ctx->severity >= FT_SEVERITY_HIGH) ? FT_RECOVERY_CUSTOM : FT_RECOVERY_NONE;
        break;
    case FT_FAULT_STACK_OVERFLOW:
        action = FT_RECOVERY_RESTART_THREAD;
        stress_state.thread_failures++;
        break;
    case FT_FAULT_RESOURCE_EXHAUSTION:
        action = FT_RECOVERY_SAFE_MODE;
        break;
    case FT_FAULT_RACE_CONDITION:
        action = (stress_state.current_phase == PHASE_THREAD_STRESS) ? 
                 FT_RECOVERY_CUSTOM : FT_RECOVERY_NONE;
        break;
    case FT_FAULT_TIMING_VIOLATION:
        action = FT_RECOVERY_NONE; /* Log and continue */
        stress_state.timing_violations++;
        break;
    case FT_FAULT_DATA_CORRUPTION:
        action = (ctx->severity == FT_SEVERITY_CRITICAL) ? 
                 FT_RECOVERY_SAFE_MODE : FT_RECOVERY_CUSTOM;
        break;
    default:
        action = (ctx->severity >= FT_SEVERITY_HIGH) ? 
                 FT_RECOVERY_CUSTOM : FT_RECOVERY_NONE;
        break;
    }
    
    if (action != FT_RECOVERY_NONE) {
        stress_state.recoveries_attempted++;
    }
    
    k_mutex_unlock(&stress_state.state_mutex);
    
    return action;
}

/**
 * @brief Enhanced stress test recovery callback
 */
static int stress_recovery_callback(const struct ft_fault_context *ctx, 
                                   enum ft_recovery_action action)
{
    LOG_INF("Executing recovery action %d for fault type %d in phase %s", 
            action, ctx->fault_type, phase_configs[stress_state.current_phase].name);
    
    int recovery_result = 0;
    k_mutex_lock(&stress_state.state_mutex, K_FOREVER);
    
    /* Detailed recovery operations with success tracking */
    switch (action) {
    case FT_RECOVERY_CUSTOM:
        /* Cleanup and resource recovery */
        k_sleep(K_MSEC(100)); /* Simulate cleanup time */
        break;
    case FT_RECOVERY_SAFE_MODE:
        /* Reduce system load */
        LOG_WRN("Entering safe mode - reducing system load");
        break;
    default:
        break;
    }
    
    return 0; /* Success */
}

/**
 * @brief Memory stress testing thread
 */
static void memory_stress_thread(void *p1, void *p2, void *p3)
{
    int thread_id = POINTER_TO_INT(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("Memory stress thread %d started", thread_id);
    
    while (stress_state.test_running && 
           (stress_state.current_phase == PHASE_MEMORY_STRESS ||
            stress_state.current_phase == PHASE_COMBINED_STRESS)) {
        
        /* Perform memory allocation/deallocation cycles */
        for (int i = 0; i < MEMORY_STRESS_ITERATIONS && stress_state.test_running; i++) {
            size_t alloc_size = FRAGMENT_ALLOCATION_SIZE + (sys_rand32_get() % 512);
            void *ptr = k_malloc(alloc_size);
            
            k_mutex_lock(&stress_state.state_mutex, K_FOREVER);
            
            if (ptr) {
                stress_state.memory_allocations++;
                
                /* Track allocation for later deallocation */
                if (stress_state.allocation_count < MAX_ALLOCATIONS) {
                    stress_state.allocations[stress_state.allocation_count] = ptr;
                    stress_state.allocation_sizes[stress_state.allocation_count] = alloc_size;
                    stress_state.allocation_count++;
                    
                    ft_track_allocation(ptr, alloc_size);
                    
                    /* Initialize memory to create realistic usage pattern */
                    memset(ptr, thread_id & 0xFF, alloc_size);
                } else {
                    k_free(ptr);
                }
            } else {
                stress_state.memory_failures++;
                
                /* Report memory exhaustion */
                uintptr_t ctx[4] = {alloc_size, stress_state.allocation_count, 0, 0};
                FT_REPORT_FAULT(FT_FAULT_RESOURCE_EXHAUSTION, FT_SEVERITY_HIGH,
                               "Memory allocation failure", ctx);
            }
            
            k_mutex_unlock(&stress_state.state_mutex);
            
            /* Randomly deallocate some memory to create fragmentation */
            if ((i % 10 == 0) && (stress_state.allocation_count > 0)) {
                k_mutex_lock(&stress_state.state_mutex, K_FOREVER);
                
                uint32_t dealloc_index = sys_rand32_get() % stress_state.allocation_count;
                void *dealloc_ptr = stress_state.allocations[dealloc_index];
                
                if (dealloc_ptr) {
                    k_free(dealloc_ptr);
                    ft_track_deallocation(dealloc_ptr);
                    stress_state.memory_deallocations++;
                    
                    /* Compact allocation array */
                    for (uint32_t j = dealloc_index; j < stress_state.allocation_count - 1; j++) {
                        stress_state.allocations[j] = stress_state.allocations[j + 1];
                        stress_state.allocation_sizes[j] = stress_state.allocation_sizes[j + 1];
                    }
                    stress_state.allocation_count--;
                }
                
                k_mutex_unlock(&stress_state.state_mutex);
            }
            
            /* Monitor memory usage periodically */
            if (i % 100 == 0) {
                ft_monitor_memory_usage();
            }
            
            /* Brief delay to allow other threads to run */
            k_sleep(K_MSEC(1));
        }
        
        /* Longer delay between stress cycles */
        k_sleep(K_MSEC(100));
    }
    
    LOG_INF("Memory stress thread %d completed", thread_id);
}

/**
 * @brief Thread stress controller - manages thread creation/destruction
 */
static void thread_stress_controller(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("Thread stress controller started");
    
    while (stress_state.test_running && 
           (stress_state.current_phase == PHASE_THREAD_STRESS ||
            stress_state.current_phase == PHASE_COMBINED_STRESS)) {
        
        /* Create new threads up to limit */
        k_mutex_lock(&stress_state.state_mutex, K_FOREVER);
        
        if (active_thread_count < MAX_STRESS_THREADS - 4) { /* Leave some threads for other operations */
            uint32_t thread_idx = active_thread_count;
            
            k_thread_create(&stress_threads[thread_idx], 
                           stress_thread_stacks[thread_idx], 
                           STRESS_THREAD_STACK_SIZE,
                           memory_stress_thread, 
                           INT_TO_POINTER(thread_idx), NULL, NULL,
                           K_PRIO_PREEMPT(10), 0, K_NO_WAIT);
            
            char thread_name[16];
            snprintf(thread_name, sizeof(thread_name), "stress_%d", thread_idx);
            k_thread_name_set(&stress_threads[thread_idx], thread_name);
            
            active_thread_count++;
            stress_state.threads_created++;
        }
        
        k_mutex_unlock(&stress_state.state_mutex);
        
        /* Let threads run for a while */
        k_sleep(K_MSEC(5000));
        
        /* Randomly terminate some threads */
        if (active_thread_count > 2 && (sys_rand32_get() % 3 == 0)) {
            k_mutex_lock(&stress_state.state_mutex, K_FOREVER);
            
            uint32_t terminate_idx = sys_rand32_get() % active_thread_count;
            k_thread_abort(&stress_threads[terminate_idx]);
            stress_state.threads_destroyed++;
            
            /* Compact thread array */
            for (uint32_t i = terminate_idx; i < active_thread_count - 1; i++) {
                stress_threads[i] = stress_threads[i + 1];
            }
            active_thread_count--;
            
            k_mutex_unlock(&stress_state.state_mutex);
        }
        
        /* Monitor stack usage */
        ft_monitor_stack_usage();
        
        k_sleep(K_MSEC(2000));
    }
    
    LOG_INF("Thread stress controller completed");
}

/**
 * @brief Timing stress thread - creates timing violations
 */
static void timing_stress_thread(void *p1, void *p2, void *p3)
{
    int thread_id = POINTER_TO_INT(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("Timing stress thread %d started", thread_id);
    
    int64_t deadline_interval = 1000; /* 1 second deadlines */
    
    while (stress_state.test_running && 
           (stress_state.current_phase == PHASE_TIMING_STRESS ||
            stress_state.current_phase == PHASE_COMBINED_STRESS)) {
        
        int64_t deadline = k_uptime_get() + deadline_interval;
        
        /* Simulate work that might miss deadline */
        uint32_t work_duration = 500 + (sys_rand32_get() % 1000); /* 0.5-1.5 seconds */
        
        /* Perform computational work */
        volatile uint32_t compute_result = 0;
        int64_t work_start = k_uptime_get();
        
        while ((k_uptime_get() - work_start) < work_duration) {
            for (int i = 0; i < 10000; i++) {
                compute_result += i * thread_id;
            }
            k_yield(); /* Allow other threads to run */
        }
        
        int64_t completion_time = k_uptime_get();
        
        /* Check if deadline was missed */
        if (completion_time > deadline) {
            k_mutex_lock(&stress_state.state_mutex, K_FOREVER);
            stress_state.timing_violations++;
            k_mutex_unlock(&stress_state.state_mutex);
            
            uintptr_t ctx[4] = {
                (uintptr_t)deadline,
                (uintptr_t)completion_time,
                work_duration,
                thread_id
            };
            
            FT_REPORT_FAULT(FT_FAULT_TIMING_VIOLATION, FT_SEVERITY_MEDIUM,
                           "Deadline missed", ctx);
        }
        
        /* Brief rest before next iteration */
        k_sleep(K_MSEC(100));
    }
    
    LOG_INF("Timing stress thread %d completed", thread_id);
}

/**
 * @brief Fault injection thread - periodically injects various faults
 */
static void fault_injection_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("Fault injection thread started");
    
    while (stress_state.test_running) {
        /* Wait for random interval between fault injections */
        uint32_t delay = 10000 + (sys_rand32_get() % 20000); /* 10-30 seconds */
        k_sleep(K_MSEC(delay));
        
        if (!stress_state.test_running) break;
        
        /* Randomly select fault type to inject */
        enum ft_fault_type fault_types[] = {
            FT_FAULT_RESOURCE_EXHAUSTION,
            FT_FAULT_DATA_CORRUPTION,
            FT_FAULT_COMM_FAILURE,
            FT_FAULT_PERIPHERAL_FAILURE,
            FT_FAULT_CONFIG_ERROR
        };
        
        enum ft_fault_type fault_type = fault_types[sys_rand32_get() % ARRAY_SIZE(fault_types)];
        enum ft_fault_severity severity = (sys_rand32_get() % 4);
        
        uintptr_t ctx[4] = {
            sys_rand32_get(),
            k_uptime_get(),
            (uintptr_t)k_current_get(),
            0
        };
        
        FT_REPORT_FAULT(fault_type, severity, "Injected fault for stress testing", ctx);
        
        k_mutex_lock(&stress_state.state_mutex, K_FOREVER);
        stress_state.fault_injections++;
        k_mutex_unlock(&stress_state.state_mutex);
    }
    
    LOG_INF("Fault injection thread completed");
}

/**
 * @brief Progress monitoring and phase management
 */
static void monitor_progress_and_phases(void)
{
    int64_t current_time = k_uptime_get();
    int64_t elapsed_ms = current_time - stress_state.test_start_time;
    int64_t phase_elapsed = current_time - stress_state.phase_start_time;
    
    /* Check if current phase should complete */
    if (stress_state.current_phase < PHASE_ENDURANCE_TEST) {
        const struct phase_configuration *config = &phase_configs[stress_state.current_phase];
        
        if (phase_elapsed >= config->duration_ms) {
            /* Move to next phase */
            stress_state.current_phase++;
            stress_state.phase_start_time = current_time;
            
            if (stress_state.current_phase < ARRAY_SIZE(phase_configs)) {
                const struct phase_configuration *next_config = &phase_configs[stress_state.current_phase];
                LOG_INF("=== PHASE CHANGE: %s ===", next_config->name);
                LOG_INF("Description: %s", next_config->description);
                
                if (stress_state.current_phase == PHASE_ENDURANCE_TEST) {
                    LOG_INF("Duration: Until test completion");
                } else {
                    LOG_INF("Duration: %d ms", next_config->duration_ms);
                }
            }
        }
    }
    
    /* Print progress every 5 minutes */
    static int64_t last_progress_time = 0;
    if (current_time - last_progress_time >= 300000) { /* 5 minutes */
        last_progress_time = current_time;
        
        uint32_t hours = elapsed_ms / (60 * 60 * 1000);
        uint32_t minutes = (elapsed_ms % (60 * 60 * 1000)) / (60 * 1000);
        
        LOG_INF("=== STRESS TEST PROGRESS ===");
        LOG_INF("Runtime: %dh %dm", hours, minutes);
        LOG_INF("Current Phase: %s", phase_configs[stress_state.current_phase].name);
        LOG_INF("Memory Allocs: %d, Deallocs: %d, Failures: %d", 
                stress_state.memory_allocations, 
                stress_state.memory_deallocations,
                stress_state.memory_failures);
        LOG_INF("Threads Created: %d, Destroyed: %d, Active: %d",
                stress_state.threads_created,
                stress_state.threads_destroyed, 
                active_thread_count);
        LOG_INF("Timing Violations: %d", stress_state.timing_violations);
        LOG_INF("Fault Injections: %d, Recoveries Attempted: %d", 
                stress_state.fault_injections,
                stress_state.recoveries_attempted);
        
        /* Get system statistics */
        struct ft_fault_stats ft_stats;
        struct ft_memory_stats mem_stats;
        
        ft_get_stats(&ft_stats);
        ft_get_memory_stats(&mem_stats);
        
        LOG_INF("FT Stats - Total Faults: %d, Successful Recoveries: %d",
                ft_stats.total_faults, ft_stats.successful_recoveries);
        LOG_INF("Memory - Used: %zu, Free: %zu, Tracked: %d",
                mem_stats.heap_used, mem_stats.heap_free, mem_stats.tracked_allocations);
    }
}

/**
 * @brief Print final stress test results
 */
static void print_stress_test_results(void)
{
    int64_t total_runtime_ms = k_uptime_get() - stress_state.test_start_time;
    uint32_t hours = total_runtime_ms / (60 * 60 * 1000);
    uint32_t minutes = (total_runtime_ms % (60 * 60 * 1000)) / (60 * 1000);
    uint32_t seconds = (total_runtime_ms % (60 * 1000)) / 1000;
    
    struct ft_fault_stats ft_stats;
    struct ft_memory_stats mem_stats;
    
    ft_get_stats(&ft_stats);
    ft_get_memory_stats(&mem_stats);
    
    printk("\n");
    printk("==================================================\n");
    printk("    LONG-RUNNING STRESS TEST RESULTS\n");
    printk("    ECE753 Fault Tolerance Framework Evaluation\n");
    printk("==================================================\n");
    printk("Total Runtime: %dh %dm %ds\n", hours, minutes, seconds);
    printk("Final Phase: %s\n", phase_configs[stress_state.current_phase].name);
    printk("\n");
    
    printk("--- MEMORY STRESS RESULTS ---\n");
    printk("Allocations: %d\n", stress_state.memory_allocations);
    printk("Deallocations: %d\n", stress_state.memory_deallocations);
    printk("Allocation Failures: %d\n", stress_state.memory_failures);
    printk("Current Tracked Allocations: %d\n", stress_state.allocation_count);
    printk("Heap Used: %zu bytes\n", mem_stats.heap_used);
    printk("Heap Free: %zu bytes\n", mem_stats.heap_free);
    
    if (stress_state.memory_allocations > 0) {
        uint32_t success_rate = ((stress_state.memory_allocations - stress_state.memory_failures) * 100) 
                               / stress_state.memory_allocations;
        printk("Memory Allocation Success Rate: %d%%\n", success_rate);
    }
    printk("\n");
    
    printk("--- THREADING STRESS RESULTS ---\n");
    printk("Threads Created: %d\n", stress_state.threads_created);
    printk("Threads Destroyed: %d\n", stress_state.threads_destroyed);
    printk("Active Threads at End: %d\n", active_thread_count);
    printk("\n");
    
    printk("--- TIMING STRESS RESULTS ---\n");
    printk("Timing Violations Detected: %d\n", stress_state.timing_violations);
    if (total_runtime_ms > 0) {
        uint32_t violation_rate = (stress_state.timing_violations * 3600000) / total_runtime_ms; /* per hour */
        printk("Timing Violation Rate: %d/hour\n", violation_rate);
    }
    printk("\n");
    
    printk("--- FAULT TOLERANCE RESULTS ---\n");
    printk("Total Faults Detected: %d\n", ft_stats.total_faults);
    printk("Faults Injected by Test: %d\n", stress_state.fault_injections);
    printk("Recovery Actions Executed: %d\n", stress_state.recoveries_attempted);
    printk("Successful Recoveries: %d\n", ft_stats.successful_recoveries);
    printk("Failed Recoveries: %d\n", ft_stats.failed_recoveries);
    
    if (ft_stats.total_faults > 0) {
        uint32_t recovery_rate = (ft_stats.successful_recoveries * 100) / ft_stats.total_faults;
        printk("Recovery Success Rate: %d%%\n", recovery_rate);
    }
    
    if (ft_stats.mtbf_ms > 0) {
        printk("Mean Time Between Failures: %d ms\n", ft_stats.mtbf_ms);
    }
    printk("\n");
    
    printk("--- SYSTEM STABILITY ASSESSMENT ---\n");
    if (ft_stats.failed_recoveries == 0 && stress_state.memory_failures < 10) {
        printk("STATUS: EXCELLENT - System maintained stability\n");
    } else if (ft_stats.failed_recoveries < 5 && stress_state.memory_failures < 50) {
        printk("STATUS: GOOD - System generally stable with minor issues\n");
    } else if (ft_stats.failed_recoveries < 20) {
        printk("STATUS: FAIR - System experienced some instability\n");
    } else {
        printk("STATUS: POOR - System showed significant instability\n");
    }
    
    printk("==================================================\n");
    printk("           STRESS TEST COMPLETED\n");
    printk("==================================================\n");
}

/**
 * @brief Main application entry point
 */
int main(void)
{
    int ret;
    
    printk("Enhanced Zephyr Fault Tolerance - Long-Running Stress Test\n");
    printk("ECE753 Project - Safety-Critical Embedded Systems\n");
    printk("Duration: %d hours\n\n", STRESS_TEST_DURATION_HOURS);
    
    /* Initialize stress test state */
    k_mutex_init(&stress_state.state_mutex);
    k_sem_init(&stress_state.phase_sem, 0, 1);
    stress_state.test_start_time = k_uptime_get();
    stress_state.phase_start_time = stress_state.test_start_time;
    stress_state.current_phase = PHASE_INITIALIZATION;
    stress_state.test_running = true;
    
    /* Initialize fault tolerance framework */
    ret = ft_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize fault tolerance framework: %d", ret);
        return ret;
    }
    
    LOG_INF("Fault tolerance framework initialized");
    
    /* Register fault handlers */
    for (int i = FT_FAULT_STACK_OVERFLOW; i <= FT_FAULT_UNKNOWN; i++) {
        ft_register_fault_handler((enum ft_fault_type)i, stress_fault_handler);
    }
    
    /* Register recovery callback */
    ft_register_recovery_callback(stress_recovery_callback);
    
    LOG_INF("=== STARTING STRESS TEST ===");
    LOG_INF("Phase: %s", phase_configs[PHASE_INITIALIZATION].name);
    
    /* Start monitoring threads */
    k_thread_create(&stress_threads[0], stress_thread_stacks[0], STRESS_THREAD_STACK_SIZE,
                   fault_injection_thread, NULL, NULL, NULL,
                   K_PRIO_PREEMPT(8), 0, K_MSEC(5000));
    k_thread_name_set(&stress_threads[0], "fault_inject");
    active_thread_count = 1;
    
    /* Main test loop */
    while (stress_state.test_running) {
        int64_t elapsed_ms = k_uptime_get() - stress_state.test_start_time;
        
        /* Check if test should complete */
        if (elapsed_ms >= STRESS_TEST_DURATION_MS) {
            break;
        }
        
        /* Start phase-specific threads */
        switch (stress_state.current_phase) {
        case PHASE_MEMORY_STRESS:
        case PHASE_COMBINED_STRESS:
            if (active_thread_count < 4) {
                /* Start memory stress threads */
                for (int i = active_thread_count; i < 4 && i < MAX_STRESS_THREADS; i++) {
                    k_thread_create(&stress_threads[i], stress_thread_stacks[i], 
                                   STRESS_THREAD_STACK_SIZE,
                                   memory_stress_thread, INT_TO_POINTER(i), NULL, NULL,
                                   K_PRIO_PREEMPT(9), 0, K_NO_WAIT);
                    
                    char name[16];
                    snprintf(name, sizeof(name), "mem_stress_%d", i);
                    k_thread_name_set(&stress_threads[i], name);
                    active_thread_count++;
                }
            }
            break;
            
        case PHASE_THREAD_STRESS:
            if (active_thread_count < 3) {
                /* Start thread stress controller */
                k_thread_create(&stress_threads[1], stress_thread_stacks[1], 
                               STRESS_THREAD_STACK_SIZE,
                               thread_stress_controller, NULL, NULL, NULL,
                               K_PRIO_PREEMPT(7), 0, K_NO_WAIT);
                k_thread_name_set(&stress_threads[1], "thread_ctrl");
                active_thread_count++;
            }
            break;
            
        case PHASE_TIMING_STRESS:
            if (active_thread_count < 6) {
                /* Start timing stress threads */
                for (int i = active_thread_count; i < 6 && i < MAX_STRESS_THREADS; i++) {
                    k_thread_create(&stress_threads[i], stress_thread_stacks[i], 
                                   STRESS_THREAD_STACK_SIZE,
                                   timing_stress_thread, INT_TO_POINTER(i), NULL, NULL,
                                   K_PRIO_PREEMPT(6), 0, K_NO_WAIT);
                    
                    char name[16];
                    snprintf(name, sizeof(name), "timing_%d", i);
                    k_thread_name_set(&stress_threads[i], name);
                    active_thread_count++;
                }
            }
            break;
            
        default:
            break;
        }
        
        /* Monitor progress and manage phases */
        monitor_progress_and_phases();
        
        /* System monitoring */
        ft_monitor_memory_usage();
        ft_monitor_stack_usage();
        
        /* Sleep before next monitoring cycle */
        k_sleep(K_MSEC(1000));
    }
    
    /* Test completion */
    stress_state.test_running = false;
    stress_state.current_phase = PHASE_COMPLETE;
    
    LOG_INF("=== STOPPING STRESS TEST ===");
    
    /* Wait for threads to finish gracefully */
    k_sleep(K_MSEC(5000));
    
    /* Abort any remaining threads */
    for (uint32_t i = 0; i < active_thread_count; i++) {
        k_thread_abort(&stress_threads[i]);
    }
    
    /* Cleanup remaining allocations */
    k_mutex_lock(&stress_state.state_mutex, K_FOREVER);
    for (uint32_t i = 0; i < stress_state.allocation_count; i++) {
        if (stress_state.allocations[i]) {
            k_free(stress_state.allocations[i]);
            ft_track_deallocation(stress_state.allocations[i]);
            stress_state.memory_deallocations++;
        }
    }
    stress_state.allocation_count = 0;
    k_mutex_unlock(&stress_state.state_mutex);
    
    /* Print final results */
    print_stress_test_results();
    
    LOG_INF("Long-running stress test completed successfully");
    
    return 0;
}