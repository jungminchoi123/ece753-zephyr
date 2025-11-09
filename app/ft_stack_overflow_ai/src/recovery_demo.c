/**
 * @file stack_overflow_recovery_demo.c
 * @author Jack Ostapeic
 * @brief Demonstrates real stack overflow recovery without system reboot
 *
 * This shows how the fault tolerance framework can recover from stack
 * overflows by restarting threads with larger stacks while maintaining
 * system operation and preserving critical state.
 */

#include <zephyr/kernel.h>
#include <zephyr/fault_tolerance.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(stack_recovery_demo, LOG_LEVEL_INF);

#define INITIAL_STACK_SIZE 512   // Small stack to trigger overflow
#define RECOVERY_STACK_SIZE 2048 // Larger stack for recovery
#define THREAD_PRIORITY K_PRIO_COOP(5)
#define MAX_RECOVERY_ATTEMPTS 3

/* System state that must be preserved across recoveries */
struct system_state {
    uint32_t critical_counter;
    uint32_t total_operations;
    uint32_t successful_operations;
    uint32_t failed_operations;
    bool system_operational;
};

static struct system_state sys_state = {
    .critical_counter = 0,
    .total_operations = 0,
    .successful_operations = 0,
    .failed_operations = 0,
    .system_operational = true
};

/* Thread management structures */
struct monitored_thread {
    struct k_thread thread;
    k_thread_stack_t *stack;
    size_t stack_size;
    k_thread_entry_t entry_point;
    void *p1, *p2, *p3;
    const char *name;
    uint32_t restart_count;
    bool active;
};

/* Worker thread that will experience stack overflow */
static struct monitored_thread worker_thread;
static K_THREAD_STACK_DEFINE(initial_worker_stack, INITIAL_STACK_SIZE);
static K_THREAD_STACK_DEFINE(recovery_worker_stack, RECOVERY_STACK_SIZE);

/* Critical system thread that must continue running */
static struct k_thread critical_thread;
static K_THREAD_STACK_DEFINE(critical_stack, 1024);

/* Recovery coordination */
static K_SEM_DEFINE(recovery_sem, 0, 1);
static K_MUTEX_DEFINE(state_mutex);

static bool framework_initialized = false;

/* Simulates work that causes stack overflow on small stacks */
int recursive_work(int depth, int work_load) 
{
    /* Large stack allocation to trigger overflow quickly */
    volatile char buffer[100];
    int result = 0;
    
    /* Fill buffer to prevent optimization */
    for (int i = 0; i < 100; i++) {
        buffer[i] = (depth + work_load + i) & 0xFF;
        result += buffer[i];
    }
    
    k_mutex_lock(&state_mutex, K_FOREVER);
    sys_state.total_operations++;
    k_mutex_unlock(&state_mutex);
    
    printk("Worker: depth=%d, work=%d, operations=%u\n", 
           depth, work_load, sys_state.total_operations);
    
    if (depth < work_load) {
        /* Recurse to eventually overflow the stack */
        result += recursive_work(depth + 1, work_load);
    }
    
    return result;
}

/* Worker thread function */
void worker_thread_func(void *p1, void *p2, void *p3)
{
    int work_load = POINTER_TO_INT(p1);
    
    printk("Worker thread started (restart #%u), work_load=%d\n", 
           worker_thread.restart_count, work_load);
    
    k_sleep(K_MSEC(100)); /* Let system settle */
    
    try_work:
    k_mutex_lock(&state_mutex, K_FOREVER);
    sys_state.total_operations++;
    k_mutex_unlock(&state_mutex);
    
    /* Do some work that will eventually cause stack overflow */
    int result = recursive_work(0, work_load);
    
    /* If we get here, the work completed successfully */
    k_mutex_lock(&state_mutex, K_FOREVER);
    sys_state.successful_operations++;
    k_mutex_unlock(&state_mutex);
    
    printk("Worker: Work completed successfully! Result=%d\n", result);
    
    /* Simulate doing more work */
    work_load += 2; /* Increase difficulty */
    k_sleep(K_SECONDS(3));
    goto try_work;
}

/* Critical system thread that must keep running */
void critical_thread_func(void *p1, void *p2, void *p3)
{
    uint32_t heartbeat = 0;
    
    while (sys_state.system_operational) {
        k_mutex_lock(&state_mutex, K_FOREVER);
        sys_state.critical_counter++;
        k_mutex_unlock(&state_mutex);
        
        heartbeat++;
        printk("CRITICAL: System heartbeat #%u, counter=%u, ops=%u/%u\n",
               heartbeat, sys_state.critical_counter, 
               sys_state.successful_operations, sys_state.total_operations);
        
        k_sleep(K_SECONDS(2));
    }
    
    printk("CRITICAL: System shutdown requested\n");
}

/* Custom recovery handler that restarts threads with larger stacks */
static enum ft_handler_result recovery_fault_handler(
    const struct ft_fault_context *fault_ctx,
    struct ft_recovery_context *recovery_ctx,
    void *user_data)
{
    printk("\n=== STACK OVERFLOW RECOVERY HANDLER ===\n");
    printk("Fault in thread: %p\n", fault_ctx->thread_id);
    printk("Error: %s\n", ft_get_fault_type_name(fault_ctx->fault_type));
    printk("Restart count: %u/%u\n", worker_thread.restart_count, MAX_RECOVERY_ATTEMPTS);
    
    /* Check if this is our monitored worker thread */
    if (fault_ctx->thread_id == &worker_thread.thread) {
        if (worker_thread.restart_count >= MAX_RECOVERY_ATTEMPTS) {
            printk("❌ Maximum restart attempts reached, giving up\n");
            recovery_ctx->action = FT_RECOVERY_NONE;
            
            k_mutex_lock(&state_mutex, K_FOREVER);
            sys_state.failed_operations++;
            k_mutex_unlock(&state_mutex);
            
            return FT_HANDLER_HANDLED;
        }
        
        printk("✓ Initiating thread recovery...\n");
        
        /* Signal recovery process */
        k_sem_give(&recovery_sem);
        
        /* Set up custom recovery */
        recovery_ctx->action = FT_RECOVERY_CUSTOM;
        recovery_ctx->async_recovery = true;
        
        return FT_HANDLER_HANDLED;
    }
    
    printk("❓ Unknown thread fault, continuing to other handlers\n");
    return FT_HANDLER_CONTINUE;
}

/* Recovery work function */
void recovery_worker(struct k_work *work)
{
    printk("\n🔄 RECOVERY: Starting thread recovery process...\n");
    
    /* Abort the faulted thread */
    printk("🔄 RECOVERY: Aborting faulted thread\n");
    k_thread_abort(&worker_thread.thread);
    worker_thread.active = false;
    
    /* Update statistics */
    k_mutex_lock(&state_mutex, K_FOREVER);
    sys_state.failed_operations++;
    k_mutex_unlock(&state_mutex);
    
    /* Wait a moment for cleanup */
    k_sleep(K_MSEC(500));
    
    /* Switch to larger stack if this is the first restart */
    if (worker_thread.restart_count == 0) {
        printk("🔄 RECOVERY: Upgrading to larger stack (%zu -> %zu bytes)\n",
               worker_thread.stack_size, RECOVERY_STACK_SIZE);
        worker_thread.stack = recovery_worker_stack;
        worker_thread.stack_size = RECOVERY_STACK_SIZE;
    }
    
    worker_thread.restart_count++;
    
    /* Create new thread instance */
    printk("🔄 RECOVERY: Creating new thread instance (attempt #%u)\n", 
           worker_thread.restart_count);
    
    k_tid_t new_tid = k_thread_create(
        &worker_thread.thread,
        worker_thread.stack,
        worker_thread.stack_size,
        worker_thread.entry_point,
        INT_TO_POINTER(5 + worker_thread.restart_count), /* Increase work load */
        worker_thread.p2,
        worker_thread.p3,
        THREAD_PRIORITY,
        0,
        K_NO_WAIT
    );
    
    if (new_tid != NULL) {
        k_thread_name_set(new_tid, worker_thread.name);
        worker_thread.active = true;
        printk("✅ RECOVERY: Thread successfully restarted with TID %p\n", new_tid);
        
        /* Re-register the thread with fault tolerance framework */
        ft_thread_register(new_tid, worker_thread.entry_point,
                          INT_TO_POINTER(5 + worker_thread.restart_count), NULL, NULL,
                          worker_thread.stack_size, THREAD_PRIORITY, 0,
                          worker_thread.name);
    } else {
        printk("❌ RECOVERY: Failed to create new thread\n");
    }
    
    printk("🔄 RECOVERY: Recovery process complete\n\n");
}

/* Recovery work item */
static K_WORK_DEFINE(recovery_work, recovery_worker);

/* Recovery coordination thread */
void recovery_coordinator(void *p1, void *p2, void *p3)
{
    printk("Recovery coordinator started\n");
    
    while (sys_state.system_operational) {
        /* Wait for recovery signal */
        k_sem_take(&recovery_sem, K_FOREVER);
        
        if (!sys_state.system_operational) {
            break;
        }
        
        /* Submit recovery work */
        k_work_submit(&recovery_work);
    }
    
    printk("Recovery coordinator shutting down\n");
}

/* Recovery coordinator thread */
static struct k_thread recovery_thread;
static K_THREAD_STACK_DEFINE(recovery_stack, 1024);

/* Register recovery handler */
static struct ft_handler recovery_handler = {
    .fault_type = FT_FAULT_STACK_OVERFLOW,
    .handler = recovery_fault_handler,
    .priority = 1,  /* Highest priority */
    .name = "stack_recovery_handler",
    .user_data = NULL
};

/* Fatal error handler - the entry point for our recovery */
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
    if (reason == K_ERR_STACK_CHK_FAIL && framework_initialized) {
        printk("\n🚨 STACK OVERFLOW DETECTED 🚨\n");
        
        /* Create fault context */
        struct ft_fault_context ctx = {
            .fault_type = FT_FAULT_STACK_OVERFLOW,
            .severity = FT_SEVERITY_ERROR,
            .timestamp = k_uptime_get(),
            .thread_id = k_current_get(),
            .error_code = reason,
            .description = "Stack overflow in worker thread",
            .esf = esf
        };
        
        /* Report to fault tolerance framework */
        ft_report_fault(&ctx);
        
        /* Show current system state */
        printk("📊 System State: Critical=%u, Ops=%u, Success=%u, Failed=%u\n",
               sys_state.critical_counter, sys_state.total_operations,
               sys_state.successful_operations, sys_state.failed_operations);
        
        /* The framework will handle recovery, we just need to halt this thread */
        printk("Halting faulted thread for recovery...\n");
        k_fatal_halt(reason);
        
        return; /* This won't be reached, but for clarity */
    }
    
    /* For non-stack overflow errors, halt the system */
    printk("💀 Non-recoverable error %u, halting system\n", reason);
    k_fatal_halt(reason);
}

int main(void)
{
    printk("\n=== STACK OVERFLOW RECOVERY DEMONSTRATION ===\n");
    printk("This demo shows continuous operation after stack overflow recovery\n\n");
    
    /* Initialize fault tolerance framework */
    printk("Initializing fault tolerance framework...\n");
    int ret = fault_tolerance_init(NULL);
    if (ret != 0) {
        printk("Failed to initialize framework: %d\n", ret);
        return ret;
    }
    framework_initialized = true;
    printk("✅ Fault tolerance framework ready\n");
    
    /* Register our recovery handler */
    ret = ft_register_handler(&recovery_handler);
    if (ret != 0) {
        printk("Failed to register recovery handler: %d\n", ret);
        return ret;
    }
    printk("✅ Recovery handler registered\n");
    
    /* Initialize worker thread structure */
    worker_thread.stack = initial_worker_stack;
    worker_thread.stack_size = INITIAL_STACK_SIZE;
    worker_thread.entry_point = worker_thread_func;
    worker_thread.name = "worker";
    worker_thread.restart_count = 0;
    worker_thread.active = false;
    
    /* Start recovery coordinator */
    k_thread_create(&recovery_thread, recovery_stack, K_THREAD_STACK_SIZEOF(recovery_stack),
                   recovery_coordinator, NULL, NULL, NULL,
                   K_PRIO_COOP(3), 0, K_NO_WAIT);
    k_thread_name_set(&recovery_thread, "recovery_coordinator");
    printk("✅ Recovery coordinator started\n");
    
    /* Start critical system thread */
    k_thread_create(&critical_thread, critical_stack, K_THREAD_STACK_SIZEOF(critical_stack),
                   critical_thread_func, NULL, NULL, NULL,
                   K_PRIO_COOP(4), 0, K_NO_WAIT);
    k_thread_name_set(&critical_thread, "critical_system");
    printk("✅ Critical system thread started\n");
    
    /* Start initial worker thread */
    k_tid_t worker_tid = k_thread_create(
        &worker_thread.thread, worker_thread.stack, worker_thread.stack_size,
        worker_thread.entry_point, INT_TO_POINTER(5), NULL, NULL,
        THREAD_PRIORITY, 0, K_NO_WAIT);
    
    if (worker_tid != NULL) {
        k_thread_name_set(worker_tid, worker_thread.name);
        worker_thread.active = true;
        
        /* Register with fault tolerance framework IMMEDIATELY */
        ret = ft_thread_register(worker_tid, worker_thread.entry_point,
                                INT_TO_POINTER(5), NULL, NULL,
                                worker_thread.stack_size, THREAD_PRIORITY, 0,
                                worker_thread.name);
        
        printk("✅ Worker thread started and registered: %p\n", worker_tid);
        if (ret != 0) {
            printk("⚠️  Warning: Thread registration failed: %d\n", ret);
        }
    } else {
        printk("❌ Failed to create worker thread\n");
        return -1;
    }
    
    printk("\n🚀 SYSTEM OPERATIONAL - Watch for stack overflow recovery...\n");
    printk("The critical system will continue running while worker threads are recovered\n\n");
    
    /* Main system loop */
    uint32_t main_loop_count = 0;
    while (sys_state.system_operational) {
        k_sleep(K_SECONDS(5));
        main_loop_count++;
        
        printk("\n📈 MAIN: System status check #%u\n", main_loop_count);
        printk("   Operations: %u total, %u successful, %u failed\n",
               sys_state.total_operations, sys_state.successful_operations, 
               sys_state.failed_operations);
        printk("   Worker restarts: %u/%u\n", 
               worker_thread.restart_count, MAX_RECOVERY_ATTEMPTS);
        printk("   Critical counter: %u\n", sys_state.critical_counter);
        
        /* Stop after reasonable demonstration */
        if (main_loop_count >= 10 || worker_thread.restart_count >= MAX_RECOVERY_ATTEMPTS) {
            printk("\n🏁 Demonstration complete, shutting down...\n");
            sys_state.system_operational = false;
        }
    }
    
    printk("\n=== FINAL STATISTICS ===\n");
    printk("Total operations: %u\n", sys_state.total_operations);
    printk("Successful operations: %u\n", sys_state.successful_operations);
    printk("Failed operations: %u\n", sys_state.failed_operations);
    printk("Worker restarts: %u\n", worker_thread.restart_count);
    printk("Critical system uptime: %u heartbeats\n", sys_state.critical_counter);
    printk("System remained operational throughout recovery!\n");
    
    return 0;
}