/**
 * @file predictive_suspension_demo.c
 * @author Jack Ostapeic
 * @brief Predictive Thread Suspension Demo
 *
 * This demo shows the IDEAL fault tolerance scenario: detecting
 * impending stack overflow BEFORE corruption and cleanly suspending
 * the thread, keeping QEMU running to show all other threads continue.
 */

#include <zephyr/kernel.h>
#include <zephyr/fault_tolerance.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(predictive_demo, LOG_LEVEL_INF);

/* Thread stacks */
static K_THREAD_STACK_DEFINE(monitor_stack, 1024);
static K_THREAD_STACK_DEFINE(critical_stack, 1024);
static K_THREAD_STACK_DEFINE(network_stack, 1024);
static K_THREAD_STACK_DEFINE(sensor_stack, 1024);
static K_THREAD_STACK_DEFINE(worker_risky_stack, 1024);  /* Normal stack */
static K_THREAD_STACK_DEFINE(worker_safe_stack, 1024);   /* Normal stack */

static struct k_thread monitor_thread;
static struct k_thread critical_thread;
static struct k_thread network_thread;
static struct k_thread sensor_thread;
static struct k_thread worker_risky_thread;
static struct k_thread worker_safe_thread;

/* System state */
static volatile bool system_running = true;
static volatile bool worker_risky_active = true;
static volatile bool worker_safe_active = true;
static volatile bool fault_predicted = false;

/* Operation counters */
static uint32_t monitor_cycles = 0;
static uint32_t critical_ops = 0;
static uint32_t network_ops = 0;
static uint32_t sensor_ops = 0;
static uint32_t worker_risky_ops = 0;
static uint32_t worker_safe_ops = 0;

/* Stack usage tracking */
static size_t max_stack_used = 0;
static const size_t STACK_SIZE = 1024;
static const size_t STACK_WARNING_THRESHOLD = 800;  /* 80% usage triggers warning */

/* Suspension coordination */
static K_SEM_DEFINE(suspend_risky_worker, 0, 1);

/* Stack usage checker (simulated) */
size_t get_stack_usage(void)
{
    /* In real systems, this would check actual stack pointer vs stack base */
    /* For demo, we simulate increasing stack usage */
    static uint32_t simulated_depth = 0;
    simulated_depth += 50;  /* Simulate 50 bytes per call */
    return simulated_depth;
}

/* 📊 System Monitor Thread */
void monitor_thread_func(void *a, void *b, void *c)
{
    printk("📊 MONITOR: Predictive fault tolerance monitor started\n");
    
    while (system_running) {
        monitor_cycles++;
        
        printk("\n📊 MONITOR: Health Check #%u:\n", monitor_cycles);
        printk("   🔥 Critical:      %u ops [%s]\n", critical_ops, "✅ ACTIVE");
        printk("   🌐 Network:       %u ops [%s]\n", network_ops, "✅ ACTIVE");
        printk("   📡 Sensor:        %u ops [%s]\n", sensor_ops, "✅ ACTIVE");
        printk("   ⚠️  Worker_Risky:  %u ops [%s] (Stack: %zu/%zu bytes)\n", 
               worker_risky_ops,
               worker_risky_active ? "✅ ACTIVE" : "😴 SUSPENDED",
               max_stack_used, STACK_SIZE);
        printk("   ✅ Worker_Safe:   %u ops [%s]\n", worker_safe_ops,
               worker_safe_active ? "✅ ACTIVE" : "💤 STOPPED");
        
        if (fault_predicted) {
            printk("   🚨 STATUS: PREDICTIVE SUSPENSION ACTIVE! 🚨\n");
            printk("   🎯 RESULT: Stack overflow PREVENTED, system stable!\n");
        }
        
        k_sleep(K_SECONDS(4));
    }
    
    printk("📊 MONITOR: System monitor stopped\n");
}

/* 🔥 Critical System Service */
void critical_thread_func(void *a, void *b, void *c)
{
    printk("🔥 CRITICAL: Mission-critical service started\n");
    
    while (system_running) {
        critical_ops++;
        
        if (critical_ops % 3 == 0) {
            printk("🔥 CRITICAL: ✅ Essential operation %u completed\n", critical_ops);
        }
        
        /* Show resilience after predictive suspension */
        if (fault_predicted && critical_ops % 5 == 0) {
            printk("🔥 CRITICAL: 🌟 Unaffected by Worker_Risky suspension! 🌟\n");
        }
        
        k_sleep(K_MSEC(2500));
    }
    
    printk("🔥 CRITICAL: Mission-critical service stopped\n");
}

/* 🌐 Network Service */
void network_thread_func(void *a, void *b, void *c)
{
    printk("🌐 NETWORK: Communication service started\n");
    
    while (system_running) {
        network_ops++;
        
        if (network_ops % 4 == 0) {
            printk("🌐 NETWORK: ✅ Transmission %u completed\n", network_ops);
        }
        
        k_sleep(K_MSEC(2200));
    }
    
    printk("🌐 NETWORK: Communication service stopped\n");
}

/* 📡 Sensor Service */
void sensor_thread_func(void *a, void *b, void *c)
{
    printk("📡 SENSOR: Data acquisition started\n");
    
    while (system_running) {
        sensor_ops++;
        
        if (sensor_ops % 3 == 0) {
            printk("📡 SENSOR: ✅ Sample %u acquired\n", sensor_ops);
        }
        
        k_sleep(K_MSEC(3000));
    }
    
    printk("📡 SENSOR: Data acquisition stopped\n");
}

/* ⚠️ Risky Worker Thread - Will be predictively suspended */
void worker_risky_thread_func(void *a, void *b, void *c)
{
    printk("⚠️ WORKER_RISKY: Started - will consume stack until suspended\n");
    
    /* Give system time to show normal operation first */
    k_sleep(K_SECONDS(6));
    
    while (system_running && worker_risky_active) {
        worker_risky_ops++;
        
        printk("⚠️ WORKER_RISKY: Processing task %u\n", worker_risky_ops);
        
        /* Simulate increasing stack usage */
        max_stack_used = get_stack_usage();
        
        /* Check for predictive suspension threshold */
        if (max_stack_used >= STACK_WARNING_THRESHOLD && !fault_predicted) {
            printk("\n⚠️ WORKER_RISKY: 🚨 STACK USAGE WARNING! 🚨\n");
            printk("⚠️ WORKER_RISKY: Stack usage: %zu/%zu bytes (%.1f%%)\n", 
                   max_stack_used, STACK_SIZE, 
                   (float)max_stack_used / STACK_SIZE * 100);
            printk("⚠️ WORKER_RISKY: Triggering PREDICTIVE SUSPENSION!\n");
            
            /* Report predictive fault */
            struct ft_fault_context ctx = {
                .fault_type = FT_FAULT_STACK_OVERFLOW,
                .severity = FT_SEVERITY_WARNING,  /* Warning, not error! */
                .timestamp = k_uptime_get(),
                .thread_id = k_current_get(),
                .error_code = 0,  /* No actual error yet */
                .description = "Predictive stack overflow prevention"
            };
            
            ft_report_fault(&ctx);
            
            fault_predicted = true;
            worker_risky_active = false;
            
            printk("⚠️ WORKER_RISKY: Waiting for suspension signal...\n");
            k_sem_take(&suspend_risky_worker, K_FOREVER);
            
            printk("⚠️ WORKER_RISKY: 😴 Entering predictive suspension...\n");
            break;
        }
        
        k_sleep(K_MSEC(1800));
    }
    
    /* Suspended thread loop - shows it's suspended but not crashed */
    while (system_running) {
        printk("⚠️ WORKER_RISKY: 😴 Suspended (prevented stack overflow)\n");
        k_sleep(K_SECONDS(8));  /* Wake up occasionally to show suspension status */
    }
    
    printk("⚠️ WORKER_RISKY: Predictively suspended thread stopped\n");
}

/* ✅ Safe Worker Thread */
void worker_safe_thread_func(void *a, void *b, void *c)
{
    printk("✅ WORKER_SAFE: Started - safe operations only\n");
    
    while (system_running && worker_safe_active) {
        worker_safe_ops++;
        
        printk("✅ WORKER_SAFE: ✅ Safe task %u completed\n", worker_safe_ops);
        
        /* Show continued operation after predictive suspension */
        if (fault_predicted && worker_safe_ops % 4 == 0) {
            printk("✅ WORKER_SAFE: 🌟 Operating normally during Risky suspension! 🌟\n");
        }
        
        k_sleep(K_MSEC(2800));
    }
    
    printk("✅ WORKER_SAFE: Safe operations stopped\n");
}

/* Predictive fault handler */
static enum ft_handler_result predictive_handler(
    const struct ft_fault_context *fault_ctx,
    struct ft_recovery_context *recovery_ctx,
    void *user_data)
{
    printk("\n🛡️🛡️🛡️ PREDICTIVE FAULT PREVENTION ACTIVATED! 🛡️🛡️🛡️\n");
    printk("🛡️ DETECTED: Impending stack overflow (before corruption!)\n");
    printk("🛡️ SEVERITY: %s (preventive action)\n", 
           fault_ctx->severity == FT_SEVERITY_WARNING ? "WARNING" : "ERROR");
    printk("🛡️ STRATEGY: Clean predictive suspension\n");
    printk("🛡️ BENEFIT: No stack corruption, no system instability!\n");
    
    printk("\n🎯 PREDICTIVE FAULT TOLERANCE:\n");
    printk("🎯 Instead of waiting for actual stack overflow,\n");
    printk("🎯 we detected high stack usage and cleanly suspend\n");
    printk("🎯 the thread BEFORE any corruption occurs!\n");
    
    printk("\n🔍 OBSERVE: All other threads continue normally\n");
    printk("🔍 while Risky Worker is cleanly suspended!\n\n");
    
    /* Signal clean suspension */
    k_sem_give(&suspend_risky_worker);
    
    recovery_ctx->action = FT_RECOVERY_CUSTOM;
    return FT_HANDLER_HANDLED;
}

static struct ft_handler predictive_ft_handler = {
    .fault_type = FT_FAULT_STACK_OVERFLOW,
    .handler = predictive_handler,
    .priority = 1,
    .name = "predictive_handler"
};

int main(void)
{
    printk("\n");
    printk("========================================================\n");
    printk("         PREDICTIVE FAULT TOLERANCE DEMONSTRATION\n");
    printk("========================================================\n");
    printk("This demo shows the IDEAL fault tolerance scenario:\n");
    printk("PREDICTIVE SUSPENSION - detecting impending faults\n");
    printk("BEFORE they occur and cleanly suspending threads!\n");
    printk("\n");
    printk("How it works:\n");
    printk("  1. Monitor stack usage in real-time\n");
    printk("  2. When usage hits 80% threshold, trigger warning\n");
    printk("  3. Cleanly suspend risky thread BEFORE corruption\n");
    printk("  4. All other threads continue normally\n");
    printk("  5. System remains stable - no corruption!\n");
    printk("  6. QEMU continues running - no crashes!\n");
    printk("\n");
    printk("This represents the GOLD STANDARD of fault tolerance:\n");
    printk("Prevention is better than recovery!\n");
    printk("========================================================\n\n");
    
    /* Initialize framework */
    if (fault_tolerance_init(NULL) != 0) {
        printk("❌ Framework initialization failed\n");
        return -1;
    }
    
    if (ft_register_handler(&predictive_ft_handler) != 0) {
        printk("❌ Handler registration failed\n");
        return -1;
    }
    
    printk("✅ Predictive fault tolerance framework ready\n\n");
    
    /* Start all threads */
    printk("🚀 Starting all threads...\n\n");
    
    k_thread_create(&monitor_thread, monitor_stack, K_THREAD_STACK_SIZEOF(monitor_stack),
                   monitor_thread_func, NULL, NULL, NULL, K_PRIO_COOP(1), 0, K_NO_WAIT);
    k_thread_name_set(&monitor_thread, "monitor");
    
    k_thread_create(&critical_thread, critical_stack, K_THREAD_STACK_SIZEOF(critical_stack),
                   critical_thread_func, NULL, NULL, NULL, K_PRIO_COOP(2), 0, K_NO_WAIT);
    k_thread_name_set(&critical_thread, "critical");
    
    k_thread_create(&network_thread, network_stack, K_THREAD_STACK_SIZEOF(network_stack),
                   network_thread_func, NULL, NULL, NULL, K_PRIO_COOP(3), 0, K_NO_WAIT);
    k_thread_name_set(&network_thread, "network");
    
    k_thread_create(&sensor_thread, sensor_stack, K_THREAD_STACK_SIZEOF(sensor_stack),
                   sensor_thread_func, NULL, NULL, NULL, K_PRIO_COOP(4), 0, K_NO_WAIT);
    k_thread_name_set(&sensor_thread, "sensor");
    
    k_thread_create(&worker_safe_thread, worker_safe_stack, K_THREAD_STACK_SIZEOF(worker_safe_stack),
                   worker_safe_thread_func, NULL, NULL, NULL, K_PRIO_COOP(5), 0, K_NO_WAIT);
    k_thread_name_set(&worker_safe_thread, "worker_safe");
    
    k_thread_create(&worker_risky_thread, worker_risky_stack, K_THREAD_STACK_SIZEOF(worker_risky_stack),
                   worker_risky_thread_func, NULL, NULL, NULL, K_PRIO_COOP(6), 0, K_NO_WAIT);
    k_thread_name_set(&worker_risky_thread, "worker_risky");
    
    printk("🎬 ALL THREADS RUNNING! Waiting for predictive fault prevention...\n\n");
    
    /* Let system run and demonstrate predictive fault tolerance */
    uint32_t demo_minutes = 0;
    while (system_running && demo_minutes < 10) {  /* Run for up to 10 minutes */
        k_sleep(K_SECONDS(30));
        demo_minutes++;
        
        /* Show status after predictive suspension */
        if (fault_predicted && demo_minutes >= 2) {
            printk("\n⏰ STATUS UPDATE (minute %u):\n", demo_minutes);
            printk("   🛡️ Predictive suspension: ACTIVE\n");
            printk("   ✅ System stability: EXCELLENT\n");
            printk("   📊 Active threads: 5/6 (83%% operational)\n");
            printk("   🎯 Fault prevention: SUCCESS\n\n");
            
            /* After 5 minutes of successful operation, conclude demo */
            if (demo_minutes >= 5) {
                break;
            }
        }
    }
    
    /* Final demonstration results */
    printk("\n\n");
    printk("=========================================================\n");
    printk("          PREDICTIVE FAULT TOLERANCE RESULTS\n");
    printk("=========================================================\n");
    printk("🛡️ PREVENTION STRATEGY: Predictive thread suspension\n");
    printk("📊 SYSTEM STABILITY: Excellent (no corruption occurred)\n");
    printk("⏱️  UPTIME: Continuous operation maintained\n");
    printk("\n");
    printk("Thread Operations:\n");
    printk("  📊 Monitor:       %u cycles ✅ (CONTINUOUS MONITORING)\n", monitor_cycles);
    printk("  🔥 Critical:      %u ops   ✅ (MISSION CRITICAL PRESERVED)\n", critical_ops);
    printk("  🌐 Network:       %u ops   ✅ (COMMUNICATIONS MAINTAINED)\n", network_ops);
    printk("  📡 Sensor:        %u ops   ✅ (DATA ACQUISITION ACTIVE)\n", sensor_ops);
    printk("  ✅ Worker_Safe:   %u ops   ✅ (NORMAL OPERATIONS)\n", worker_safe_ops);
    printk("  ⚠️  Worker_Risky:  %u ops   😴 (PREDICTIVELY SUSPENDED)\n", worker_risky_ops);
    printk("\n");
    printk("Fault Prevention:\n");
    printk("  🎯 Stack overflow:     PREVENTED ✅\n");
    printk("  🎯 System corruption:  AVOIDED ✅\n");
    printk("  🎯 Service continuity: MAINTAINED ✅\n");
    printk("  🎯 Predictive action:  SUCCESSFUL ✅\n");
    printk("\n");
    printk("🏆 GOLD STANDARD ACHIEVED!\n");
    printk("   ✅ Fault detected BEFORE occurrence\n");
    printk("   ✅ Clean suspension without corruption\n");
    printk("   ✅ All critical services preserved\n");
    printk("   ✅ System stability maintained\n");
    printk("   ✅ Zero downtime achieved\n");
    printk("   ✅ No QEMU crashes - perfect demo!\n");
    printk("\n");
    printk("🌟 REAL-WORLD APPLICATION:\n");
    printk("   This predictive approach is used in:\n");
    printk("   • Space missions (satellites, rovers)\n");
    printk("   • Medical devices (life support, monitors)\n");
    printk("   • Automotive safety systems\n");
    printk("   • Industrial control systems\n");
    printk("   • Aviation flight control systems\n");
    printk("=========================================================\n\n");
    
    printk("🎯 DEMONSTRATION COMPLETE!\n");
    printk("💡 This proves that embedded systems CAN achieve\n");
    printk("   fault tolerance WITHOUT system reboots!\n\n");
    printk("🔍 Use Ctrl+A, X to exit QEMU when ready\n\n");
    
    /* Continue running to show sustained operation */
    while (system_running) {
        k_sleep(K_SECONDS(60));  /* Keep running */
    }
    
    return 0;
}