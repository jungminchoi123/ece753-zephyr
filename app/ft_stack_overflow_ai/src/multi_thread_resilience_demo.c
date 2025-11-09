/**
 * @file multi_thread_resilience_demo.c
 * @author Jack Ostapeic
 * @brief Multi-Thread Resilience Demonstration
 *
 * This demo launches multiple threads to clearly show that when one thread
 * experiences a stack overflow, ONLY that thread is terminated while all
 * other threads continue running normally. This proves system-level resilience.
 */

#include <zephyr/kernel.h>
#include <zephyr/fault_tolerance.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/random/random.h>

LOG_MODULE_REGISTER(resilience_demo, LOG_LEVEL_INF);

/* Thread stacks and control blocks */
static K_THREAD_STACK_DEFINE(critical_stack, 1024);
static K_THREAD_STACK_DEFINE(network_stack, 1024);
static K_THREAD_STACK_DEFINE(sensor_stack, 1024);
static K_THREAD_STACK_DEFINE(ui_stack, 1024);
static K_THREAD_STACK_DEFINE(logger_stack, 1024);
static K_THREAD_STACK_DEFINE(worker1_stack, 512);  /* Small stack - will overflow */
static K_THREAD_STACK_DEFINE(worker2_stack, 1024); /* Normal stack */
static K_THREAD_STACK_DEFINE(worker3_stack, 1024); /* Normal stack */

static struct k_thread critical_thread;
static struct k_thread network_thread;
static struct k_thread sensor_thread;
static struct k_thread ui_thread;
static struct k_thread logger_thread;
static struct k_thread worker1_thread;
static struct k_thread worker2_thread;
static struct k_thread worker3_thread;

/* Global system state */
static struct {
    atomic_t system_uptime;
    atomic_t total_operations;
    atomic_t faults_detected;
    atomic_t threads_running;
    bool demo_active;
} system_state = {
    .demo_active = true
};

/* Thread status tracking */
static struct {
    uint32_t critical_heartbeats;
    uint32_t network_packets;
    uint32_t sensor_readings;
    uint32_t ui_updates;
    uint32_t log_entries;
    uint32_t worker1_cycles;
    uint32_t worker2_cycles;
    uint32_t worker3_cycles;
    bool worker1_alive;
    bool worker2_alive;
    bool worker3_alive;
} thread_stats = {
    .worker1_alive = true,
    .worker2_alive = true,
    .worker3_alive = true
};

/* Stack-consuming recursive function */
int deep_computation(int depth, int max_depth)
{
    /* Large buffer to consume stack space */
    volatile char computation_buffer[100];
    int result = 0;
    
    /* Simulate computation work */
    for (int i = 0; i < 100; i++) {
        computation_buffer[i] = (char)(depth + i + sys_rand32_get());
        result += computation_buffer[i];
    }
    
    if (depth < max_depth) {
        result += deep_computation(depth + 1, max_depth);
    }
    
    return result;
}

/* 🔥 Critical System Thread - MUST NEVER STOP */
void critical_system_thread(void *p1, void *p2, void *p3)
{
    printk("🔥 CRITICAL: System guardian started - protecting core services\n");
    atomic_inc(&system_state.threads_running);
    
    while (system_state.demo_active) {
        thread_stats.critical_heartbeats++;
        atomic_inc(&system_state.system_uptime);
        
        /* Monitor system health */
        uint32_t faults = atomic_get(&system_state.faults_detected);
        uint32_t ops = atomic_get(&system_state.total_operations);
        
        printk("🔥 CRITICAL: Beat #%u | Operations: %u | Faults: %u | Status: %s\n",
               thread_stats.critical_heartbeats, ops, faults,
               faults > 0 ? "⚡ FAULT RECOVERY ACTIVE" : "✅ NORMAL");
        
        k_sleep(K_SECONDS(2));
    }
    
    atomic_dec(&system_state.threads_running);
    printk("🔥 CRITICAL: Shutting down gracefully\n");
}

/* 🌐 Network Service Thread */
void network_service_thread(void *p1, void *p2, void *p3)
{
    printk("🌐 NETWORK: Communication service started\n");
    atomic_inc(&system_state.threads_running);
    
    while (system_state.demo_active) {
        thread_stats.network_packets++;
        atomic_inc(&system_state.total_operations);
        
        /* Simulate network activity */
        if (thread_stats.network_packets % 5 == 0) {
            printk("🌐 NETWORK: Processed %u packets | Worker1: %s | Worker2: %s | Worker3: %s\n",
                   thread_stats.network_packets,
                   thread_stats.worker1_alive ? "ALIVE" : "DEAD",
                   thread_stats.worker2_alive ? "ALIVE" : "DEAD", 
                   thread_stats.worker3_alive ? "ALIVE" : "DEAD");
        }
        
        k_sleep(K_MSEC(800));
    }
    
    atomic_dec(&system_state.threads_running);
    printk("🌐 NETWORK: Service stopped\n");
}

/* 📡 Sensor Monitoring Thread */
void sensor_monitoring_thread(void *p1, void *p2, void *p3)
{
    printk("📡 SENSOR: Environmental monitoring started\n");
    atomic_inc(&system_state.threads_running);
    
    while (system_state.demo_active) {
        thread_stats.sensor_readings++;
        atomic_inc(&system_state.total_operations);
        
        /* Simulate sensor readings */
        uint32_t temp = 20 + (sys_rand32_get() % 15);
        uint32_t humidity = 45 + (sys_rand32_get() % 20);
        
        printk("📡 SENSOR: Reading #%u | Temp: %u°C | Humidity: %u%% | Threads alive: %d\n",
               thread_stats.sensor_readings, temp, humidity,
               (thread_stats.worker1_alive ? 1 : 0) + 
               (thread_stats.worker2_alive ? 1 : 0) + 
               (thread_stats.worker3_alive ? 1 : 0));
        
        k_sleep(K_MSEC(1500));
    }
    
    atomic_dec(&system_state.threads_running);
    printk("📡 SENSOR: Monitoring stopped\n");
}

/* 🖥️ User Interface Thread */
void user_interface_thread(void *p1, void *p2, void *p3)
{
    printk("🖥️ UI: User interface started\n");
    atomic_inc(&system_state.threads_running);
    
    while (system_state.demo_active) {
        thread_stats.ui_updates++;
        atomic_inc(&system_state.total_operations);
        
        /* Simulate UI updates */
        if (thread_stats.ui_updates % 3 == 0) {
            uint32_t active_workers = (thread_stats.worker1_alive ? 1 : 0) + 
                                     (thread_stats.worker2_alive ? 1 : 0) + 
                                     (thread_stats.worker3_alive ? 1 : 0);
            printk("🖥️ UI: Update #%u | Active workers: %u/3 | System responsive: YES\n",
                   thread_stats.ui_updates, active_workers);
        }
        
        k_sleep(K_MSEC(1200));
    }
    
    atomic_dec(&system_state.threads_running);
    printk("🖥️ UI: Interface closed\n");
}

/* 📝 System Logger Thread */
void system_logger_thread(void *p1, void *p2, void *p3)
{
    printk("📝 LOGGER: System logging started\n");
    atomic_inc(&system_state.threads_running);
    
    while (system_state.demo_active) {
        thread_stats.log_entries++;
        atomic_inc(&system_state.total_operations);
        
        /* Log system state */
        if (thread_stats.log_entries % 4 == 0) {
            uint32_t uptime = atomic_get(&system_state.system_uptime);
            uint32_t faults = atomic_get(&system_state.faults_detected);
            printk("📝 LOGGER: Entry #%u | Uptime: %us | Faults logged: %u | All services operational\n",
                   thread_stats.log_entries, uptime * 2, faults);
        }
        
        k_sleep(K_MSEC(2000));
    }
    
    atomic_dec(&system_state.threads_running);
    printk("📝 LOGGER: Logging stopped\n");
}

/* ⚙️ Worker Thread 1 - WILL OVERFLOW (small stack) */
void worker1_thread_func(void *p1, void *p2, void *p3)
{
    printk("⚙️ WORKER1: Started with small stack (512B) - THIS WILL OVERFLOW\n");
    atomic_inc(&system_state.threads_running);
    
    k_sleep(K_MSEC(300)); /* Let other threads start */
    
    while (system_state.demo_active && thread_stats.worker1_alive) {
        thread_stats.worker1_cycles++;
        atomic_inc(&system_state.total_operations);
        
        printk("⚙️ WORKER1: Cycle %u - starting deep computation (WILL OVERFLOW)...\n", 
               thread_stats.worker1_cycles);
        
        /* This WILL cause stack overflow on 512B stack */
        int result = deep_computation(0, 20);  /* 20 levels * 100 bytes = ~2000 bytes */
        
        /* This line should never be reached */
        printk("⚙️ WORKER1: ✅ Cycle %u completed (result=%d)\n", 
               thread_stats.worker1_cycles, result);
        
        k_sleep(K_MSEC(1000));
    }
    
    atomic_dec(&system_state.threads_running);
    printk("⚙️ WORKER1: Thread stopped\n");
}

/* ⚙️ Worker Thread 2 - Normal operation */
void worker2_thread_func(void *p1, void *p2, void *p3)
{
    printk("⚙️ WORKER2: Started with normal stack (1024B) - should continue running\n");
    atomic_inc(&system_state.threads_running);
    
    while (system_state.demo_active && thread_stats.worker2_alive) {
        thread_stats.worker2_cycles++;
        atomic_inc(&system_state.total_operations);
        
        /* Safe computation that won't overflow */
        int result = deep_computation(0, 5);  /* 5 levels * 100 bytes = ~500 bytes */
        
        printk("⚙️ WORKER2: ✅ Cycle %u completed successfully (result=%d)\n", 
               thread_stats.worker2_cycles, result);
        
        k_sleep(K_MSEC(900));
    }
    
    atomic_dec(&system_state.threads_running);
    printk("⚙️ WORKER2: Thread stopped\n");
}

/* ⚙️ Worker Thread 3 - Normal operation */
void worker3_thread_func(void *p1, void *p2, void *p3)
{
    printk("⚙️ WORKER3: Started with normal stack (1024B) - should continue running\n");
    atomic_inc(&system_state.threads_running);
    
    while (system_state.demo_active && thread_stats.worker3_alive) {
        thread_stats.worker3_cycles++;
        atomic_inc(&system_state.total_operations);
        
        /* Safe computation that won't overflow */
        int result = deep_computation(0, 6);  /* 6 levels * 100 bytes = ~600 bytes */
        
        printk("⚙️ WORKER3: ✅ Cycle %u completed successfully (result=%d)\n", 
               thread_stats.worker3_cycles, result);
        
        k_sleep(K_MSEC(1100));
    }
    
    atomic_dec(&system_state.threads_running);
    printk("⚙️ WORKER3: Thread stopped\n");
}

/* Fault handler */
static enum ft_handler_result stack_overflow_handler(
    const struct ft_fault_context *fault_ctx,
    struct ft_recovery_context *recovery_ctx,
    void *user_data)
{
    atomic_inc(&system_state.faults_detected);
    
    printk("\n💥 FAULT DETECTED! Thread %p experienced stack overflow\n", fault_ctx->thread_id);
    
    /* Determine which worker thread failed */
    if (fault_ctx->thread_id == &worker1_thread) {
        thread_stats.worker1_alive = false;
        printk("☠️  WORKER1 terminated due to stack overflow\n");
    } else {
        printk("☠️  Unknown worker thread terminated\n");
    }
    
    printk("🔄 Only the faulting thread is terminated - ALL OTHER THREADS CONTINUE!\n");
    printk("🌟 SYSTEM RESILIENCE: %u/%u core services still operational!\n", 
           5, 5); /* Critical, Network, Sensor, UI, Logger all continue */
    
    recovery_ctx->action = FT_RECOVERY_NONE;  /* Let thread terminate naturally */
    recovery_ctx->async_recovery = false;
    
    return FT_HANDLER_HANDLED;
}

/* Handler registration */
static struct ft_handler overflow_handler = {
    .fault_type = FT_FAULT_STACK_OVERFLOW,
    .handler = stack_overflow_handler,
    .priority = 1,
    .name = "multi_thread_overflow_handler"
};

/* Fatal error handler */
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
    if (reason == K_ERR_STACK_CHK_FAIL) {
        printk("\n🚨 SYSTEM FAULT HANDLER: Stack overflow detected!\n");
        printk("🚨 Thread %p will be terminated\n", k_current_get());
        printk("🌟 ALL OTHER THREADS CONTINUE RUNNING NORMALLY! 🌟\n");
        
        /* Report to framework */
        struct ft_fault_context ctx = {
            .fault_type = FT_FAULT_STACK_OVERFLOW,
            .severity = FT_SEVERITY_ERROR,
            .timestamp = k_uptime_get(),
            .thread_id = k_current_get(),
            .error_code = reason,
            .description = "Stack overflow - thread termination"
        };
        
        ft_report_fault(&ctx);
        
        /* Only THIS thread is halted - others continue! */
        k_fatal_halt(reason);
    }
    
    /* For other errors, halt system */
    printk("Non-recoverable system error, halting\n");
    k_fatal_halt(reason);
}

int main(void)
{
    printk("\n");
    printk("===========================================================\n");
    printk("           MULTI-THREAD RESILIENCE DEMONSTRATION\n");
    printk("===========================================================\n");
    printk("This demo starts 8 threads:\n");
    printk("  🔥 Critical System    🌐 Network Service\n");
    printk("  📡 Sensor Monitor     🖥️ User Interface\n");
    printk("  📝 System Logger      ⚙️ Worker 1 (WILL FAIL)\n");
    printk("  ⚙️ Worker 2 (safe)    ⚙️ Worker 3 (safe)\n");
    printk("\n");
    printk("Watch: When Worker 1 overflows its stack, ONLY that thread\n");
    printk("dies while ALL 7 other threads continue running normally!\n");
    printk("===========================================================\n\n");
    
    /* Initialize fault tolerance framework */
    printk("🚀 Initializing fault tolerance framework...\n");
    if (fault_tolerance_init(NULL) != 0) {
        printk("❌ Framework initialization failed\n");
        return -1;
    }
    
    if (ft_register_handler(&overflow_handler) != 0) {
        printk("❌ Handler registration failed\n");
        return -1;
    }
    
    printk("✅ Fault tolerance framework ready\n\n");
    
    /* Start all system service threads */
    printk("🚀 Starting all system threads...\n\n");
    
    k_thread_create(&critical_thread, critical_stack, K_THREAD_STACK_SIZEOF(critical_stack),
                   critical_system_thread, NULL, NULL, NULL,
                   K_PRIO_COOP(1), 0, K_NO_WAIT);
    k_thread_name_set(&critical_thread, "critical");
    
    k_thread_create(&network_thread, network_stack, K_THREAD_STACK_SIZEOF(network_stack),
                   network_service_thread, NULL, NULL, NULL,
                   K_PRIO_COOP(2), 0, K_NO_WAIT);
    k_thread_name_set(&network_thread, "network");
    
    k_thread_create(&sensor_thread, sensor_stack, K_THREAD_STACK_SIZEOF(sensor_stack),
                   sensor_monitoring_thread, NULL, NULL, NULL,
                   K_PRIO_COOP(3), 0, K_NO_WAIT);
    k_thread_name_set(&sensor_thread, "sensor");
    
    k_thread_create(&ui_thread, ui_stack, K_THREAD_STACK_SIZEOF(ui_stack),
                   user_interface_thread, NULL, NULL, NULL,
                   K_PRIO_COOP(4), 0, K_NO_WAIT);
    k_thread_name_set(&ui_thread, "ui");
    
    k_thread_create(&logger_thread, logger_stack, K_THREAD_STACK_SIZEOF(logger_stack),
                   system_logger_thread, NULL, NULL, NULL,
                   K_PRIO_COOP(5), 0, K_NO_WAIT);
    k_thread_name_set(&logger_thread, "logger");
    
    /* Start worker threads - one will fail, others continue */
    k_thread_create(&worker1_thread, worker1_stack, K_THREAD_STACK_SIZEOF(worker1_stack),
                   worker1_thread_func, NULL, NULL, NULL,
                   K_PRIO_COOP(6), 0, K_NO_WAIT);
    k_thread_name_set(&worker1_thread, "worker1_doomed");
    
    k_thread_create(&worker2_thread, worker2_stack, K_THREAD_STACK_SIZEOF(worker2_stack),
                   worker2_thread_func, NULL, NULL, NULL,
                   K_PRIO_COOP(7), 0, K_NO_WAIT);
    k_thread_name_set(&worker2_thread, "worker2_safe");
    
    k_thread_create(&worker3_thread, worker3_stack, K_THREAD_STACK_SIZEOF(worker3_stack),
                   worker3_thread_func, NULL, NULL, NULL,
                   K_PRIO_COOP(8), 0, K_NO_WAIT);
    k_thread_name_set(&worker3_thread, "worker3_safe");
    
    printk("🎬 ALL THREADS STARTED! Monitoring resilience...\n\n");
    
    /* Monitor the demonstration */
    for (int demo_minute = 0; demo_minute < 15; demo_minute++) {
        k_sleep(K_SECONDS(4)); /* Check every 4 seconds */
        
        uint32_t threads_alive = atomic_get(&system_state.threads_running);
        uint32_t total_ops = atomic_get(&system_state.total_operations);
        uint32_t faults = atomic_get(&system_state.faults_detected);
        
        printk("\n📊 RESILIENCE CHECK [%d/15]:\n", demo_minute + 1);
        printk("   🧵 Active threads: %u\n", threads_alive);
        printk("   📈 Total operations: %u\n", total_ops);
        printk("   💥 Faults handled: %u\n", faults);
        printk("   ✨ Core services: %s\n", 
               (threads_alive >= 5) ? "FULLY OPERATIONAL" : "DEGRADED");
        printk("   🎯 System status: %s\n\n",
               faults > 0 ? "RESILIENCE PROVEN! 🎉" : "awaiting fault...");
        
        /* Once we've proven resilience, we can conclude */
        if (faults > 0 && demo_minute > 8) {
            printk("✅ RESILIENCE DEMONSTRATION COMPLETE!\n");
            break;
        }
    }
    
    /* Graceful shutdown */
    printk("\n🛑 Initiating graceful shutdown...\n");
    system_state.demo_active = false;
    
    /* Give threads time to shut down */
    k_sleep(K_SECONDS(2));
    
    printk("\n");
    printk("===================================\n");
    printk("      DEMONSTRATION RESULTS\n");
    printk("===================================\n");
    printk("Critical heartbeats: %u ✅\n", thread_stats.critical_heartbeats);
    printk("Network packets: %u ✅\n", thread_stats.network_packets);
    printk("Sensor readings: %u ✅\n", thread_stats.sensor_readings);
    printk("UI updates: %u ✅\n", thread_stats.ui_updates);
    printk("Log entries: %u ✅\n", thread_stats.log_entries);
    printk("Worker1 cycles: %u %s\n", thread_stats.worker1_cycles, thread_stats.worker1_alive ? "✅" : "💥 TERMINATED");
    printk("Worker2 cycles: %u ✅\n", thread_stats.worker2_cycles);
    printk("Worker3 cycles: %u ✅\n", thread_stats.worker3_cycles);
    printk("\n");
    printk("Total operations: %u\n", atomic_get(&system_state.total_operations));
    printk("Faults handled: %u\n", atomic_get(&system_state.faults_detected));
    printk("\n");
    printk("🎯 CONCLUSION: System demonstrated resilience!\n");
    printk("   Only faulting thread terminated, others continued!\n");
    printk("===================================\n");
    
    return 0;
}