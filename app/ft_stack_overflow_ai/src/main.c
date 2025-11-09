/**
 * @file main.c
 * @author Jack Ostapeic, MS ECE student at UW-Madison
 * @brief Modular Fault Tolerance API Testing Application
 *
 * This application validates stack overflow recovery using the comprehensive
 * fault tolerance framework. It demonstrates the integration of the framework
 * with Zephyr's fatal error handling system.
 */

#include <zephyr/kernel.h>
#include <zephyr/fault_tolerance.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

#define STACK_SIZE 1024
#define THREAD_PRIORITY K_PRIO_COOP(5)

LOG_MODULE_REGISTER(ft_test, LOG_LEVEL_INF);

// Thread structures
struct k_thread test_thread;
K_THREAD_STACK_DEFINE(test_thread_stack, STACK_SIZE);

static bool framework_initialized = false;
static uint32_t test_iteration = 0;

// Custom fault handler for testing
static enum ft_handler_result test_fault_handler(
    const struct ft_fault_context *fault_ctx,
    struct ft_recovery_context *recovery_ctx,
    void *user_data)
{
    printk("\n=== CUSTOM FAULT HANDLER CALLED ===\n");
    printk("Fault Type: %s\n", ft_get_fault_type_name(fault_ctx->fault_type));
    printk("Severity: %s\n", ft_get_severity_name(fault_ctx->severity));
    printk("Thread ID: %p\n", fault_ctx->thread_id);
    printk("Error Code: 0x%x\n", fault_ctx->error_code);
    printk("Description: %s\n", fault_ctx->description ? fault_ctx->description : "none");
    printk("===================================\n");

    // Set up recovery action
    recovery_ctx->action = FT_RECOVERY_RESTART_THREAD;
    recovery_ctx->target_thread = fault_ctx->thread_id;
    recovery_ctx->async_recovery = true;

    return FT_HANDLER_HANDLED;
}

// Register our custom handler
static struct ft_handler custom_handler = {
    .fault_type = FT_FAULT_STACK_OVERFLOW,
    .handler = test_fault_handler,
    .priority = 5,
    .name = "test_handler",
    .user_data = NULL
};

// Recursive bomb to trigger stack overflow
void recursive_bomb(int depth)
{
    volatile char buffer[200];
    for (int i = 0; i < 200; i++) {
        buffer[i] = (depth + i) & 0xFF;
    }

    printk("Recursion depth: %d\n", depth);
    k_yield();
    recursive_bomb(depth + 1);
}

// Test thread function
void test_thread_func(void *p1, void *p2, void *p3)
{
    printk("Test thread started (iteration %d) - triggering stack overflow...\n", 
           test_iteration);
    
    k_sleep(K_MSEC(100));
    recursive_bomb(0);
}

// Custom fatal error handler that uses the FT API
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
    printk("\n*** FATAL ERROR HANDLER CALLED ***\n");
    printk("Reason: %u (K_ERR_STACK_CHK_FAIL=%u)\n", reason, K_ERR_STACK_CHK_FAIL);

    if (reason == K_ERR_STACK_CHK_FAIL && framework_initialized) {
        printk("Stack overflow detected - reporting to FT framework\n");

        // Create fault context
        struct ft_fault_context ctx = {
            .fault_type = FT_FAULT_STACK_OVERFLOW,
            .severity = FT_SEVERITY_ERROR,
            .timestamp = k_uptime_get(),
            .thread_id = k_current_get(),
            .error_code = reason,
            .description = "Stack overflow detected in test thread",
            .custom_data = NULL,
            .custom_data_size = 0,
            .esf = esf,
            .fault_address = NULL
        };

        // Report to fault tolerance framework
        int ret = ft_report_fault(&ctx);
        if (ret == 0) {
            printk("Successfully reported fault to FT framework\n");
            
            // Get and display statistics
            struct ft_stats stats;
            if (ft_get_stats(&stats) == 0) {
                printk("FT Statistics:\n");
                printk("  Total faults: %u\n", (uint32_t)atomic_get(&stats.total_faults));
                printk("  Recovered faults: %u\n", (uint32_t)atomic_get(&stats.recovered_faults));
                printk("  Stack overflow faults: %u\n", 
                       (uint32_t)atomic_get(&stats.fault_counts[FT_FAULT_STACK_OVERFLOW]));
            }
        } else {
            printk("Failed to report fault: %d\n", ret);
        }

        // Schedule next test iteration
        test_iteration++;
        if (test_iteration < 3) {
            printk("Scheduling next test iteration in 2 seconds...\n");
            k_sleep(K_SECONDS(2));
            
            k_thread_create(&test_thread, test_thread_stack, STACK_SIZE,
                           test_thread_func, NULL, NULL, NULL,
                           THREAD_PRIORITY, 0, K_NO_WAIT);
        } else {
            printk("Test completed - performed %d iterations\n", test_iteration);
        }
    }

    printk("Fatal error handler complete - hanging\n");
    k_fatal_halt(reason);
}

int main(void)
{
    LOG_INF("Fault Tolerance Framework Test Application Starting...");

    // Initialize the fault tolerance framework
    int ret = fault_tolerance_init(NULL);
    if (ret != 0) {
        LOG_ERR("Failed to initialize fault tolerance framework: %d", ret);
        return ret;
    }
    
    framework_initialized = true;
    LOG_INF("Fault tolerance framework initialized successfully");

    // Register our custom fault handler
    ret = ft_register_handler(&custom_handler);
    if (ret != 0) {
        LOG_ERR("Failed to register custom handler: %d", ret);
        return ret;
    }
    LOG_INF("Custom fault handler registered");

    // Register the test thread for recovery monitoring
    k_tid_t test_tid = k_thread_create(&test_thread, test_thread_stack, STACK_SIZE,
                                      test_thread_func, NULL, NULL, NULL,
                                      THREAD_PRIORITY, 0, K_NO_WAIT);

#ifdef CONFIG_FT_ENABLE_THREAD_RECOVERY
    ret = ft_thread_register(test_tid, test_thread_func, 
                            NULL, NULL, NULL,
                            STACK_SIZE, THREAD_PRIORITY, 0,
                            "test_thread");
    if (ret != 0) {
        LOG_WRN("Failed to register thread for recovery: %d", ret);
    } else {
        LOG_INF("Test thread registered for recovery monitoring");
    }
#endif

    // Main loop
    while (1) {
        k_sleep(K_SECONDS(1));
        
        if (framework_initialized) {
            // Periodically display framework status
            struct ft_stats stats;
            if (ft_get_stats(&stats) == 0) {
                LOG_INF("Framework stats - Total: %u, Recovered: %u", 
                        (uint32_t)atomic_get(&stats.total_faults),
                        (uint32_t)atomic_get(&stats.recovered_faults));
            }
        }
    }

    return 0;
}
