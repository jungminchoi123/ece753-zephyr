/**
 * @file main_simple.c
 * @brief Enhanced Zephyr Fault Tolerance Framework - Simplified Long-Running Stress Test
 * 
 * ECE753 Project - Safety-Critical Embedded Systems
 * A comprehensive stress testing application for fault tolerance evaluation.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fault_tolerance/fault_tolerance.h>

LOG_MODULE_REGISTER(stress_endurance, LOG_LEVEL_INF);

/* Configuration constants */
#define STRESS_TEST_DURATION_HOURS 24
#define STRESS_TEST_DURATION_MS (STRESS_TEST_DURATION_HOURS * 60 * 60 * 1000)
#define MAX_STRESS_THREADS 12
#define STRESS_THREAD_STACK_SIZE 2048
#define MAX_ALLOCATIONS 100
#define MEMORY_STRESS_ITERATIONS 500
#define FRAGMENT_ALLOCATION_SIZE 256
#define FAULT_INJECTION_INTERVAL_MS 5000
#define STATISTICS_REPORT_INTERVAL 60000

/* Test phases */
enum stress_phase {
    PHASE_INITIALIZATION = 0,
    PHASE_MEMORY_STRESS,
    PHASE_THREAD_STRESS,
    PHASE_TIMING_STRESS,
    PHASE_COMBINED_STRESS,
    PHASE_ENDURANCE_TEST,
    PHASE_COMPLETE
};

/* Global test state */
struct stress_test_state {
    struct k_mutex state_mutex;
    struct k_sem phase_sem;
    
    /* Test timing */
    int64_t test_start_time;
    int64_t phase_start_time;
    enum stress_phase current_phase;
    bool test_running;
    
    /* Statistics */
    uint32_t memory_allocations;
    uint32_t memory_deallocations;
    uint32_t memory_failures;
    uint32_t threads_created;
    uint32_t threads_destroyed;
    uint32_t timing_violations;
    uint32_t fault_injections;
    uint32_t recoveries_attempted;
    uint32_t recoveries_successful;
    uint32_t recoveries_failed;
    
    /* Memory tracking */
    void *allocations[MAX_ALLOCATIONS];
    size_t allocation_sizes[MAX_ALLOCATIONS];
    uint32_t allocation_count;
};

/* Thread management */
static struct k_thread stress_threads[MAX_STRESS_THREADS];
static K_THREAD_STACK_ARRAY_DEFINE(stress_thread_stacks, MAX_STRESS_THREADS, STRESS_THREAD_STACK_SIZE);
static uint32_t active_thread_count = 0;

/* Global state */
static struct stress_test_state stress_state = {0};

/* Phase names for reporting */
static const char *phase_names[] = {
    "Initialization",
    "Memory Stress", 
    "Thread Stress",
    "Timing Stress",
    "Combined Stress",
    "Endurance Test",
    "Complete"
};

/**
 * @brief Enhanced fault handler with adaptive recovery
 */
static enum ft_recovery_action stress_fault_handler(const struct ft_fault_context *ctx)
{
    enum ft_recovery_action action = FT_RECOVERY_NONE;
    
    k_mutex_lock(&stress_state.state_mutex, K_FOREVER);
    
    printk("Fault detected: type=%d, severity=%d, phase=%s\n", 
           ctx->fault_type, ctx->severity, phase_names[stress_state.current_phase]);
    
    /* Determine recovery action based on fault type and current phase */
    switch (ctx->fault_type) {
    case FT_FAULT_STACK_OVERFLOW:
    case FT_FAULT_MEMORY_CORRUPTION:
        action = FT_RECOVERY_SAFE_MODE;
        break;
    case FT_FAULT_RESOURCE_EXHAUSTION:
        action = (stress_state.current_phase == PHASE_MEMORY_STRESS) ? 
                 FT_RECOVERY_CUSTOM : FT_RECOVERY_RETRY;
        break;
    case FT_FAULT_DEADLOCK_DETECTED:
        action = FT_RECOVERY_RESTART;
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
 * @brief Recovery callback for tracking success/failure
 */
static int stress_recovery_callback(const struct ft_fault_context *ctx, 
                                   enum ft_recovery_action action)
{
    int result = 0;
    
    k_mutex_lock(&stress_state.state_mutex, K_FOREVER);
    
    printk("Recovery action %d executed for fault type %d\n", action, ctx->fault_type);
    
    /* Simulate recovery operations and success tracking */
    switch (action) {
    case FT_RECOVERY_CUSTOM:
        /* Cleanup operations */
        k_sleep(K_MSEC(100));
        stress_state.recoveries_successful++;
        break;
    case FT_RECOVERY_SAFE_MODE:
        printk("Entering safe mode\n");
        stress_state.recoveries_successful++;
        break;
    case FT_RECOVERY_RESTART:
        printk("Restart recovery\n");
        stress_state.recoveries_successful++;
        break;
    case FT_RECOVERY_RETRY:
        stress_state.recoveries_successful++;
        break;
    default:
        stress_state.recoveries_failed++;
        result = -ENOTSUP;
        break;
    }
    
    k_mutex_unlock(&stress_state.state_mutex);
    
    return result;
}

/**
 * @brief Memory stress testing thread
 */
static void memory_stress_thread(void *p1, void *p2, void *p3)
{
    int thread_id = POINTER_TO_INT(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    printk("Memory stress thread %d started\n", thread_id);
    
    while (stress_state.test_running && 
           (stress_state.current_phase == PHASE_MEMORY_STRESS ||
            stress_state.current_phase == PHASE_COMBINED_STRESS ||
            stress_state.current_phase == PHASE_ENDURANCE_TEST)) {
        
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
                    
                    /* Initialize memory to create realistic usage pattern */
                    memset(ptr, thread_id & 0xFF, alloc_size);
                } else {
                    k_free(ptr);
                }
            } else {
                stress_state.memory_failures++;
                
                /* Report memory exhaustion using test mode for safety */
                uintptr_t ctx[4] = {alloc_size, stress_state.allocation_count, 0, 0};
                FT_REPORT_FAULT_TEST(FT_FAULT_RESOURCE_EXHAUSTION, FT_SEVERITY_HIGH,
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
            
            k_sleep(K_MSEC(1));
        }
        
        k_sleep(K_MSEC(100));
    }
    
    printk("Memory stress thread %d completed\n", thread_id);
}

/**
 * @brief Timing stress thread - creates timing violations
 */
static void timing_stress_thread(void *p1, void *p2, void *p3)
{
    int thread_id = POINTER_TO_INT(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    printk("Timing stress thread %d started\n", thread_id);
    
    int64_t deadline_interval = 1000; /* 1 second deadlines */
    
    while (stress_state.test_running && 
           (stress_state.current_phase == PHASE_TIMING_STRESS ||
            stress_state.current_phase == PHASE_COMBINED_STRESS ||
            stress_state.current_phase == PHASE_ENDURANCE_TEST)) {
        
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
            
            FT_REPORT_FAULT_TEST(FT_FAULT_TIMING_VIOLATION, FT_SEVERITY_MEDIUM,
                                "Deadline missed", ctx);
        }
        
        k_sleep(K_MSEC(100));
    }
    
    printk("Timing stress thread %d completed\n", thread_id);
}

/**
 * @brief Fault injection thread - periodically injects various faults
 */
static void fault_injection_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    printk("Fault injection thread started\n");
    
    while (stress_state.test_running) {
        /* Wait for interval between fault injections */
        k_sleep(K_MSEC(FAULT_INJECTION_INTERVAL_MS));
        
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
        
        FT_REPORT_FAULT_TEST(fault_type, severity, "Injected fault for stress testing", ctx);
        
        k_mutex_lock(&stress_state.state_mutex, K_FOREVER);
        stress_state.fault_injections++;
        k_mutex_unlock(&stress_state.state_mutex);
    }
    
    printk("Fault injection thread completed\n");
}

/**
 * @brief Progress monitoring and phase management
 */
static void monitor_progress_and_phases(void)
{
    int64_t current_time = k_uptime_get();
    int64_t elapsed_ms = current_time - stress_state.test_start_time;
    int64_t phase_elapsed = current_time - stress_state.phase_start_time;
    
    /* Phase durations (in ms) */
    int64_t phase_durations[] = {
        30000,   /* PHASE_INITIALIZATION - 30 seconds */
        300000,  /* PHASE_MEMORY_STRESS - 5 minutes */
        300000,  /* PHASE_THREAD_STRESS - 5 minutes */
        300000,  /* PHASE_TIMING_STRESS - 5 minutes */
        600000,  /* PHASE_COMBINED_STRESS - 10 minutes */
        0,       /* PHASE_ENDURANCE_TEST - until completion */
        0        /* PHASE_COMPLETE */
    };
    
    /* Check if current phase should complete */
    if (stress_state.current_phase < PHASE_ENDURANCE_TEST) {
        int64_t duration = phase_durations[stress_state.current_phase];
        
        if (phase_elapsed >= duration) {
            /* Move to next phase */
            stress_state.current_phase++;
            stress_state.phase_start_time = current_time;
            
            printk("=== PHASE CHANGE: %s ===\n", phase_names[stress_state.current_phase]);
        }
    }
    
    /* Print progress every minute */
    static int64_t last_progress_time = 0;
    if (current_time - last_progress_time >= STATISTICS_REPORT_INTERVAL) {
        last_progress_time = current_time;
        
        uint32_t hours = elapsed_ms / (60 * 60 * 1000);
        uint32_t minutes = (elapsed_ms % (60 * 60 * 1000)) / (60 * 1000);
        
        printk("=== STRESS TEST PROGRESS ===\n");
        printk("Runtime: %dh %dm\n", hours, minutes);
        printk("Phase: %s\n", phase_names[stress_state.current_phase]);
        printk("Memory Allocs: %d, Deallocs: %d, Failures: %d\n", 
                stress_state.memory_allocations, 
                stress_state.memory_deallocations,
                stress_state.memory_failures);
        printk("Threads Created: %d, Destroyed: %d, Active: %d\n",
                stress_state.threads_created,
                stress_state.threads_destroyed, 
                active_thread_count);
        printk("Timing Violations: %d\n", stress_state.timing_violations);
        printk("Fault Injections: %d, Recoveries: %d/%d\n", 
                stress_state.fault_injections,
                stress_state.recoveries_successful,
                stress_state.recoveries_attempted);
        
        /* Get system statistics */
        struct ft_fault_stats ft_stats;
        ft_get_stats(&ft_stats);
        
        printk("FT Stats - Total: %d, Successful: %d, Failed: %d\n",
                ft_stats.total_faults, ft_stats.successful_recoveries, ft_stats.failed_recoveries);
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
    ft_get_stats(&ft_stats);
    
    printk("\n");
    printk("==================================================\n");
    printk("    LONG-RUNNING STRESS TEST RESULTS\n");
    printk("    ECE753 Fault Tolerance Framework Evaluation\n");
    printk("==================================================\n");
    printk("Total Runtime: %dh %dm %ds\n", hours, minutes, seconds);
    printk("Final Phase: %s\n", phase_names[stress_state.current_phase]);
    printk("\n");
    
    printk("--- MEMORY STRESS RESULTS ---\n");
    printk("Allocations: %d\n", stress_state.memory_allocations);
    printk("Deallocations: %d\n", stress_state.memory_deallocations);
    printk("Allocation Failures: %d\n", stress_state.memory_failures);
    printk("Current Tracked Allocations: %d\n", stress_state.allocation_count);
    
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
        uint32_t violation_rate = (stress_state.timing_violations * 3600000) / total_runtime_ms;
        printk("Timing Violation Rate: %d/hour\n", violation_rate);
    }
    printk("\n");
    
    printk("--- FAULT TOLERANCE RESULTS ---\n");
    printk("Total Faults Detected: %d\n", ft_stats.total_faults);
    printk("Faults Injected by Test: %d\n", stress_state.fault_injections);
    printk("Recovery Actions Executed: %d\n", stress_state.recoveries_attempted);
    printk("Successful Recoveries: %d\n", stress_state.recoveries_successful);
    printk("Failed Recoveries: %d\n", stress_state.recoveries_failed);
    
    if (ft_stats.total_faults > 0) {
        uint32_t recovery_rate = (ft_stats.successful_recoveries * 100) / ft_stats.total_faults;
        printk("Recovery Success Rate: %d%%\n", recovery_rate);
    }
    printk("\n");
    
    printk("--- SYSTEM STABILITY ASSESSMENT ---\n");
    if (stress_state.recoveries_failed == 0 && stress_state.memory_failures < 10) {
        printk("STATUS: EXCELLENT - System maintained stability\n");
    } else if (stress_state.recoveries_failed < 5 && stress_state.memory_failures < 50) {
        printk("STATUS: GOOD - System generally stable with minor issues\n");
    } else if (stress_state.recoveries_failed < 20) {
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
        printk("Failed to initialize fault tolerance framework: %d\n", ret);
        return ret;
    }
    
    printk("Fault tolerance framework initialized\n");
    
    /* Enable test mode for safe fault injection */
    ft_set_test_mode(true);
    
    /* Register fault handlers */
    for (int i = FT_FAULT_STACK_OVERFLOW; i <= FT_FAULT_UNKNOWN; i++) {
        ft_register_fault_handler((enum ft_fault_type)i, stress_fault_handler);
    }
    
    /* Register recovery callback */
    ft_register_recovery_callback(stress_recovery_callback);
    
    printk("=== STARTING STRESS TEST ===\n");
    printk("Phase: %s\n", phase_names[PHASE_INITIALIZATION]);
    
    /* Start fault injection thread */
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
        case PHASE_ENDURANCE_TEST:
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
                    stress_state.threads_created++;
                }
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
                    stress_state.threads_created++;
                }
            }
            break;
            
        default:
            break;
        }
        
        /* Monitor progress and manage phases */
        monitor_progress_and_phases();
        
        /* Sleep before next monitoring cycle */
        k_sleep(K_MSEC(1000));
    }
    
    /* Test completion */
    stress_state.test_running = false;
    stress_state.current_phase = PHASE_COMPLETE;
    
    printk("=== STOPPING STRESS TEST ===\n");
    
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
            stress_state.memory_deallocations++;
        }
    }
    stress_state.allocation_count = 0;
    k_mutex_unlock(&stress_state.state_mutex);
    
    /* Print final results */
    print_stress_test_results();
    
    printk("Long-running stress test completed successfully\n");
    
    return 0;
}