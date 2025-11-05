/*
 * Copyright (c) 2025 ECE753 Project Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance.h>

LOG_MODULE_REGISTER(ft_minimal_test, LOG_LEVEL_INF);

void main(void)
{
    LOG_INF("Enhanced Zephyr Fault Tolerance - Minimal Test");
    LOG_INF("ECE753 Project - Safety-Critical Embedded Systems");
    
    /* Initialize fault tolerance framework */
    int ret = ft_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize fault tolerance framework: %d", ret);
        return;
    }
    
    LOG_INF("Fault tolerance framework initialized successfully");
    
    /* Test basic statistics */
    struct ft_system_stats stats;
    ret = ft_get_statistics(&stats);
    if (ret == 0) {
        LOG_INF("Fault tolerance statistics retrieved successfully");
        printk("  Total faults: %u\n", stats.total_faults);
        printk("  Critical faults: %u\n", stats.critical_faults);
        printk("  Successful recoveries: %u\n", stats.successful_recoveries);
        printk("  Failed recoveries: %u\n", stats.failed_recoveries);
    }
    
    /* Test memory monitoring initialization */
    struct ft_memory_stats mem_stats;
    ret = ft_get_memory_stats(&mem_stats);
    if (ret == 0) {
        LOG_INF("Memory statistics retrieved successfully");
        printk("  Heap total: %u bytes\n", (uint32_t)mem_stats.heap_total);
        printk("  Heap used: %u bytes\n", (uint32_t)mem_stats.heap_used);
        printk("  Heap free: %u bytes\n", (uint32_t)mem_stats.heap_free);
    }
    
    LOG_INF("Basic functionality test completed successfully");
    
    /* Keep running to show the monitoring threads are working */
    while (1) {
        k_sleep(K_SECONDS(5));
        LOG_INF("System running normally...");
    }
}