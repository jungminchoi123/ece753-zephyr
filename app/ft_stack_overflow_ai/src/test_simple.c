/**
 * @file test_simple.c
 * @author Jack Ostapeic
 * @brief Simple Fault Tolerance Framework Test
 *
 * This demonstrates the basic functionality of the fault tolerance framework
 * without trying to continue execution after fatal errors.
 */

#include <zephyr/kernel.h>
#include <zephyr/fault_tolerance.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(ft_simple_test, LOG_LEVEL_INF);

#define STACK_SIZE 512  // Small stack to trigger overflow quickly
#define THREAD_PRIORITY K_PRIO_COOP(5)

struct k_thread test_thread;
K_THREAD_STACK_DEFINE(test_thread_stack, STACK_SIZE);

static bool framework_initialized = false;

// Custom fault handler for testing
static enum ft_handler_result test_fault_handler(
    const struct ft_fault_context *fault_ctx,
    struct ft_recovery_context *recovery_ctx,
    void *user_data)
{
    printk("\n=== FAULT TOLERANCE FRAMEWORK TEST SUCCESS! ===\n");
    printk("✓ Fault Type: %s\n", ft_get_fault_type_name(fault_ctx->fault_type));
    printk("✓ Severity: %s\n", ft_get_severity_name(fault_ctx->severity));
    printk("✓ Thread ID: %p\n", fault_ctx->thread_id);
    printk("✓ Error Code: 0x%x\n", fault_ctx->error_code);
    printk("✓ Description: %s\n", fault_ctx->description ? fault_ctx->description : "none");
    printk("✓ Framework working correctly!\n");
    printk("===============================================\n");

    // Just log the event, don't try to recover in this simple test
    recovery_ctx->action = FT_RECOVERY_NONE;
    
    return FT_HANDLER_HANDLED;
}

// Register our custom handler
static struct ft_handler custom_handler = {
    .fault_type = FT_FAULT_STACK_OVERFLOW,
    .handler = test_fault_handler,
    .priority = 1,  // High priority
    .name = "simple_test_handler",
    .user_data = NULL
};

// Simple recursive function to trigger stack overflow
void trigger_stack_overflow(int depth)
{
    volatile char buffer[100];  // Stack-consuming buffer
    
    // Fill buffer to prevent optimization
    for (int i = 0; i < 100; i++) {
        buffer[i] = depth & 0xFF;
    }
    
    printk("Stack overflow test depth: %d\n", depth);
    
    // Recurse until stack overflows
    trigger_stack_overflow(depth + 1);
}

void test_thread_func(void *p1, void *p2, void *p3)
{
    printk("Starting fault tolerance framework test...\n");
    k_sleep(K_MSEC(100));  // Let things settle
    
    printk("Triggering stack overflow to test framework...\n");
    trigger_stack_overflow(0);
}

// Fatal error handler integration
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
    printk("\n*** FATAL ERROR DETECTED ***\n");
    
    if (reason == K_ERR_STACK_CHK_FAIL && framework_initialized) {
        // Create fault context
        struct ft_fault_context ctx = {
            .fault_type = FT_FAULT_STACK_OVERFLOW,
            .severity = FT_SEVERITY_ERROR,
            .timestamp = k_uptime_get(),
            .thread_id = k_current_get(),
            .error_code = reason,
            .description = "Stack overflow in test thread",
            .custom_data = NULL,
            .custom_data_size = 0,
            .esf = esf,
            .fault_address = NULL
        };

        // Report to fault tolerance framework
        int ret = ft_report_fault(&ctx);
        if (ret == 0) {
            printk("✓ Successfully reported to fault tolerance framework\n");
            
            // Show statistics
            struct ft_stats stats;
            if (ft_get_stats(&stats) == 0) {
                printk("\n=== FINAL STATISTICS ===\n");
                printk("Total faults: %u\n", (uint32_t)atomic_get(&stats.total_faults));
                printk("Stack overflow faults: %u\n", 
                       (uint32_t)atomic_get(&stats.fault_counts[FT_FAULT_STACK_OVERFLOW]));
                printk("=========================\n");
            }
        } else {
            printk("✗ Failed to report fault: %d\n", ret);
        }
        
        printk("\n*** TEST COMPLETED SUCCESSFULLY! ***\n");
        printk("The fault tolerance framework is working correctly.\n");
    } else {
        printk("Framework not initialized or different error type\n");
    }

    // Cleanly halt the system
    k_fatal_halt(reason);
}

int main(void)
{
    LOG_INF("Simple Fault Tolerance Framework Test Starting...");

    // Initialize the fault tolerance framework
    printk("Initializing fault tolerance framework...\n");
    int ret = fault_tolerance_init(NULL);
    if (ret != 0) {
        printk("✗ Failed to initialize framework: %d\n", ret);
        return ret;
    }
    
    framework_initialized = true;
    printk("✓ Fault tolerance framework initialized\n");

    // Register our test handler
    ret = ft_register_handler(&custom_handler);
    if (ret != 0) {
        printk("✗ Failed to register handler: %d\n", ret);
        return ret;
    }
    printk("✓ Test fault handler registered\n");

    // Create test thread
    k_tid_t test_tid = k_thread_create(&test_thread, test_thread_stack, STACK_SIZE,
                                      test_thread_func, NULL, NULL, NULL,
                                      THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(test_tid, "test_thread");
    
    printk("✓ Test thread created: %p\n", test_tid);
    printk("\n--- Framework test will begin shortly ---\n");

    // Wait for the test to complete
    k_sleep(K_SECONDS(5));

    printk("Test should have completed by now.\n");
    return 0;
}