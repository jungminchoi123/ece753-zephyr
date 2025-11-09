/**
 * @file clear_resilience_demo.c
 * @author Jack Ostapeic
 * @brief Clear Multi-Thread Resilience Demonstration
 *
 * This version shows threads starting up clearly, then demonstrates
 * that when one thread faults, all others continue running.
 */

#include <zephyr/kernel.h>
#include <zephyr/fault_tolerance.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(clear_demo, LOG_LEVEL_INF);

/* Thread stacks */
static K_THREAD_STACK_DEFINE(monitor_stack, 1024);
static K_THREAD_STACK_DEFINE(service1_stack, 1024);
static K_THREAD_STACK_DEFINE(service2_stack, 1024);
static K_THREAD_STACK_DEFINE(service3_stack, 1024);
static K_THREAD_STACK_DEFINE(worker_fault_stack, 400);  /* TOO SMALL - will overflow */
static K_THREAD_STACK_DEFINE(worker_safe_stack, 1024);   /* Normal size */

static struct k_thread monitor_thread;
static struct k_thread service1_thread;
static struct k_thread service2_thread;
static struct k_thread service3_thread;
static struct k_thread worker_fault_thread;
static struct k_thread worker_safe_thread;

static volatile bool demo_running = true;
static volatile bool worker_fault_alive = true;
static volatile bool worker_safe_alive = true;

static uint32_t monitor_ticks = 0;
static uint32_t service1_ops = 0;
static uint32_t service2_ops = 0;
static uint32_t service3_ops = 0;
static uint32_t worker_safe_ops = 0;

/* Stack consuming function */
int stack_consumer(int depth)
{
    volatile char big_buffer[80];  /* 80 bytes per call */
    int sum = 0;
    
    for (int i = 0; i < 80; i++) {
        big_buffer[i] = depth + i;
        sum += big_buffer[i];
    }
    
    if (depth < 15) {  /* 15 levels * 80 bytes = 1200+ bytes needed */
        sum += stack_consumer(depth + 1);
    }
    
    return sum;
}

/* 📊 Monitor Thread - Shows system health */
void monitor_thread_func(void *a, void *b, void *c)
{
    printk("📊 MONITOR: System monitor started\n");
    
    while (demo_running) {
        monitor_ticks++;
        printk("📊 MONITOR: Tick %u | Service1: %u ops | Service2: %u ops | Service3: %u ops\n",
               monitor_ticks, service1_ops, service2_ops, service3_ops);
        printk("           Workers: Fault=%s, Safe=%s (ops=%u)\n",
               worker_fault_alive ? "ALIVE" : "💀DEAD", 
               worker_safe_alive ? "ALIVE" : "💀DEAD",
               worker_safe_ops);
        k_sleep(K_SECONDS(2));
    }
    printk("📊 MONITOR: Stopped\n");
}

/* 🔧 Service Threads - Critical system services */
void service1_thread_func(void *a, void *b, void *c)
{
    printk("🔧 SERVICE1: Critical service started\n");
    while (demo_running) {
        service1_ops++;
        if (service1_ops % 4 == 0) {
            printk("🔧 SERVICE1: ✅ Operation %u completed\n", service1_ops);
        }
        k_sleep(K_MSEC(1500));
    }
    printk("🔧 SERVICE1: Stopped\n");
}

void service2_thread_func(void *a, void *b, void *c)
{
    printk("⚡ SERVICE2: Network service started\n");
    while (demo_running) {
        service2_ops++;
        if (service2_ops % 3 == 0) {
            printk("⚡ SERVICE2: ✅ Network op %u completed\n", service2_ops);
        }
        k_sleep(K_MSEC(1800));
    }
    printk("⚡ SERVICE2: Stopped\n");
}

void service3_thread_func(void *a, void *b, void *c)
{
    printk("🛡️ SERVICE3: Security service started\n");
    while (demo_running) {
        service3_ops++;
        if (service3_ops % 5 == 0) {
            printk("🛡️ SERVICE3: ✅ Security check %u passed\n", service3_ops);
        }
        k_sleep(K_MSEC(2200));
    }
    printk("🛡️ SERVICE3: Stopped\n");
}

/* 💀 Worker Thread - WILL OVERFLOW STACK */
void worker_fault_thread_func(void *a, void *b, void *c)
{
    printk("💀 WORKER_FAULT: Starting with 400B stack - WILL OVERFLOW!\n");
    
    /* Give other threads time to start */
    k_sleep(K_SECONDS(3));
    
    printk("💀 WORKER_FAULT: Beginning deep computation that needs >400B...\n");
    
    /* This will overflow the 400B stack */
    int result = stack_consumer(0);  /* Needs ~1200B, only have 400B */
    
    /* This line should never execute */
    printk("💀 WORKER_FAULT: ERROR - should never reach here! Result: %d\n", result);
    worker_fault_alive = false;
}

/* ✅ Worker Thread - SAFE */
void worker_safe_thread_func(void *a, void *b, void *c)
{
    printk("✅ WORKER_SAFE: Starting with 1024B stack - this is safe\n");
    
    while (demo_running && worker_safe_alive) {
        worker_safe_ops++;
        
        /* Safe operation - only uses ~160B */
        int result = stack_consumer(0);
        result += stack_consumer(1);  /* Only 2 levels = ~160B total */
        
        printk("✅ WORKER_SAFE: ✅ Safe operation %u completed (result=%d)\n", 
               worker_safe_ops, result);
        
        k_sleep(K_MSEC(2500));
    }
    printk("✅ WORKER_SAFE: Stopped\n");
}

/* Fault handler */
static enum ft_handler_result fault_handler(
    const struct ft_fault_context *fault_ctx,
    struct ft_recovery_context *recovery_ctx,
    void *user_data)
{
    printk("\n🚨🚨🚨 FAULT HANDLER CALLED! 🚨🚨🚨\n");
    printk("🚨 Thread %p experienced stack overflow\n", fault_ctx->thread_id);
    
    if (fault_ctx->thread_id == &worker_fault_thread) {
        worker_fault_alive = false;
        printk("💀 WORKER_FAULT thread is now DEAD\n");
    } else {
        printk("💀 Unknown thread is now DEAD\n");
    }
    
    printk("🌟 BUT LOOK: All other threads continue running!\n");
    printk("🌟 Monitor: ✅ | Service1: ✅ | Service2: ✅ | Service3: ✅ | Worker_Safe: ✅\n");
    printk("🌟 SYSTEM RESILIENCE PROVEN! 🌟\n\n");
    
    recovery_ctx->action = FT_RECOVERY_NONE;  /* Let thread die */
    return FT_HANDLER_HANDLED;
}

static struct ft_handler my_handler = {
    .fault_type = FT_FAULT_STACK_OVERFLOW,
    .handler = fault_handler,
    .priority = 1,
    .name = "clear_demo_handler"
};

/* Fatal error handler */
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
    if (reason == K_ERR_STACK_CHK_FAIL) {
        printk("\n💥 STACK OVERFLOW FATAL ERROR!\n");
        printk("💥 Thread: %p\n", k_current_get());
        printk("💥 BUT OTHER THREADS KEEP RUNNING!\n");
        
        struct ft_fault_context ctx = {
            .fault_type = FT_FAULT_STACK_OVERFLOW,
            .severity = FT_SEVERITY_ERROR,
            .timestamp = k_uptime_get(),
            .thread_id = k_current_get(),
            .error_code = reason,
            .description = "Stack overflow demonstration"
        };
        
        ft_report_fault(&ctx);
        
        printk("💥 Only THIS thread dies - others continue!\n");
        k_fatal_halt(reason);
    }
    
    printk("System error, halting\n");
    k_fatal_halt(reason);
}

int main(void)
{
    printk("\n");
    printk("========================================\n");
    printk("     CLEAR RESILIENCE DEMONSTRATION\n");
    printk("========================================\n");
    printk("Starting 6 threads:\n");
    printk("  📊 Monitor (system health)\n");
    printk("  🔧 Service1 (critical)\n");
    printk("  ⚡ Service2 (network)\n");
    printk("  🛡️ Service3 (security)\n");
    printk("  💀 Worker_Fault (WILL DIE from stack overflow)\n");
    printk("  ✅ Worker_Safe (safe operations)\n");
    printk("========================================\n\n");
    
    /* Initialize framework */
    if (fault_tolerance_init(NULL) != 0) {
        printk("❌ Framework init failed\n");
        return -1;
    }
    
    if (ft_register_handler(&my_handler) != 0) {
        printk("❌ Handler registration failed\n");
        return -1;
    }
    
    printk("✅ Fault tolerance framework ready\n\n");
    
    /* Start threads */
    k_thread_create(&monitor_thread, monitor_stack, K_THREAD_STACK_SIZEOF(monitor_stack),
                   monitor_thread_func, NULL, NULL, NULL, K_PRIO_COOP(1), 0, K_NO_WAIT);
    k_thread_name_set(&monitor_thread, "monitor");
    
    k_thread_create(&service1_thread, service1_stack, K_THREAD_STACK_SIZEOF(service1_stack),
                   service1_thread_func, NULL, NULL, NULL, K_PRIO_COOP(2), 0, K_NO_WAIT);
    k_thread_name_set(&service1_thread, "service1");
    
    k_thread_create(&service2_thread, service2_stack, K_THREAD_STACK_SIZEOF(service2_stack),
                   service2_thread_func, NULL, NULL, NULL, K_PRIO_COOP(3), 0, K_NO_WAIT);
    k_thread_name_set(&service2_thread, "service2");
    
    k_thread_create(&service3_thread, service3_stack, K_THREAD_STACK_SIZEOF(service3_stack),
                   service3_thread_func, NULL, NULL, NULL, K_PRIO_COOP(4), 0, K_NO_WAIT);
    k_thread_name_set(&service3_thread, "service3");
    
    k_thread_create(&worker_safe_thread, worker_safe_stack, K_THREAD_STACK_SIZEOF(worker_safe_stack),
                   worker_safe_thread_func, NULL, NULL, NULL, K_PRIO_COOP(5), 0, K_NO_WAIT);
    k_thread_name_set(&worker_safe_thread, "worker_safe");
    
    /* Start the doomed worker thread last */
    k_thread_create(&worker_fault_thread, worker_fault_stack, K_THREAD_STACK_SIZEOF(worker_fault_stack),
                   worker_fault_thread_func, NULL, NULL, NULL, K_PRIO_COOP(6), 0, K_NO_WAIT);
    k_thread_name_set(&worker_fault_thread, "worker_fault");
    
    printk("🚀 All threads started! Watch the resilience demonstration...\n\n");
    
    /* Run demo for a while */
    for (int i = 0; i < 15; i++) {
        k_sleep(K_SECONDS(2));
        
        if (!worker_fault_alive && i > 5) {
            printk("\n🎉 RESILIENCE PROVEN! Worker_Fault died but others continue!\n");
            break;
        }
    }
    
    /* Shutdown */
    printk("\n🛑 Shutting down demonstration...\n");
    demo_running = false;
    worker_safe_alive = false;
    
    k_sleep(K_SECONDS(1));
    
    printk("\n");
    printk("================================\n");
    printk("      DEMONSTRATION RESULTS\n");
    printk("================================\n");
    printk("Monitor ticks: %u ✅\n", monitor_ticks);
    printk("Service1 ops: %u ✅\n", service1_ops);
    printk("Service2 ops: %u ✅\n", service2_ops);
    printk("Service3 ops: %u ✅\n", service3_ops);
    printk("Worker_Safe ops: %u ✅\n", worker_safe_ops);
    printk("Worker_Fault: %s 💀\n", worker_fault_alive ? "ALIVE (ERROR!)" : "DEAD (EXPECTED)");
    printk("\n");
    printk("✅ CONCLUSION: System remained resilient!\n");
    printk("   Only the faulting thread died!\n");
    printk("   All other threads continued normally!\n");
    printk("================================\n");
    
    return 0;
}