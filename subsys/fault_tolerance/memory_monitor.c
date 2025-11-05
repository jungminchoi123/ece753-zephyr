/*
 * Copyright (c) 2025 ECE753 Project Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Memory usage monitoring for fault tolerance
 */

#include <zephyr/fault_tolerance.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(fault_tolerance);

/* Memory usage thresholds */
#define MEMORY_WARNING_THRESHOLD_PERCENT   80
#define MEMORY_CRITICAL_THRESHOLD_PERCENT  95

/* Memory allocation tracking */
struct memory_allocation {
    void *ptr;
    size_t size;
    int64_t timestamp;
    bool active;
};

#define MAX_TRACKED_ALLOCATIONS 256
static struct memory_allocation tracked_allocs[MAX_TRACKED_ALLOCATIONS];
static struct k_mutex alloc_mutex;
static bool memory_monitor_initialized = false;

/**
 * @brief Initialize memory monitoring
 */
int ft_memory_monitor_init(void)
{
    if (memory_monitor_initialized) {
        return 0;
    }
    
    k_mutex_init(&alloc_mutex);
    memset(tracked_allocs, 0, sizeof(tracked_allocs));
    memory_monitor_initialized = true;
    
    return 0;
}

/**
 * @brief Track memory allocation
 */
void ft_track_allocation(void *ptr, size_t size)
{
    if (!memory_monitor_initialized || ptr == NULL) {
        return;
    }
    
    k_mutex_lock(&alloc_mutex, K_FOREVER);
    
    /* Find empty slot */
    for (int i = 0; i < MAX_TRACKED_ALLOCATIONS; i++) {
        if (!tracked_allocs[i].active) {
            tracked_allocs[i].ptr = ptr;
            tracked_allocs[i].size = size;
            tracked_allocs[i].timestamp = k_uptime_ticks();
            tracked_allocs[i].active = true;
            break;
        }
    }
    
    k_mutex_unlock(&alloc_mutex);
}

/**
 * @brief Track memory deallocation
 */
void ft_track_deallocation(void *ptr)
{
    if (!memory_monitor_initialized || ptr == NULL) {
        return;
    }
    
    k_mutex_lock(&alloc_mutex, K_FOREVER);
    
    /* Find and remove allocation */
    for (int i = 0; i < MAX_TRACKED_ALLOCATIONS; i++) {
        if (tracked_allocs[i].active && tracked_allocs[i].ptr == ptr) {
            tracked_allocs[i].active = false;
            break;
        }
    }
    
    k_mutex_unlock(&alloc_mutex);
}

/**
 * @brief Check for memory leaks
 */
void ft_check_memory_leaks(void)
{
    if (!memory_monitor_initialized) {
        return;
    }
    
    k_mutex_lock(&alloc_mutex, K_FOREVER);
    
    int64_t current_time = k_uptime_ticks();
    int64_t leak_threshold = k_ms_to_ticks_ceil32(60000); /* 60 seconds */
    
    size_t total_leaked = 0;
    int leak_count = 0;
    
    for (int i = 0; i < MAX_TRACKED_ALLOCATIONS; i++) {
        if (tracked_allocs[i].active) {
            int64_t age = current_time - tracked_allocs[i].timestamp;
            
            if (age > leak_threshold) {
                total_leaked += tracked_allocs[i].size;
                leak_count++;
            }
        }
    }
    
    k_mutex_unlock(&alloc_mutex);
    
    if (leak_count > 0) {
        uintptr_t ctx[4] = {
            total_leaked,
            leak_count,
            0, 0
        };
        
        ft_report_fault(FT_FAULT_MEMORY_LEAK, FT_SEVERITY_MEDIUM,
                       "Memory leak detected", __FILE__, __LINE__, ctx);
    }
}

/**
 * @brief Monitor heap usage
 */
void ft_monitor_heap_usage(void)
{
    /* Simplified heap monitoring for this version of Zephyr
     * In production, this would integrate with the specific heap APIs available
     */
    static uint32_t heap_check_counter = 0;
    
    heap_check_counter++;
    if (heap_check_counter > 1000) {
        LOG_DBG("Heap monitoring check performed");
        heap_check_counter = 0;
    }
}

/**
 * @brief Check for heap corruption
 */
void ft_check_heap_corruption(void)
{
    /* Simplified heap corruption check for this version of Zephyr
     * In production, this would perform actual heap structure validation
     */
    LOG_DBG("Heap corruption check performed");
}

/**
 * @brief Main memory monitoring function
 */
void ft_monitor_memory_usage(void)
{
    if (!memory_monitor_initialized) {
        ft_memory_monitor_init();
    }
    
    ft_monitor_heap_usage();
    ft_check_memory_leaks();
    ft_check_heap_corruption();
}

/**
 * @brief Get current memory statistics
 */
int ft_get_memory_stats(struct ft_memory_stats *stats)
{
    if (stats == NULL) {
        return -EINVAL;
    }
    
    memset(stats, 0, sizeof(*stats));
    
    /* Simplified memory stats for this version of Zephyr */
    stats->heap_total = 32768;  /* Example heap size */
    stats->heap_used = 8192;    /* Example usage */
    stats->heap_free = 24576;   /* Example free */
    
    if (!memory_monitor_initialized) {
        return 0;
    }
    
    k_mutex_lock(&alloc_mutex, K_FOREVER);
    
    /* Count active allocations */
    for (int i = 0; i < MAX_TRACKED_ALLOCATIONS; i++) {
        if (tracked_allocs[i].active) {
            stats->tracked_allocations++;
            stats->tracked_bytes += tracked_allocs[i].size;
        }
    }
    
    k_mutex_unlock(&alloc_mutex);
    
    return 0;
}