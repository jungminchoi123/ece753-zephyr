#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(memory_leak_test, LOG_LEVEL_INF);

/* Memory tracking structures */
struct allocation_record {
    void *ptr;
    size_t size;
    bool freed;
    int64_t timestamp;
};

#define MAX_TRACKED_ALLOCATIONS 50
static struct allocation_record allocations[MAX_TRACKED_ALLOCATIONS];
static int allocation_count = 0;
static size_t total_allocated = 0;
static size_t total_freed = 0;

static void track_allocation(void *ptr, size_t size) {
    if (allocation_count < MAX_TRACKED_ALLOCATIONS && ptr != NULL) {
        allocations[allocation_count].ptr = ptr;
        allocations[allocation_count].size = size;
        allocations[allocation_count].freed = false;
        allocations[allocation_count].timestamp = k_uptime_get();
        allocation_count++;
        total_allocated += size;
    }
}

static void track_deallocation(void *ptr) {
    for (int i = 0; i < allocation_count; i++) {
        if (allocations[i].ptr == ptr && !allocations[i].freed) {
            allocations[i].freed = true;
            total_freed += allocations[i].size;
            break;
        }
    }
}

static size_t get_leaked_memory(void) {
    return total_allocated - total_freed;
}

static void report_memory_status(void) {
    size_t leaked = get_leaked_memory();
    
    LOG_INF("📊 Memory Status:");
    LOG_INF("   Total allocated: %zu bytes", total_allocated);
    LOG_INF("   Total freed: %zu bytes", total_freed);
    LOG_INF("   Leaked memory: %zu bytes", leaked);
    
    if (leaked > CONFIG_FT_MEMORY_LEAK_THRESHOLD) {
        LOG_ERR("🚨 MEMORY LEAK DETECTED! Leaked: %zu bytes", leaked);
        ft_report_fault(FT_MEMORY_LEAK_FAULT, FT_SEVERITY_MEDIUM);
    }
}

/* Simulate common memory leak patterns */
static void test_simple_leak(void) {
    LOG_INF("📝 Testing simple memory leak");
    
    /* Allocate without freeing */
    for (int i = 0; i < 10; i++) {
        void *ptr = k_malloc(64);
        track_allocation(ptr, 64);
        LOG_INF("   Allocated 64 bytes at %p (leak #%d)", ptr, i + 1);
    }
    
    report_memory_status();
}

static void test_partial_cleanup(void) {
    LOG_INF("📝 Testing partial cleanup pattern");
    
    void *ptrs[10];
    
    /* Allocate multiple blocks */
    for (int i = 0; i < 10; i++) {
        ptrs[i] = k_malloc(32);
        track_allocation(ptrs[i], 32);
    }
    
    /* Free only some of them (common bug pattern) */
    for (int i = 0; i < 5; i++) {
        track_deallocation(ptrs[i]);
        k_free(ptrs[i]);
        LOG_INF("   Freed block %d", i);
    }
    
    LOG_WRN("   Intentionally not freeing blocks 5-9 (simulating leak)");
    report_memory_status();
}

static void test_loop_leak(void) {
    LOG_INF("📝 Testing loop-based memory leak");
    
    /* Common pattern: allocate in loop without proper cleanup */
    for (int iteration = 0; iteration < 5; iteration++) {
        LOG_INF("   Loop iteration %d", iteration);
        
        /* Allocate temporary buffers */
        for (int i = 0; i < 3; i++) {
            void *temp = k_malloc(128);
            track_allocation(temp, 128);
            
            /* Simulate work with the buffer */
            k_sleep(K_MSEC(10));
            
            /* "Forget" to free one buffer per iteration */
            if (i != 2) {
                track_deallocation(temp);
                k_free(temp);
            }
        }
    }
    
    report_memory_status();
}

static void test_double_free_protection(void) {
    LOG_INF("📝 Testing double-free protection");
    
    void *ptr = k_malloc(256);
    track_allocation(ptr, 256);
    LOG_INF("   Allocated 256 bytes at %p", ptr);
    
    /* First free (legitimate) */
    track_deallocation(ptr);
    k_free(ptr);
    LOG_INF("   First free successful");
    
    /* Attempt double-free (bug) */
    LOG_WRN("   Attempting double-free (should be caught)...");
    /* Note: k_free might not detect double-free, but we can track it */
    
    /* Check if this was already freed */
    bool already_freed = false;
    for (int i = 0; i < allocation_count; i++) {
        if (allocations[i].ptr == ptr && allocations[i].freed) {
            already_freed = true;
            break;
        }
    }
    
    if (already_freed) {
        LOG_ERR("🚨 DOUBLE-FREE DETECTED for pointer %p!", ptr);
        ft_report_fault(FT_BUFFER_OVERFLOW_FAULT, FT_SEVERITY_HIGH);
    }
}

void memory_leak_test_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("🧠 Starting memory leak detection test");
    LOG_INF("   Leak threshold: %d bytes", CONFIG_FT_MEMORY_LEAK_THRESHOLD);
    
    k_sleep(K_SECONDS(1));
    
    /* Test 1: Simple memory leaks */
    test_simple_leak();
    k_sleep(K_MSEC(500));
    
    /* Test 2: Partial cleanup (common in error handling) */
    test_partial_cleanup();
    k_sleep(K_MSEC(500));
    
    /* Test 3: Loop-based leaks */
    test_loop_leak();
    k_sleep(K_MSEC(500));
    
    /* Test 4: Double-free protection */
    test_double_free_protection();
    
    /* Final memory report */
    LOG_INF("🔍 Final memory leak analysis:");
    report_memory_status();
    
    /* List unfreed allocations */
    int leak_count = 0;
    for (int i = 0; i < allocation_count; i++) {
        if (!allocations[i].freed) {
            LOG_WRN("   LEAK: %zu bytes at %p (allocated at %lld ms)", 
                   allocations[i].size, allocations[i].ptr, 
                   allocations[i].timestamp);
            leak_count++;
        }
    }
    
    LOG_INF("🧠 Memory leak test completed - %d leaks detected", leak_count);
}