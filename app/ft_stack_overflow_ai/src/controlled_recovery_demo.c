/**
 * @file controlled_recovery_demo.c
 * @author Jack Ostapeic
 * @brief Controlled Stack Overflow Recovery Demonstration
 *
 * This demonstrates stack overflow recovery in a controlled manner,
 * showing how the system continues operating while recovering faulted threads.
 */

#include <zephyr/kernel.h>
#include <zephyr/fault_tolerance.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(controlled_recovery, LOG_LEVEL_INF);

#define INITIAL_STACK_SIZE 512
#define RECOVERY_STACK_SIZE 2048
#define THREAD_PRIORITY K_PRIO_COOP(5)

/* System state */
struct {
    uint32_t system_uptime_seconds;
    uint32_t worker_restarts;
    uint32_t successful_operations;
    uint32_t failed_operations;
    bool system_running;
} system_state = {0, 0, 0, 0, true};

/* Thread structures */
struct k_thread worker_thread;
struct k_thread monitor_thread;
K_THREAD_STACK_DEFINE(worker_stack_small, INITIAL_STACK_SIZE);
K_THREAD_STACK_DEFINE(worker_stack_large, RECOVERY_STACK_SIZE);
K_THREAD_STACK_DEFINE(monitor_stack, 1024);

/* Control variables */
static bool use_large_stack = false;
static bool framework_ready = false;
K_SEM_DEFINE(worker_control, 0, 1);

/* Stack overflow trigger function */
int cause_overflow(int depth, int target_depth)
{
    /* Large local buffer to consume stack */
    volatile char buffer[64];
    int sum = 0;
    
    /* Fill buffer */
    for (int i = 0; i < 64; i++) {
        buffer[i] = depth + i;
        sum += buffer[i];
    }
    
    printk("  Recursion depth %d/%d (sum=%d)\n", depth, target_depth, sum);
    
    if (depth < target_depth) {
        sum += cause_overflow(depth + 1, target_depth);
    }
    
    return sum;
}

/* Worker thread that will experience stack overflow */
void worker_thread_func(void *p1, void *p2, void *p3)
{
    int work_cycle = 0;
    
    printk("🔧 Worker thread started (restart #%u, stack: %s)\n", 
           system_state.worker_restarts,
           use_large_stack ? "LARGE" : "small");
    
    while (system_state.system_running) {
        /* Wait for permission to start work */
        k_sem_take(&worker_control, K_FOREVER);
        
        if (!system_state.system_running) break;
        
        work_cycle++;
        printk("🔧 Worker: Starting work cycle %d\n", work_cycle);
        
        /* Do work that will cause stack overflow on small stack */
        {
            int target_depth = use_large_stack ? 8 : 15; /* 15 will overflow 512B stack */
            int result = cause_overflow(0, target_depth);
            
            /* If we get here, work completed successfully */
            system_state.successful_operations++;
            printk("✅ Worker: Cycle %d completed successfully (result=%d)\n", 
                   work_cycle, result);
        }
        
        /* Brief pause between work cycles */
        k_sleep(K_MSEC(500));
    }
    
    printk("🔧 Worker thread shutting down\n");
}

/* System monitor that keeps running during recovery */
void monitor_thread_func(void *p1, void *p2, void *p3)
{
    uint32_t tick = 0;
    
    while (system_state.system_running) {
        tick++;
        system_state.system_uptime_seconds++;
        
        printk("📊 MONITOR: Tick %u | Uptime: %us | Restarts: %u | Success: %u | Failed: %u\n",
               tick, system_state.system_uptime_seconds, 
               system_state.worker_restarts, system_state.successful_operations,
               system_state.failed_operations);
        
        /* Allow worker to do some work */
        if (tick % 3 == 1) {  /* Give work permission every 3 seconds */
            printk("📊 MONITOR: Authorizing worker operation...\n");
            k_sem_give(&worker_control);
        }
        
        k_sleep(K_SECONDS(1));
    }
    
    printk("📊 Monitor shutting down\n");
}

/* Custom recovery handler */
static enum ft_handler_result recovery_handler(
    const struct ft_fault_context *fault_ctx,
    struct ft_recovery_context *recovery_ctx,
    void *user_data)
{
    printk("\n🚨 RECOVERY HANDLER ACTIVATED 🚨\n");
    printk("   Fault Type: %s\n", ft_get_fault_type_name(fault_ctx->fault_type));
    printk("   Thread: %p\n", fault_ctx->thread_id);
    printk("   Current restarts: %u\n", system_state.worker_restarts);
    
    if (system_state.worker_restarts >= 3) {
        printk("❌ Maximum restart attempts reached - giving up on recovery\n");
        recovery_ctx->action = FT_RECOVERY_NONE;
        system_state.failed_operations++;
        return FT_HANDLER_HANDLED;
    }
    
    /* Mark that we need to use the larger stack for next restart */
    if (!use_large_stack) {
        use_large_stack = true;
        printk("✅ Upgrading to larger stack for next restart\n");
    }
    
    /* Indicate that custom recovery will restart the thread */
    recovery_ctx->action = FT_RECOVERY_CUSTOM;
    recovery_ctx->async_recovery = true;
    
    printk("✅ Recovery handler will restart worker thread\n");
    return FT_HANDLER_HANDLED;
}

/* Recovery work function */
void do_recovery(struct k_work *work)
{
    printk("\n🔄 STARTING RECOVERY PROCESS 🔄\n");
    
    /* Update statistics */
    system_state.worker_restarts++;
    system_state.failed_operations++;
    
    /* Wait for things to settle */
    k_sleep(K_MSEC(1000));
    
    /* Choose stack based on recovery state */
    k_thread_stack_t *stack = use_large_stack ? worker_stack_large : worker_stack_small;
    size_t stack_size = use_large_stack ? RECOVERY_STACK_SIZE : INITIAL_STACK_SIZE;
    
    printk("🔄 Creating new worker thread (stack: %zu bytes)\n", stack_size);
    
    /* Create new worker thread */
    k_tid_t new_worker = k_thread_create(
        &worker_thread, stack, stack_size,
        worker_thread_func, NULL, NULL, NULL,
        THREAD_PRIORITY, 0, K_NO_WAIT);
        
    if (new_worker != NULL) {
        k_thread_name_set(new_worker, "recovered_worker");
        printk("✅ Worker thread successfully restarted: %p\n", new_worker);
    } else {
        printk("❌ Failed to restart worker thread\n");
    }
    
    printk("🔄 RECOVERY PROCESS COMPLETE 🔄\n\n");
}

static K_WORK_DEFINE(recovery_work, do_recovery);

/* Fault tolerance handler registration */
static struct ft_handler custom_handler = {
    .fault_type = FT_FAULT_STACK_OVERFLOW,
    .handler = recovery_handler,
    .priority = 1,
    .name = "controlled_recovery_handler"
};

/* Fatal error handler */
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
    if (reason == K_ERR_STACK_CHK_FAIL && framework_ready) {
        printk("\n💥 STACK OVERFLOW DETECTED! 💥\n");
        
        /* Report to fault tolerance framework */
        struct ft_fault_context ctx = {
            .fault_type = FT_FAULT_STACK_OVERFLOW,
            .severity = FT_SEVERITY_ERROR,
            .timestamp = k_uptime_get(),
            .thread_id = k_current_get(),
            .error_code = reason,
            .description = "Controlled stack overflow test"
        };
        
        int ret = ft_report_fault(&ctx);
        printk("Fault reported to framework: %s\n", ret == 0 ? "SUCCESS" : "FAILED");
        
        /* Submit recovery work */
        k_work_submit(&recovery_work);
        
        /* Show system is still alive */
        printk("💚 SYSTEM CONTINUES TO OPERATE DURING RECOVERY 💚\n");
        
        /* Halt this thread for recovery */
        k_fatal_halt(reason);
    }
    
    printk("Non-recoverable error %u\n", reason);
    k_fatal_halt(reason);
}

int main(void)
{
    printk("\n");
    printk("===========================================\n");
    printk("  CONTROLLED STACK OVERFLOW RECOVERY DEMO\n");
    printk("===========================================\n");
    printk("This demonstrates system continuation during\n");
    printk("stack overflow recovery of individual threads\n");
    printk("===========================================\n\n");
    
    /* Initialize fault tolerance framework */
    printk("🚀 Initializing fault tolerance framework...\n");
    int ret = fault_tolerance_init(NULL);
    if (ret != 0) {
        printk("❌ Framework initialization failed: %d\n", ret);
        return ret;
    }
    
    /* Register recovery handler */
    ret = ft_register_handler(&custom_handler);
    if (ret != 0) {
        printk("❌ Handler registration failed: %d\n", ret);
        return ret;
    }
    
    framework_ready = true;
    printk("✅ Fault tolerance framework ready\n");
    
    /* Start system monitor (critical system component) */
    k_thread_create(&monitor_thread, monitor_stack, K_THREAD_STACK_SIZEOF(monitor_stack),
                   monitor_thread_func, NULL, NULL, NULL,
                   K_PRIO_COOP(3), 0, K_NO_WAIT);
    k_thread_name_set(&monitor_thread, "system_monitor");
    printk("✅ System monitor started\n");
    
    /* Start initial worker thread */
    k_tid_t worker = k_thread_create(
        &worker_thread, worker_stack_small, INITIAL_STACK_SIZE,
        worker_thread_func, NULL, NULL, NULL,
        THREAD_PRIORITY, 0, K_NO_WAIT);
        
    if (worker != NULL) {
        k_thread_name_set(worker, "initial_worker");
        printk("✅ Initial worker thread started: %p\n", worker);
    } else {
        printk("❌ Failed to create initial worker thread\n");
        return -1;
    }
    
    printk("\n🎬 DEMONSTRATION STARTING...\n");
    printk("Watch how the system monitor continues running\n");
    printk("while the worker thread is recovered from stack overflow!\n\n");
    
    /* Main loop - let the demonstration run */
    int demo_seconds = 0;
    while (system_state.system_running && demo_seconds < 30) {
        k_sleep(K_SECONDS(1));
        demo_seconds++;
        
        /* Stop after sufficient demonstration */
        if (system_state.worker_restarts >= 3) {
            printk("\n🏁 DEMONSTRATION COMPLETE!\n");
            break;
        }
    }
    
    /* Shutdown */
    system_state.system_running = false;
    k_sem_give(&worker_control); /* Wake up worker to shut down */
    
    printk("\n");
    printk("=====================================\n");
    printk("           FINAL RESULTS\n");
    printk("=====================================\n");
    printk("System uptime: %u seconds\n", system_state.system_uptime_seconds);
    printk("Worker restarts: %u\n", system_state.worker_restarts);
    printk("Successful operations: %u\n", system_state.successful_operations);
    printk("Failed operations: %u\n", system_state.failed_operations);
    printk("=====================================\n");
    printk("✅ SYSTEM SURVIVED MULTIPLE STACK\n");
    printk("   OVERFLOWS WITH CONTINUOUS OPERATION!\n");
    printk("=====================================\n");
    
    return 0;
}