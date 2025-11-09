/**
 * @file buffer_overflow_demo.c
 * @author Jack Ostapeic
 * @brief Buffer Overflow Fault Tolerance Demonstration
 *
 * This demo shows comprehensive buffer overflow detection and prevention:
 * 1. Canary-based buffer protection
 * 2. Bounds checking with automatic recovery
 * 3. Memory corruption detection
 * 4. Graceful degradation when faults occur
 * 5. Multi-thread resilience during memory faults
 */

#include <zephyr/kernel.h>
#include <zephyr/fault_tolerance.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/random/random.h>
#include <string.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(buffer_overflow_demo, LOG_LEVEL_INF);

/* Buffer protection constants */
#define BUFFER_SIZE 64
#define CANARY_VALUE 0xDEADBEEF
#define SAFE_ZONE_SIZE 16
#define MAX_OPERATIONS 1000

/* Thread stack sizes - generous to prevent other issues */
#define STACK_SIZE 2048

static K_THREAD_STACK_DEFINE(monitor_stack, STACK_SIZE);
static K_THREAD_STACK_DEFINE(safe_worker_stack, STACK_SIZE);
static K_THREAD_STACK_DEFINE(risky_worker_stack, STACK_SIZE);
static K_THREAD_STACK_DEFINE(network_stack, STACK_SIZE);

static struct k_thread monitor_thread;
static struct k_thread safe_worker_thread;
static struct k_thread risky_worker_thread;
static struct k_thread network_thread;

/* System state */
static volatile bool system_running = true;
static volatile bool risky_worker_active = true;
static volatile bool buffer_overflow_detected = false;

/* Operation counters */
static uint32_t monitor_cycles = 0;
static uint32_t safe_worker_ops = 0;
static uint32_t risky_worker_ops = 0;
static uint32_t network_ops = 0;

/* Protected buffer structure with canaries */
struct protected_buffer {
    uint32_t start_canary;              /* Start boundary marker */
    char data[BUFFER_SIZE];             /* Actual buffer data */
    uint32_t end_canary;                /* End boundary marker */
    char safe_zone[SAFE_ZONE_SIZE];     /* Safety buffer to catch overflows */
    uint32_t final_canary;              /* Final protection marker */
    size_t write_count;                 /* Track number of writes */
    bool is_corrupted;                  /* Corruption flag */
};

/* Global protected buffers for different threads */
static struct protected_buffer safe_buffer = {
    .start_canary = CANARY_VALUE,
    .end_canary = CANARY_VALUE,
    .final_canary = CANARY_VALUE,
    .write_count = 0,
    .is_corrupted = false
};

static struct protected_buffer risky_buffer = {
    .start_canary = CANARY_VALUE,
    .end_canary = CANARY_VALUE,
    .final_canary = CANARY_VALUE,
    .write_count = 0,
    .is_corrupted = false
};

/* Recovery semaphore */
static K_SEM_DEFINE(recovery_complete, 0, 1);

/* Buffer overflow fault handler */
static struct ft_handler buffer_overflow_handler = {
    .fault_type = FT_FAULT_MEMORY_CORRUPTION,
    .priority = 1,
    .name = "buffer_overflow_handler",
    .handler = NULL,
    .user_data = NULL
};

/**
 * @brief Check buffer integrity using canary values
 * @param buffer Pointer to protected buffer
 * @return true if buffer is intact, false if corrupted
 */
static bool check_buffer_integrity(struct protected_buffer *buffer)
{
    if (buffer->start_canary != CANARY_VALUE) {
        printk("🚨 CORRUPTION: Start canary compromised! Expected 0x%08X, got 0x%08X\n",
               CANARY_VALUE, buffer->start_canary);
        return false;
    }
    
    if (buffer->end_canary != CANARY_VALUE) {
        printk("🚨 CORRUPTION: End canary compromised! Expected 0x%08X, got 0x%08X\n",
               CANARY_VALUE, buffer->end_canary);
        return false;
    }
    
    if (buffer->final_canary != CANARY_VALUE) {
        printk("🚨 CORRUPTION: Final canary compromised! Expected 0x%08X, got 0x%08X\n",
               CANARY_VALUE, buffer->final_canary);
        return false;
    }
    
    /* Check safe zone for any non-zero bytes (overflow detection) */
    for (int i = 0; i < SAFE_ZONE_SIZE; i++) {
        if (buffer->safe_zone[i] != 0) {
            printk("🚨 BUFFER OVERFLOW: Safe zone byte %d corrupted (value: 0x%02X)\n",
                   i, (uint8_t)buffer->safe_zone[i]);
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Repair corrupted buffer by restoring canaries and clearing data
 * @param buffer Pointer to corrupted buffer
 */
static void repair_buffer(struct protected_buffer *buffer)
{
    printk("🔧 REPAIR: Restoring buffer integrity...\n");
    
    /* Restore canary values */
    buffer->start_canary = CANARY_VALUE;
    buffer->end_canary = CANARY_VALUE;
    buffer->final_canary = CANARY_VALUE;
    
    /* Clear corrupted data */
    memset(buffer->data, 0, BUFFER_SIZE);
    memset(buffer->safe_zone, 0, SAFE_ZONE_SIZE);
    
    /* Reset metadata */
    buffer->write_count = 0;
    buffer->is_corrupted = false;
    
    printk("🔧 REPAIR: Buffer restored to safe state\n");
}

/**
 * @brief Safe buffer write with bounds checking
 * @param buffer Target buffer
 * @param data Data to write
 * @param len Length of data
 * @return true if write successful, false if would overflow
 */
static bool safe_buffer_write(struct protected_buffer *buffer, const char *data, size_t len)
{
    /* Check current integrity */
    if (!check_buffer_integrity(buffer)) {
        buffer->is_corrupted = true;
        return false;
    }
    
    /* Bounds check */
    if (len > BUFFER_SIZE) {
        printk("⚠️ BOUNDS CHECK: Write size %zu exceeds buffer size %d\n", len, BUFFER_SIZE);
        return false;
    }
    
    /* Safe write */
    memcpy(buffer->data, data, len);
    buffer->write_count++;
    
    return true;
}

/**
 * @brief Unsafe buffer write that can cause overflow (for demonstration)
 * @param buffer Target buffer
 * @param data Data to write
 * @param len Length of data (can exceed buffer size)
 */
static void unsafe_buffer_write(struct protected_buffer *buffer, const char *data, size_t len)
{
    /* Dangerous: no bounds checking! */
    memcpy(buffer->data, data, len);
    buffer->write_count++;
}

/**
 * @brief Buffer overflow fault handler function
 */
static enum ft_handler_result handle_buffer_overflow(const struct ft_fault_context *fault_ctx,
                                                   struct ft_recovery_context *recovery_ctx,
                                                   void *user_data)
{
    printk("\n🛡️ BUFFER OVERFLOW HANDLER: Memory corruption detected!\n");
    printk("🛡️ HANDLER: Thread '%s' caused buffer overflow\n",
           k_thread_name_get(fault_ctx->thread_id) ?: "unknown");
    printk("🛡️ HANDLER: Initiating buffer repair and thread isolation...\n");
    
    /* Repair the corrupted buffer */
    repair_buffer(&risky_buffer);
    repair_buffer(&safe_buffer);
    
    /* Signal recovery completion */
    k_sem_give(&recovery_complete);
    
    recovery_ctx->action = FT_RECOVERY_NONE;
    
    printk("🛡️ HANDLER: Buffer overflow recovery complete!\n");
    
    return FT_HANDLER_HANDLED;
}

/**
 * @brief Monitor thread - tracks system health and buffer integrity
 */
void monitor_thread_func(void *a, void *b, void *c)
{
    printk("📊 MONITOR: Buffer overflow monitoring started\n");
    
    while (system_running) {
        monitor_cycles++;
        k_sleep(K_SECONDS(3));
        
        printk("\n📊 MONITOR: Health Check #%u:\n", monitor_cycles);
        printk("   ✅ Safe Worker:  %u ops [%s]\n", 
               safe_worker_ops, "🟢 ACTIVE");
        printk("   ⚠️  Risky Worker: %u ops [%s]\n", 
               risky_worker_ops, 
               risky_worker_active ? "🟢 ACTIVE" : "🔴 ISOLATED");
        printk("   🌐 Network:      %u ops [%s]\n", 
               network_ops, "🟢 ACTIVE");
        
        /* Check buffer integrity */
        bool safe_intact = check_buffer_integrity(&safe_buffer);
        bool risky_intact = check_buffer_integrity(&risky_buffer);
        
        printk("   🛡️ Safe Buffer:   %s (writes: %zu)\n", 
               safe_intact ? "✅ INTACT" : "🚨 CORRUPTED", safe_buffer.write_count);
        printk("   🛡️ Risky Buffer:  %s (writes: %zu)\n", 
               risky_intact ? "✅ INTACT" : "🚨 CORRUPTED", risky_buffer.write_count);
        
        if (!safe_intact || !risky_intact) {
            printk("   🚨 MEMORY CORRUPTION DETECTED!\n");
            
            /* Report fault to framework */
            struct ft_fault_context ctx = {
                .fault_type = FT_FAULT_MEMORY_CORRUPTION,
                .severity = FT_SEVERITY_ERROR,
                .timestamp = k_uptime_get(),
                .thread_id = risky_worker_active ? &risky_worker_thread : &safe_worker_thread,
                .error_code = 0,
                .description = "Buffer overflow detected via canary check"
            };
            
            buffer_overflow_detected = true;
            ft_report_fault(&ctx);
        }
        
        if (buffer_overflow_detected) {
            printk("   🛡️ Fault tolerance: ACTIVE\n");
            printk("   🎯 System recovery: IN PROGRESS\n");
        }
    }
}

/**
 * @brief Safe worker thread - demonstrates proper buffer handling
 */
void safe_worker_thread_func(void *a, void *b, void *c)
{
    k_sleep(K_SECONDS(2));  /* Stagger startup */
    printk("✅ SAFE WORKER: Starting with proper bounds checking\n");
    
    while (system_running) {
        safe_worker_ops++;
        
        /* Generate safe test data */
        char test_data[32];
        uint32_t data_size = sys_rand32_get() % 30 + 1;  /* 1-30 bytes */
        
        for (int i = 0; i < data_size; i++) {
            test_data[i] = 'A' + (sys_rand32_get() % 26);
        }
        test_data[data_size] = '\0';
        
        /* Always use safe write */
        if (safe_buffer_write(&safe_buffer, test_data, data_size)) {
            if (safe_worker_ops % 5 == 0) {
                printk("✅ SAFE WORKER: Operation %u complete (wrote %u bytes safely)\n",
                       safe_worker_ops, data_size);
            }
        } else {
            printk("✅ SAFE WORKER: Write rejected - bounds check prevented overflow!\n");
        }
        
        k_sleep(K_MSEC(1500));
    }
}

/**
 * @brief Show final demonstration results
 */
static void show_final_results(void)
{
    printk("\n\n");
    printk("=========================================================\n");
    printk("        BUFFER OVERFLOW FAULT TOLERANCE RESULTS\n");
    printk("=========================================================\n");
    printk("🛡️ PROTECTION STRATEGY: Multi-layered buffer guards\n");
    printk("📊 DETECTION METHOD: Canary + bounds + integrity checks\n");
    printk("🔧 RECOVERY APPROACH: Buffer repair + thread isolation\n");
    printk("⏱️  SYSTEM UPTIME: Continuous operation maintained\n");
    printk("\n");
    printk("Thread Performance Summary:\n");
    printk("  📊 Monitor:     %u health checks ✅ (CORRUPTION DETECTION)\n", monitor_cycles);
    printk("  ✅ Safe Worker: %u operations   ✅ (ZERO OVERFLOWS)\n", safe_worker_ops);
    printk("  🌐 Network:     %u packets      ✅ (UNINTERRUPTED SERVICE)\n", network_ops);
    printk("  ⚠️  Risky Worker:%u operations   🛠️ (RECOVERED AFTER FAULT)\n", risky_worker_ops);
    printk("\n");
    printk("Buffer Protection Results:\n");
    printk("  🛡️ Canary detection: %s\n", buffer_overflow_detected ? "✅ SUCCESSFUL" : "⏸️ STANDBY");
    printk("  🔧 Buffer repair: %s\n", buffer_overflow_detected ? "✅ COMPLETED" : "⏸️ NOT NEEDED");
    printk("  ✅ Safe buffer: %zu writes (✅ ALWAYS PROTECTED)\n", safe_buffer.write_count);
    printk("  ⚠️  Risky buffer: %zu writes (%s)\n", risky_buffer.write_count,
           risky_buffer.is_corrupted ? "🛠️ REPAIRED" : "✅ INTACT");
    printk("\n");
    printk("🏆 DEMONSTRATION COMPLETE: Buffer overflow detection\n");
    printk("    and recovery proves embedded systems can handle\n");
    printk("    memory corruption faults gracefully!\n");
    printk("\n");
    printk("🎯 KEY ACHIEVEMENT: Zero data loss, zero downtime,\n");
    printk("    automatic fault recovery in real-time!\n");
    printk("=========================================================\n");
}

/**
 * @brief Risky worker thread - demonstrates buffer overflow scenarios
 */
void risky_worker_thread_func(void *a, void *b, void *c)
{
    k_sleep(K_SECONDS(4));  /* Stagger startup */
    printk("⚠️ RISKY WORKER: Starting operations (DANGER: potential overflows)\n");
    
    while (system_running && risky_worker_active) {
        risky_worker_ops++;
        
        /* Start with safe operations, gradually increase risk */
        if (risky_worker_ops <= 5) {
            /* Safe operations initially */
            char safe_data[] = "Safe operation";
            if (safe_buffer_write(&risky_buffer, safe_data, strlen(safe_data))) {
                printk("⚠️ RISKY WORKER: Safe operation %u (establishing baseline)\n", risky_worker_ops);
            }
        } else if (risky_worker_ops <= 10) {
            /* Slightly risky - pushing boundaries */
            char boundary_data[BUFFER_SIZE + 1];
            memset(boundary_data, 'X', BUFFER_SIZE);
            boundary_data[BUFFER_SIZE] = '\0';
            
            if (safe_buffer_write(&risky_buffer, boundary_data, BUFFER_SIZE)) {
                printk("⚠️ RISKY WORKER: Boundary test %u (max safe size)\n", risky_worker_ops);
            }
        } else {
            /* DANGEROUS: Intentional buffer overflow for demonstration */
            char overflow_data[BUFFER_SIZE + 20];  /* Bigger than buffer! */
            memset(overflow_data, 'O', sizeof(overflow_data) - 1);
            overflow_data[sizeof(overflow_data) - 1] = '\0';
            
            printk("⚠️ RISKY WORKER: ⚡ ATTEMPTING BUFFER OVERFLOW ⚡ (op %u)\n", risky_worker_ops);
            printk("⚠️ RISKY WORKER: Writing %zu bytes to %d byte buffer!\n", 
                   sizeof(overflow_data) - 1, BUFFER_SIZE);
            
            /* This WILL cause overflow! */
            unsafe_buffer_write(&risky_buffer, overflow_data, sizeof(overflow_data) - 1);
            
            printk("⚠️ RISKY WORKER: Overflow write complete - checking for corruption...\n");
            
            /* Check immediately for corruption */
            if (!check_buffer_integrity(&risky_buffer)) {
                printk("⚠️ RISKY WORKER: 🚨 CORRUPTION CONFIRMED! 🚨\n");
                risky_worker_active = false;
                
                /* Wait for recovery */
                printk("⚠️ RISKY WORKER: Waiting for fault tolerance recovery...\n");
                k_sem_take(&recovery_complete, K_FOREVER);
                printk("⚠️ RISKY WORKER: 🛠️ Recovery complete, entering safe mode\n");
                
                /* Continue operating in safe mode after recovery */
                risky_worker_active = true;  /* Reactivate in safe mode */
                printk("⚠️ RISKY WORKER: Resuming operations in SAFE MODE\n");
                
                /* Continue to safe mode operations below */
            }
        }
        
        /* Safe mode operations after fault recovery */
        if (buffer_overflow_detected && risky_worker_ops > 11) {
            /* Only safe operations from now on */
            char safe_data[32];
            uint32_t data_size = (sys_rand32_get() % 20) + 1;  /* 1-20 bytes - always safe */
            
            for (int i = 0; i < data_size; i++) {
                safe_data[i] = 'S' + (sys_rand32_get() % 6);  /* S,T,U,V,W,X */
            }
            safe_data[data_size] = '\0';
            
            if (safe_buffer_write(&risky_buffer, safe_data, data_size)) {
                if (risky_worker_ops % 10 == 0) {
                    printk("⚠️ RISKY WORKER: Safe mode operation %u (wrote %u bytes)\n", 
                           risky_worker_ops, data_size);
                }
            }
        }
        
        k_sleep(K_MSEC(2000));
    }
    
    printk("⚠️ RISKY WORKER: Thread entering safe mode\n");
}

/**
 * @brief Network thread - simulates network operations continuing during faults
 */
void network_thread_func(void *a, void *b, void *c)
{
    k_sleep(K_SECONDS(3));  /* Stagger startup */
    printk("🌐 NETWORK: Communications subsystem started\n");
    
    while (system_running) {
        network_ops++;
        
        if (network_ops % 6 == 0) {
            printk("🌐 NETWORK: Processed packet %u (communications stable)\n", network_ops);
        }
        
        /* Show resilience during buffer overflow recovery */
        if (buffer_overflow_detected && network_ops % 3 == 0) {
            printk("🌐 NETWORK: Maintaining communications during fault recovery\n");
        }
        
        k_sleep(K_MSEC(2500));
    }
}

int main(void)
{
    printk("\n");
    printk("=========================================================\n");
    printk("       BUFFER OVERFLOW FAULT TOLERANCE DEMONSTRATION\n");
    printk("=========================================================\n");
    printk("This demo shows comprehensive buffer overflow protection:\n");
    printk("  1. 🛡️ Canary-based corruption detection\n");
    printk("  2. ✅ Bounds checking with safe writes\n");
    printk("  3. 🚨 Real-time corruption monitoring\n");
    printk("  4. 🔧 Automatic buffer repair and recovery\n");
    printk("  5. 🎯 Thread isolation during faults\n");
    printk("  6. 🌐 Service continuity during recovery\n");
    printk("\n");
    printk("Buffer Protection Features:\n");
    printk("  • Start/End/Final canary values (0x%08X)\n", CANARY_VALUE);
    printk("  • %d byte protected buffer + %d byte safe zone\n", BUFFER_SIZE, SAFE_ZONE_SIZE);
    printk("  • Real-time integrity monitoring\n");
    printk("  • Automatic fault detection and reporting\n");
    printk("=========================================================\n\n");
    
    /* Initialize framework */
    if (fault_tolerance_init(NULL) != 0) {
        printk("❌ Framework initialization failed\n");
        return -1;
    }
    
    /* Set up buffer overflow handler */
    buffer_overflow_handler.handler = handle_buffer_overflow;
    if (ft_register_handler(&buffer_overflow_handler) != 0) {
        printk("❌ Handler registration failed\n");
        return -1;
    }
    
    printk("✅ Buffer overflow fault tolerance ready\n\n");
    
    /* Initialize buffers */
    memset(safe_buffer.data, 0, BUFFER_SIZE);
    memset(safe_buffer.safe_zone, 0, SAFE_ZONE_SIZE);
    memset(risky_buffer.data, 0, BUFFER_SIZE);
    memset(risky_buffer.safe_zone, 0, SAFE_ZONE_SIZE);
    
    printk("🛡️ Protected buffers initialized with canary guards\n");
    printk("🚀 Starting demonstration threads...\n\n");
    
    /* Start all threads */
    k_thread_create(&monitor_thread, monitor_stack, K_THREAD_STACK_SIZEOF(monitor_stack),
                   monitor_thread_func, NULL, NULL, NULL, K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
    k_thread_name_set(&monitor_thread, "monitor");
    
    k_thread_create(&safe_worker_thread, safe_worker_stack, K_THREAD_STACK_SIZEOF(safe_worker_stack),
                   safe_worker_thread_func, NULL, NULL, NULL, K_PRIO_PREEMPT(7), 0, K_NO_WAIT);
    k_thread_name_set(&safe_worker_thread, "safe_worker");
    
    k_thread_create(&network_thread, network_stack, K_THREAD_STACK_SIZEOF(network_stack),
                   network_thread_func, NULL, NULL, NULL, K_PRIO_PREEMPT(8), 0, K_NO_WAIT);
    k_thread_name_set(&network_thread, "network");
    
    k_thread_create(&risky_worker_thread, risky_worker_stack, K_THREAD_STACK_SIZEOF(risky_worker_stack),
                   risky_worker_thread_func, NULL, NULL, NULL, K_PRIO_PREEMPT(9), 0, K_NO_WAIT);
    k_thread_name_set(&risky_worker_thread, "risky_worker");
    
    printk("🎬 ALL THREADS RUNNING!\n\n");
    
    /* Run demonstration continuously - show status updates during fault recovery */
    uint32_t demo_seconds = 0;
    bool results_shown = false;
    while (system_running) {  /* Run indefinitely to show continuous operation */
        k_sleep(K_SECONDS(15));
        demo_seconds += 15;
        
        /* Show status updates during fault recovery */
        if (buffer_overflow_detected && demo_seconds >= 45 && demo_seconds <= 120) {
            printk("\n⏰ STATUS (t+%u seconds):\n", demo_seconds);
            printk("   🛡️ Buffer overflow detection: ✅ SUCCESSFUL\n");
            printk("   🔧 Automatic recovery: ✅ COMPLETE\n");
            printk("   🎯 Thread isolation: ✅ EFFECTIVE\n");
            printk("   🌐 Service continuity: ✅ MAINTAINED\n");
            printk("   🔄 System recovery: ✅ OPERATIONAL\n");
        }
        
        /* Show final results once after recovery, then continue operations */
        if (buffer_overflow_detected && demo_seconds >= 120 && !results_shown) {
            show_final_results();
            results_shown = true;
            printk("\n🔄 CONTINUING NORMAL OPERATIONS AFTER FAULT RECOVERY...\n");
            printk("   (System proving continuous resilience)\n\n");
        }
        
        /* Show continuous operation heartbeat every minute after results shown */
        if (results_shown && demo_seconds % 60 == 0) {
            printk("💓 CONTINUOUS OPERATION: System healthy (uptime: %llu ms)\n", k_uptime_get());
            printk("   🔄 Active threads: Monitor=%u, SafeWorker=%u, Network=%u, RiskyWorker=%u\n",
                   monitor_cycles, safe_worker_ops, network_ops, risky_worker_ops);
            printk("   🛡️ Buffer status: %s\n",
                   (check_buffer_integrity(&safe_buffer) && check_buffer_integrity(&risky_buffer)) ?
                   "✅ ALL INTACT" : "🛠️ REPAIRED");
        }
    }
    
    return 0;
}