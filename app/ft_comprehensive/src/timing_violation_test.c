#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance.h>

LOG_MODULE_REGISTER(timing_violation_test, LOG_LEVEL_INF);

/* Timing constraint structures */
struct timing_constraint {
    const char *task_name;
    int64_t deadline_ms;
    int64_t start_time;
    bool active;
};

#define MAX_TIMING_TASKS 5
static struct timing_constraint timing_tasks[MAX_TIMING_TASKS];
static int task_count = 0;

static void start_timing_task(const char *name, int64_t deadline_ms) {
    if (task_count < MAX_TIMING_TASKS) {
        timing_tasks[task_count].task_name = name;
        timing_tasks[task_count].deadline_ms = deadline_ms;
        timing_tasks[task_count].start_time = k_uptime_get();
        timing_tasks[task_count].active = true;
        task_count++;
        
        LOG_INF("⏱️  Started timing task '%s' with %lld ms deadline", 
               name, deadline_ms);
    }
}

static void finish_timing_task(const char *name) {
    int64_t current_time = k_uptime_get();
    
    for (int i = 0; i < task_count; i++) {
        if (timing_tasks[i].active && 
            strcmp(timing_tasks[i].task_name, name) == 0) {
            
            int64_t elapsed = current_time - timing_tasks[i].start_time;
            timing_tasks[i].active = false;
            
            LOG_INF("⏱️  Task '%s' completed in %lld ms", name, elapsed);
            
            if (elapsed > timing_tasks[i].deadline_ms) {
                LOG_ERR("🚨 TIMING VIOLATION! Task '%s' exceeded deadline", name);
                LOG_ERR("   Deadline: %lld ms, Actual: %lld ms, Overrun: %lld ms",
                       timing_tasks[i].deadline_ms, elapsed, 
                       elapsed - timing_tasks[i].deadline_ms);
                
                ft_report_fault(FT_TIMING_VIOLATION_FAULT, FT_SEVERITY_MEDIUM);
            } else {
                LOG_INF("✅ Task '%s' met deadline (%lld ms remaining)", 
                       name, timing_tasks[i].deadline_ms - elapsed);
            }
            break;
        }
    }
}

/* Timing violation test scenarios */
static void test_simple_deadline_miss(void) {
    LOG_INF("📝 Testing simple deadline miss");
    
    start_timing_task("quick_task", 100);  /* 100ms deadline */
    
    /* Simulate work that takes too long */
    LOG_INF("   Performing work that should take 50ms but will take 150ms...");
    k_sleep(K_MSEC(150));  /* Intentionally exceed deadline */
    
    finish_timing_task("quick_task");
}

static void test_periodic_task_overrun(void) {
    LOG_INF("📝 Testing periodic task overrun");
    
    const int period_ms = 200;
    const int work_time_ms = 250;  /* Longer than period */
    
    for (int cycle = 0; cycle < 3; cycle++) {
        char task_name[32];
        snprintf(task_name, sizeof(task_name), "periodic_cycle_%d", cycle);
        
        start_timing_task(task_name, period_ms);
        
        LOG_INF("   Cycle %d: Work time %dms exceeds period %dms", 
               cycle, work_time_ms, period_ms);
        k_sleep(K_MSEC(work_time_ms));
        
        finish_timing_task(task_name);
    }
}

static void test_interrupt_latency_violation(void) {
    LOG_INF("📝 Testing interrupt response latency");
    
    /* Simulate high-priority interrupt response requirement */
    start_timing_task("interrupt_response", 5);  /* Very tight 5ms deadline */
    
    LOG_INF("   Simulating interrupt handler with 5ms deadline...");
    
    /* Simulate some critical interrupt processing */
    volatile int computation = 0;
    for (int i = 0; i < 100000; i++) {
        computation += i % 7;  /* Some computation */
    }
    
    /* Add intentional delay to violate timing */
    k_sleep(K_MSEC(10));  /* This will cause violation */
    
    finish_timing_task("interrupt_response");
}

static void test_cascading_delays(void) {
    LOG_INF("📝 Testing cascading delay effects");
    
    /* Start multiple dependent tasks */
    start_timing_task("task_a", 100);
    start_timing_task("task_b", 200);
    start_timing_task("task_c", 300);
    
    /* Task A takes too long, affecting others */
    LOG_INF("   Task A taking 120ms (deadline 100ms)...");
    k_sleep(K_MSEC(120));
    finish_timing_task("task_a");
    
    /* Task B now has less time */
    LOG_INF("   Task B execution (affected by Task A delay)...");
    k_sleep(K_MSEC(100));
    finish_timing_task("task_b");
    
    /* Task C gets remaining time */
    LOG_INF("   Task C execution (further affected)...");
    k_sleep(K_MSEC(80));
    finish_timing_task("task_c");
}

static void test_watchdog_timeout(void) {
    LOG_INF("📝 Testing watchdog timeout scenario");
    
    start_timing_task("watchdog_task", 500);  /* 500ms watchdog timeout */
    
    LOG_INF("   Simulating task that should pet watchdog every 400ms...");
    
    /* First pet - OK */
    k_sleep(K_MSEC(300));
    LOG_INF("   Watchdog pet at 300ms - OK");
    
    /* Second interval - will exceed watchdog timeout */
    k_sleep(K_MSEC(400));  /* Total: 700ms, exceeds 500ms timeout */
    LOG_ERR("   Watchdog timeout would have occurred!");
    
    finish_timing_task("watchdog_task");
}

void timing_violation_test_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("⏰ Starting timing violation detection test");
    LOG_INF("   Default timing threshold: %d ms", CONFIG_FT_TIMING_VIOLATION_THRESHOLD_MS);
    
    k_sleep(K_SECONDS(1));
    
    /* Test 1: Simple deadline miss */
    test_simple_deadline_miss();
    k_sleep(K_MSEC(500));
    
    /* Test 2: Periodic task overrun */
    test_periodic_task_overrun();
    k_sleep(K_MSEC(500));
    
    /* Test 3: Interrupt latency violation */
    test_interrupt_latency_violation();
    k_sleep(K_MSEC(500));
    
    /* Test 4: Cascading delays */
    test_cascading_delays();
    k_sleep(K_MSEC(500));
    
    /* Test 5: Watchdog timeout */
    test_watchdog_timeout();
    
    /* Summary */
    LOG_INF("📊 Timing Violation Test Summary:");
    int violations = 0;
    for (int i = 0; i < task_count; i++) {
        if (!timing_tasks[i].active) {
            int64_t elapsed = k_uptime_get() - timing_tasks[i].start_time;
            if (elapsed > timing_tasks[i].deadline_ms) {
                violations++;
            }
        }
    }
    
    LOG_INF("   Total tasks: %d", task_count);
    LOG_INF("   Timing violations: %d", violations);
    
    LOG_INF("⏰ Timing violation test completed");
}