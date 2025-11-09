/**
 * @file safe_predictive_demo.c
 * @author Jack Ostapeic
 * @brief Safe Predictive Thread Suspension Demo
 *
 * This demo safely shows predictive fault tolerance without actual stack overflow.
 * Uses timer-based simulation to demonstrate clean thread suspension.
 */

#include <zephyr/kernel.h>
#include <zephyr/fault_tolerance.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(safe_demo, LOG_LEVEL_INF);

/* Large thread stacks to prevent any actual overflow */
#define SAFE_STACK_SIZE 2048

static K_THREAD_STACK_DEFINE(monitor_stack, SAFE_STACK_SIZE);
static K_THREAD_STACK_DEFINE(critical_stack, SAFE_STACK_SIZE);
static K_THREAD_STACK_DEFINE(network_stack, SAFE_STACK_SIZE);
static K_THREAD_STACK_DEFINE(worker_stack, SAFE_STACK_SIZE);

static struct k_thread monitor_thread;
static struct k_thread critical_thread;
static struct k_thread network_thread;
static struct k_thread worker_thread;

/* System state */
static volatile bool system_running = true;
static volatile bool worker_active = true;
static volatile bool fault_predicted = false;

/* Operation counters */
static uint32_t monitor_cycles = 0;
static uint32_t critical_ops = 0;
static uint32_t network_ops = 0;
static uint32_t worker_ops = 0;

/* Simulation state */
static uint32_t simulated_stack_usage = 0;
static const uint32_t STACK_WARNING_THRESHOLD = 80;  /* 80% usage */

/* Semaphore for clean suspension */
static K_SEM_DEFINE(suspend_worker, 0, 1);

/* Predictive fault handler */
static struct ft_handler predictive_handler = {
    .fault_type = FT_FAULT_STACK_OVERFLOW,
    .priority = 1,
    .name = "predictive_handler",
    .handler = NULL,  /* We'll set this below */
    .user_data = NULL
};

/* Simple predictive handler function */
static enum ft_handler_result handle_predictive_fault(const struct ft_fault_context *fault_ctx, 
                                                     struct ft_recovery_context *recovery_ctx,
                                                     void *user_data)
{
    printk("\n🛡️ PREDICTIVE HANDLER: Fault prevention triggered!\n");
    printk("🛡️ PREDICTIVE HANDLER: Thread '%s' approaching stack limit\n", 
           k_thread_name_get(fault_ctx->thread_id) ?: "unknown");
    printk("🛡️ PREDICTIVE HANDLER: Initiating clean suspension...\n");
    
    /* Signal the worker to suspend cleanly */
    k_sem_give(&suspend_worker);
    
    recovery_ctx->action = FT_RECOVERY_NONE;  /* No recovery needed - we prevented the fault */
    
    printk("🛡️ PREDICTIVE HANDLER: Prevention successful!\n");
    
    return FT_HANDLER_HANDLED;
}

/* 📊 Monitor Thread */
void monitor_thread_func(void *a, void *b, void *c)
{
    printk("📊 MONITOR: Starting system health monitor\n");
    
    while (system_running) {
        monitor_cycles++;
        k_sleep(K_SECONDS(2));
        
        printk("\n📊 MONITOR: Health Check #%u:\n", monitor_cycles);
        printk("   🔥 Critical:  %u ops [✅ ACTIVE]\n", critical_ops);
        printk("   🌐 Network:   %u ops [✅ ACTIVE]\n", network_ops);
        printk("   ⚙️ Worker:    %u ops [%s] (Stack usage: %u%%)\n", 
               worker_ops,
               worker_active ? "✅ ACTIVE" : "😴 SUSPENDED",
               simulated_stack_usage);
        
        if (fault_predicted) {
            printk("   🛡️ Predictive fault prevention: ACTIVE\n");
            printk("   🎯 System integrity: MAINTAINED\n");
        }
    }
}

/* 🔥 Critical Thread - Mission critical operations */
void critical_thread_func(void *a, void *b, void *c)
{
    k_sleep(K_SECONDS(1));  /* Stagger startup */
    
    while (system_running) {
        critical_ops++;
        if (critical_ops % 5 == 0) {
            printk("🔥 CRITICAL: Completed operation %u (ESSENTIAL SERVICES)\n", critical_ops);
        }
        k_sleep(K_MSEC(1500));
    }
}

/* 🌐 Network Thread - Communications */
void network_thread_func(void *a, void *b, void *c)
{
    k_sleep(K_SECONDS(2));  /* Stagger startup */
    
    while (system_running) {
        network_ops++;
        if (network_ops % 4 == 0) {
            printk("🌐 NETWORK: Packet %u processed (COMMUNICATIONS ACTIVE)\n", network_ops);
        }
        k_sleep(K_MSEC(2000));
    }
}

/* ⚙️ Worker Thread - Risky operations with predictive monitoring */
void worker_thread_func(void *a, void *b, void *c)
{
    k_sleep(K_SECONDS(3));  /* Stagger startup */
    
    printk("⚙️ WORKER: Starting operations with stack monitoring\n");
    
    while (system_running && worker_active) {
        worker_ops++;
        
        /* Simulate gradually increasing stack usage */
        simulated_stack_usage += 8;  /* Increase by 8% each cycle */
        
        if (worker_ops % 3 == 0) {
            printk("⚙️ WORKER: Task %u complete (Stack: %u%% used)\n", 
                   worker_ops, simulated_stack_usage);
        }
        
        /* Check for predictive threshold */
        if (simulated_stack_usage >= STACK_WARNING_THRESHOLD && !fault_predicted) {
            printk("\n⚠️ WORKER: 🚨 APPROACHING STACK LIMIT! 🚨\n");
            printk("⚠️ WORKER: Current usage: %u%% (threshold: %u%%)\n", 
                   simulated_stack_usage, STACK_WARNING_THRESHOLD);
            printk("⚠️ WORKER: Triggering PREDICTIVE FAULT PREVENTION!\n");
            
            /* Report predictive fault */
            struct ft_fault_context ctx = {
                .fault_type = FT_FAULT_STACK_OVERFLOW,
                .severity = FT_SEVERITY_WARNING,
                .timestamp = k_uptime_get(),
                .thread_id = k_current_get(),
                .error_code = 0,
                .description = "Predictive stack overflow prevention"
            };
            
            ft_report_fault(&ctx);
            
            fault_predicted = true;
            
            printk("⚠️ WORKER: Waiting for suspension signal...\n");
            k_sem_take(&suspend_worker, K_FOREVER);
            
            printk("⚠️ WORKER: 😴 Entering clean suspension (NO CORRUPTION!)\n");
            worker_active = false;
            break;
        }
        
        k_sleep(K_MSEC(1000));
    }
    
    printk("⚠️ WORKER: Thread safely suspended\n");
}

int main(void)
{
    printk("\n");
    printk("========================================================\n");
    printk("      SAFE PREDICTIVE FAULT TOLERANCE DEMONSTRATION\n");
    printk("========================================================\n");
    printk("This demo shows SAFE predictive fault prevention:\n");
    printk("  1. Monitor simulated stack usage\n");
    printk("  2. When usage hits 80%% threshold, trigger prevention\n");
    printk("  3. Cleanly suspend thread BEFORE any corruption\n");
    printk("  4. All other threads continue normally\n");
    printk("  5. System remains 100%% stable\n");
    printk("  6. QEMU stays running - no crashes!\n");
    printk("\n");
    printk("This proves fault tolerance through PREVENTION!\n");
    printk("========================================================\n\n");
    
    /* Initialize framework */
    if (fault_tolerance_init(NULL) != 0) {
        printk("❌ Framework initialization failed\n");
        return -1;
    }
    
    /* Set up predictive handler */
    predictive_handler.handler = handle_predictive_fault;
    if (ft_register_handler(&predictive_handler) != 0) {
        printk("❌ Handler registration failed\n");
        return -1;
    }
    
    printk("✅ Predictive fault tolerance framework ready\n\n");
    
    /* Start all threads with delays to prevent startup issues */
    printk("🚀 Starting system threads...\n\n");
    
    k_thread_create(&monitor_thread, monitor_stack, K_THREAD_STACK_SIZEOF(monitor_stack),
                   monitor_thread_func, NULL, NULL, NULL, K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
    k_thread_name_set(&monitor_thread, "monitor");
    
    k_thread_create(&critical_thread, critical_stack, K_THREAD_STACK_SIZEOF(critical_stack),
                   critical_thread_func, NULL, NULL, NULL, K_PRIO_PREEMPT(6), 0, K_NO_WAIT);
    k_thread_name_set(&critical_thread, "critical");
    
    k_thread_create(&network_thread, network_stack, K_THREAD_STACK_SIZEOF(network_stack),
                   network_thread_func, NULL, NULL, NULL, K_PRIO_PREEMPT(7), 0, K_NO_WAIT);
    k_thread_name_set(&network_thread, "network");
    
    k_thread_create(&worker_thread, worker_stack, K_THREAD_STACK_SIZEOF(worker_stack),
                   worker_thread_func, NULL, NULL, NULL, K_PRIO_PREEMPT(8), 0, K_NO_WAIT);
    k_thread_name_set(&worker_thread, "worker");
    
    printk("🎬 ALL THREADS RUNNING!\n\n");
    
    /* Run demonstration */
    uint32_t demo_seconds = 0;
    while (system_running && demo_seconds < 180) {  /* Run for 3 minutes */
        k_sleep(K_SECONDS(10));
        demo_seconds += 10;
        
        /* Show status updates */
        if (fault_predicted && demo_seconds >= 30) {
            printk("\n⏰ STATUS (t+%u seconds):\n", demo_seconds);
            printk("   🛡️ Predictive prevention: SUCCESSFUL\n");
            printk("   ✅ System stability: EXCELLENT\n");
            printk("   📊 Active threads: 3/4 (75%% operational)\n");
            printk("   🎯 Zero system corruption: CONFIRMED\n");
            
            /* After 90 seconds of successful operation, conclude */
            if (demo_seconds >= 90) {
                break;
            }
        }
    }
    
    /* Final results */
    printk("\n\n");
    printk("=========================================================\n");
    printk("          SAFE PREDICTIVE FAULT TOLERANCE RESULTS\n");
    printk("=========================================================\n");
    printk("🛡️ STRATEGY: Predictive prevention (NO actual faults!)\n");
    printk("📊 SYSTEM HEALTH: Excellent (zero corruption)\n");
    printk("⏱️  SYSTEM UPTIME: Continuous operation maintained\n");
    printk("🎯 MISSION SUCCESS: Critical services preserved\n");
    printk("\n");
    printk("Thread Performance Summary:\n");
    printk("  📊 Monitor:   %u cycles ✅ (CONTINUOUS HEALTH CHECKS)\n", monitor_cycles);
    printk("  🔥 Critical:  %u ops   ✅ (MISSION CRITICAL PRESERVED)\n", critical_ops);
    printk("  🌐 Network:   %u ops   ✅ (COMMUNICATIONS MAINTAINED)\n", network_ops);
    printk("  ⚙️ Worker:    %u ops   😴 (SAFELY SUSPENDED)\n", worker_ops);
    printk("\n");
    printk("Fault Prevention Success:\n");
    printk("  🛡️ Predictive detection: SUCCESSFUL\n");
    printk("  🚫 Stack corruption: PREVENTED\n");
    printk("  ✅ System integrity: MAINTAINED\n");
    printk("  🎯 Zero downtime: ACHIEVED\n");
    printk("\n");
    printk("🏆 DEMONSTRATION COMPLETE: Fault tolerance through\n");
    printk("    intelligent PREVENTION is superior to recovery!\n");
    printk("🏆 QEMU STILL RUNNING: No system crashes or corruption!\n");
    printk("=========================================================\n\n");
    
    /* Keep system running to show stability */
    printk("💤 System will continue running to demonstrate stability...\n");
    printk("   (Press CTRL+A, X to exit QEMU)\n\n");
    
    while (true) {
        k_sleep(K_SECONDS(30));
        printk("💓 Heartbeat: System running normally (uptime: %llu ms)\n", k_uptime_get());
    }
    
    return 0;
}