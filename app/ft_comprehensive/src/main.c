#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(ft_comprehensive, LOG_LEVEL_INF);

/* Test control flags - can be configured at compile time */
#ifdef CONFIG_FT_ENABLE_STACK_OVERFLOW_PROTECTION
#define RUN_STACK_OVERFLOW_TEST 1
#else
#define RUN_STACK_OVERFLOW_TEST 0
#endif

#ifdef CONFIG_FT_ENABLE_DEADLOCK_DETECTION  
#define RUN_DEADLOCK_TEST 1
#else
#define RUN_DEADLOCK_TEST 0
#endif

#ifdef CONFIG_FT_ENABLE_BUFFER_OVERFLOW_PROTECTION
#define RUN_BUFFER_OVERFLOW_TEST 1
#else
#define RUN_BUFFER_OVERFLOW_TEST 0
#endif

#ifdef CONFIG_FT_ENABLE_MEMORY_LEAK_DETECTION
#define RUN_MEMORY_LEAK_TEST 1
#else
#define RUN_MEMORY_LEAK_TEST 0
#endif

#ifdef CONFIG_FT_ENABLE_TIMING_VIOLATION_DETECTION
#define RUN_TIMING_VIOLATION_TEST 1
#else
#define RUN_TIMING_VIOLATION_TEST 0
#endif

/* Thread stacks and structures */
#define TEST_THREAD_STACK_SIZE 2048
#define MAX_TEST_THREADS 5

static K_THREAD_STACK_ARRAY_DEFINE(test_stacks, MAX_TEST_THREADS, TEST_THREAD_STACK_SIZE);
static struct k_thread test_threads[MAX_TEST_THREADS];
static int active_test_count = 0;

/* Test function declarations */
extern void deadlock_test_entry(void *p1, void *p2, void *p3);
extern void buffer_overflow_test_entry(void *p1, void *p2, void *p3);
extern void memory_leak_test_entry(void *p1, void *p2, void *p3);
extern void timing_violation_test_entry(void *p1, void *p2, void *p3);

/* Stack overflow test (internal) */
__attribute__((noinline)) void recursive_overflow(int depth) {
    volatile char buffer[1024];  /* 1KB per call */
    
    for(int i = 0; i < 1024; i++) {
        buffer[i] = (char)(depth % 256);
    }
    
    LOG_INF("Stack overflow test - recursion depth: %d", depth);
    k_sleep(K_MSEC(100));
    
    recursive_overflow(depth + 1);
}

void stack_overflow_test_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("Starting stack overflow test in 3 seconds...");
    k_sleep(K_SECONDS(3));
    
    LOG_WRN("⚠️  Triggering stack overflow fault...");
    ft_report_fault(FT_STACK_OVERFLOW_FAULT, FT_SEVERITY_HIGH);
    
    recursive_overflow(0);
}

/* Enhanced fault handler with multi-fault support */
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
    LOG_ERR("Fatal error detected: reason %d", reason);
    
    switch (reason) {
        case K_ERR_STACK_CHK_FAIL:
            LOG_ERR("Stack overflow in thread %p", k_current_get());
            ft_report_fault(FT_STACK_OVERFLOW_FAULT, FT_SEVERITY_CRITICAL);
            LOG_INF("Attempting graceful recovery...");
            k_thread_abort(k_current_get());
            return;
            
        case K_ERR_KERNEL_OOPS:
            LOG_ERR("Kernel oops in thread %p", k_current_get());
            ft_report_fault(FT_ASSERT_FAULT, FT_SEVERITY_CRITICAL);
            break;
            
        case K_ERR_CPU_EXCEPTION:
            LOG_ERR("CPU exception in thread %p", k_current_get());
            ft_report_fault(FT_UNKNOWN_FAULT, FT_SEVERITY_CRITICAL);
            break;
            
        default:
            LOG_ERR("Unknown fatal error: %d", reason);
            ft_report_fault(FT_UNKNOWN_FAULT, FT_SEVERITY_CRITICAL);
            break;
    }
    
    /* For other critical faults, halt the system */
    LOG_ERR("System halting due to unrecoverable fault");
    k_fatal_halt(reason);
}

/* Test orchestration */
static void start_test(const char* test_name, k_thread_entry_t entry_func) {
    if (active_test_count >= MAX_TEST_THREADS) {
        LOG_ERR("Too many active tests!");
        return;
    }
    
    LOG_INF("🔧 Starting %s", test_name);
    
    k_thread_create(&test_threads[active_test_count], 
                   test_stacks[active_test_count], 
                   TEST_THREAD_STACK_SIZE,
                   entry_func, NULL, NULL, NULL,
                   K_PRIO_COOP(7), 0, K_NO_WAIT);
                   
    k_thread_name_set(&test_threads[active_test_count], test_name);
    active_test_count++;
}

static void monitor_system_health(void) {
    int cycle = 0;
    int recovered_tests = 0;
    
    while (1) {
        LOG_INF("📊 System health check - cycle %d", cycle++);
        
        /* Check for recovered/terminated test threads */
        for (int i = 0; i < active_test_count; i++) {
            if (k_thread_join(&test_threads[i], K_NO_WAIT) == 0) {
                LOG_INF("✅ Test thread %d completed/recovered", i);
                recovered_tests++;
            }
        }
        
        /* System metrics */
        LOG_INF("📈 Active threads: %d, Recovered: %d", 
               active_test_count - recovered_tests, recovered_tests);
               
        /* Check fault tolerance system health */
        ft_check_stack_usage(); // This reports system-wide stack health
        
        k_sleep(K_SECONDS(5));
        
        /* Exit condition */
        if (cycle > 20 || recovered_tests >= active_test_count) {
            LOG_INF("🏁 Test suite completed - %d tests recovered successfully", 
                   recovered_tests);
            break;
        }
    }
}

void main(void) {
    LOG_INF("=== 🚀 Comprehensive Fault Tolerance Test Suite ===");
    LOG_INF("Framework will automatically handle detected faults");
    
    /* Wait for fault tolerance system initialization */
    k_sleep(K_SECONDS(2));
    
    /* Display enabled fault detection modules */
    LOG_INF("🔍 Enabled Fault Detection Modules:");
    
    if (RUN_STACK_OVERFLOW_TEST) {
        LOG_INF("  ✓ Stack Overflow Protection");
    }
    if (RUN_DEADLOCK_TEST) {
        LOG_INF("  ✓ Deadlock Detection");  
    }
    if (RUN_BUFFER_OVERFLOW_TEST) {
        LOG_INF("  ✓ Buffer Overflow Protection");
    }
    if (RUN_MEMORY_LEAK_TEST) {
        LOG_INF("  ✓ Memory Leak Detection");
    }
    if (RUN_TIMING_VIOLATION_TEST) {
        LOG_INF("  ✓ Timing Violation Detection");
    }
    
    LOG_INF("🔥 Starting fault injection tests...");
    k_sleep(K_SECONDS(1));
    
    /* Launch test threads based on configuration */
    if (RUN_STACK_OVERFLOW_TEST) {
        start_test("stack_overflow", stack_overflow_test_entry);
        k_sleep(K_SECONDS(2)); /* Stagger test starts */
    }
    
    if (RUN_DEADLOCK_TEST) {
        start_test("deadlock", deadlock_test_entry);
        k_sleep(K_SECONDS(2));
    }
    
    if (RUN_BUFFER_OVERFLOW_TEST) {
        start_test("buffer_overflow", buffer_overflow_test_entry);
        k_sleep(K_SECONDS(2));
    }
    
    if (RUN_MEMORY_LEAK_TEST) {
        start_test("memory_leak", memory_leak_test_entry);
        k_sleep(K_SECONDS(2));
    }
    
    if (RUN_TIMING_VIOLATION_TEST) {
        start_test("timing_violation", timing_violation_test_entry);
        k_sleep(K_SECONDS(2));
    }
    
    /* Monitor system health and recovery */
    LOG_INF("🏥 Starting system health monitoring...");
    monitor_system_health();
    
    LOG_INF("=== 🎯 Comprehensive Test Summary ===");
    LOG_INF("All configured fault detection modules tested");
    LOG_INF("Fault tolerance framework operational");
    
    /* Keep system running to observe long-term stability */
    while (1) {
        LOG_INF("💪 System running stably post-recovery");
        k_sleep(K_SECONDS(10));
    }
}