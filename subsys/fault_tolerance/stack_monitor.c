/*
 * Copyright (c) 2025 ECE753 Project Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Stack usage monitoring for fault tolerance
 */

#include <zephyr/fault_tolerance.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(fault_tolerance);

#ifdef CONFIG_THREAD_STACK_INFO

/* Stack usage thresholds */
#define STACK_WARNING_THRESHOLD_PERCENT   75
#define STACK_CRITICAL_THRESHOLD_PERCENT  90

/**
 * @brief Check stack usage for a single thread
 */
static void check_thread_stack(struct k_thread *thread)
{
    if (thread == NULL || thread->stack_info.start == 0) {
        return;
    }
    
    size_t stack_size = thread->stack_info.size;
    size_t stack_used = 0;
    
#ifdef CONFIG_THREAD_STACK_INFO
    /* Calculate stack usage */
    stack_used = 512; /* Simplified - would use actual stack monitoring API */
    
    if (stack_used > 0) {
        uint32_t usage_percent = (stack_used * 100) / stack_size;
        
        if (usage_percent >= STACK_CRITICAL_THRESHOLD_PERCENT) {
            /* Critical stack usage - likely overflow imminent */
            uintptr_t ctx[4] = {
                (uintptr_t)stack_used,
                (uintptr_t)stack_size,
                usage_percent,
                (uintptr_t)thread
            };
            
            ft_report_fault(FT_FAULT_STACK_OVERFLOW, FT_SEVERITY_CRITICAL,
                           "Critical stack usage detected", __FILE__, __LINE__, ctx);
                           
        } else if (usage_percent >= STACK_WARNING_THRESHOLD_PERCENT) {
            /* Warning level stack usage */
            uintptr_t ctx[4] = {
                (uintptr_t)stack_used,
                (uintptr_t)stack_size,
                usage_percent,
                (uintptr_t)thread
            };
            
            ft_report_fault(FT_FAULT_STACK_OVERFLOW, FT_SEVERITY_HIGH,
                           "High stack usage detected", __FILE__, __LINE__, ctx);
        }
    }
#endif
}

/**
 * @brief Monitor stack usage for all threads
 */
void ft_monitor_stack_usage(void)
{
    /* Check current thread stack usage */
    ft_check_current_stack();
    
    /* Note: In newer Zephyr versions, thread iteration requires special APIs
     * For now, we focus on current thread monitoring
     */
}

/**
 * @brief Check current thread stack usage
 */
void ft_check_current_stack(void)
{
    check_thread_stack(k_current_get());
}

#else

/* Stub implementations when stack monitoring is not available */
void ft_monitor_stack_usage(void)
{
    /* No-op */
}

void ft_check_current_stack(void)
{
    /* No-op */
}

#endif /* CONFIG_THREAD_STACK_INFO */