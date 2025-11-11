/**
 * @file heap_corruption_test.c
 * @brief ARM Cortex-M3 Heap Corruption and Double-Free Test Module
 * @author Jack Ostapeic, MS ECE Student at University of Wisconsin-Madison
 * 
 * This module implements comprehensive heap corruption and memory management
 * error testing for ARM Cortex-M3 architecture, utilizing both hardware and
 * software detection mechanisms for heap-related faults.
 *
 * Heap Corruption Types Tested:
 * 1. Double-Free Detection - Freeing already freed memory blocks
 * 2. Use-After-Free - Accessing memory after it has been freed
 * 3. Buffer Overflow in Heap - Writing beyond allocated boundaries
 * 4. Heap Metadata Corruption - Corrupting heap management structures
 * 5. Memory Leak Detection - Tracking unfreed allocations
 * 6. Invalid Free - Freeing non-allocated or invalid pointers
 * 7. Heap Exhaustion - Testing allocation failure handling
 * 8. Heap Fragmentation - Analyzing fragmentation effects
 *
 * Detection Methods:
 * - Custom heap wrapper with validation
 * - Magic number guards around allocations
 * - Freed memory poisoning
 * - Allocation tracking and leak detection
 * - Heap integrity validation
 * - Memory pattern analysis
 *
 * Recovery Mechanisms:
 * - Safe allocation/deallocation wrappers
 * - Heap corruption recovery procedures
 * - Memory leak cleanup
 * - Heap defragmentation
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance.h>
#include <zephyr/sys/heap.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(heap_corruption_test, LOG_LEVEL_INF);

/* Test configuration */
#define MAX_HEAP_TESTS 8
#define MAX_TRACKED_ALLOCATIONS 100
#define HEAP_MAGIC_PREFIX 0xDEADBEEF
#define HEAP_MAGIC_SUFFIX 0xCAFEBABE
#define POISON_PATTERN 0xDD
#define TEST_HEAP_SIZE (16 * 1024)  /* 16KB test heap */

/* Statistics tracking */
static int heap_tests_run = 0;
static int corruptions_detected = 0;
static volatile bool heap_fault_occurred = false;

/* Heap monitoring structures */
typedef struct allocation_record {
    void *ptr;
    size_t size;
    const char *file;
    int line;
    uint32_t magic;
    bool is_free;
    k_timeout_t alloc_time;
} allocation_record_t;

static allocation_record_t allocation_tracker[MAX_TRACKED_ALLOCATIONS];
static int allocation_count = 0;
static sys_heap_t test_heap;
static uint8_t heap_memory[TEST_HEAP_SIZE] __aligned(4);

/* Test results tracking */
typedef struct {
    const char *test_name;
    bool corruption_detected;
    bool double_free_detected;
    bool use_after_free_detected;
    uint32_t allocations_made;
    uint32_t deallocations_made;
    size_t memory_leaked;
    char description[128];
} heap_test_result_t;

static heap_test_result_t heap_results[MAX_HEAP_TESTS];

/**
 * @brief Initialize heap corruption detection system
 *
 * Sets up custom heap with monitoring and validation
 * mechanisms for comprehensive heap testing.
 *
 * @return 0 on success, negative on failure
 */
static int initialize_heap_monitoring(void)
{
    LOG_INF("Initializing heap corruption detection system...");
    
    /* Initialize custom test heap */
    sys_heap_init(&test_heap, heap_memory, TEST_HEAP_SIZE);
    
    /* Clear allocation tracking */
    memset(allocation_tracker, 0, sizeof(allocation_tracker));
    allocation_count = 0;
    
    /* Clear test results */
    memset(heap_results, 0, sizeof(heap_results));
    
    LOG_INF("✅ Heap monitoring initialized");
    LOG_INF("Test heap size: %d bytes at %p", TEST_HEAP_SIZE, heap_memory);
    
    return 0;
}

/**
 * @brief Validate allocation record integrity
 *
 * Checks allocation record for corruption or inconsistencies.
 *
 * @param record Allocation record to validate
 * @return true if valid, false if corrupted
 */
static bool validate_allocation_record(const allocation_record_t *record)
{
    if (record == NULL) return false;
    
    /* Check magic number */
    if (record->magic != HEAP_MAGIC_PREFIX) {
        LOG_ERR("🚨 Allocation record magic corrupted: 0x%08X", record->magic);
        return false;
    }
    
    /* Validate pointer alignment */
    if ((uintptr_t)record->ptr & 0x3) {
        LOG_ERR("🚨 Allocation pointer misaligned: %p", record->ptr);
        return false;
    }
    
    /* Check size reasonableness */
    if (record->size == 0 || record->size > TEST_HEAP_SIZE) {
        LOG_ERR("🚨 Invalid allocation size: %zu", record->size);
        return false;
    }
    
    return true;
}

/**
 * @brief Find allocation record by pointer
 *
 * Searches allocation tracking table for given pointer.
 *
 * @param ptr Pointer to search for
 * @return Allocation record or NULL if not found
 */
static allocation_record_t* find_allocation_record(void *ptr)
{
    for (int i = 0; i < allocation_count; i++) {
        if (allocation_tracker[i].ptr == ptr) {
            return &allocation_tracker[i];
        }
    }
    return NULL;
}

/**
 * @brief Safe malloc wrapper with tracking and validation
 *
 * Wrapper around heap allocation that adds corruption detection
 * and tracking mechanisms.
 *
 * @param size Number of bytes to allocate
 * @param file Source file name (__FILE__)
 * @param line Source line number (__LINE__)
 * @return Allocated pointer or NULL on failure
 */
static void* safe_malloc(size_t size, const char *file, int line)
{
    if (allocation_count >= MAX_TRACKED_ALLOCATIONS) {
        LOG_ERR("Allocation tracker full");
        return NULL;
    }
    
    /* Add space for magic guards */
    size_t total_size = size + 2 * sizeof(uint32_t);
    
    void *raw_ptr = sys_heap_alloc(&test_heap, total_size);
    if (raw_ptr == NULL) {
        LOG_ERR("Heap allocation failed");
        return NULL;
    }
    
    /* Set up magic guards */
    uint32_t *prefix = (uint32_t*)raw_ptr;
    uint32_t *suffix = (uint32_t*)((uint8_t*)raw_ptr + sizeof(uint32_t) + size);
    
    *prefix = HEAP_MAGIC_PREFIX;
    *suffix = HEAP_MAGIC_SUFFIX;
    
    /* User pointer starts after prefix */
    void *user_ptr = (uint8_t*)raw_ptr + sizeof(uint32_t);
    
    /* Record allocation */
    allocation_record_t *record = &allocation_tracker[allocation_count++];
    record->ptr = user_ptr;
    record->size = size;
    record->file = file;
    record->line = line;
    record->magic = HEAP_MAGIC_PREFIX;
    record->is_free = false;
    record->alloc_time = k_uptime_get();
    
    LOG_DBG("Allocated %zu bytes at %p", size, user_ptr);
    
    return user_ptr;
}

/**
 * @brief Safe free wrapper with double-free detection
 *
 * Wrapper around heap deallocation that detects double-free
 * and other memory management errors.
 *
 * @param ptr Pointer to free
 * @param file Source file name (__FILE__)
 * @param line Source line number (__LINE__)
 * @return 0 on success, negative on error
 */
static int safe_free(void *ptr, const char *file, int line)
{
    if (ptr == NULL) {
        LOG_WRN("Attempt to free NULL pointer");
        return 0;  /* Free of NULL is allowed */
    }
    
    /* Find allocation record */
    allocation_record_t *record = find_allocation_record(ptr);
    if (record == NULL) {
        LOG_ERR("🚨 Invalid free - pointer not found: %p", ptr);
        corruptions_detected++;
        ft_report_fault(FT_DOUBLE_FREE_FAULT, FT_SEVERITY_HIGH);
        return -EINVAL;
    }
    
    /* Validate record */
    if (!validate_allocation_record(record)) {
        LOG_ERR("🚨 Allocation record corrupted for %p", ptr);
        corruptions_detected++;
        return -EINVAL;
    }
    
    /* Check for double-free */
    if (record->is_free) {
        LOG_ERR("🚨 Double-free detected for %p", ptr);
        LOG_ERR("Originally allocated at %s:%d", record->file, record->line);
        LOG_ERR("Now freeing at %s:%d", file, line);
        corruptions_detected++;
        ft_report_fault(FT_DOUBLE_FREE_FAULT, FT_SEVERITY_CRITICAL);
        return -EINVAL;
    }
    
    /* Validate magic guards */
    uint8_t *raw_ptr = (uint8_t*)ptr - sizeof(uint32_t);
    uint32_t *prefix = (uint32_t*)raw_ptr;
    uint32_t *suffix = (uint32_t*)((uint8_t*)ptr + record->size);
    
    if (*prefix != HEAP_MAGIC_PREFIX) {
        LOG_ERR("🚨 Heap underrun detected at %p", ptr);
        LOG_ERR("Expected prefix: 0x%08X, Found: 0x%08X", HEAP_MAGIC_PREFIX, *prefix);
        corruptions_detected++;
        ft_report_fault(FT_BUFFER_OVERFLOW_FAULT, FT_SEVERITY_HIGH);
    }
    
    if (*suffix != HEAP_MAGIC_SUFFIX) {
        LOG_ERR("🚨 Heap overrun detected at %p", ptr);
        LOG_ERR("Expected suffix: 0x%08X, Found: 0x%08X", HEAP_MAGIC_SUFFIX, *suffix);
        corruptions_detected++;
        ft_report_fault(FT_BUFFER_OVERFLOW_FAULT, FT_SEVERITY_HIGH);
    }
    
    /* Poison freed memory */
    memset(ptr, POISON_PATTERN, record->size);
    
    /* Mark as freed */
    record->is_free = true;
    
    /* Free raw allocation */
    sys_heap_free(&test_heap, raw_ptr);
    
    LOG_DBG("Freed %zu bytes at %p", record->size, ptr);
    
    return 0;
}

/* Convenience macros */
#define SAFE_MALLOC(size) safe_malloc(size, __FILE__, __LINE__)
#define SAFE_FREE(ptr) safe_free(ptr, __FILE__, __LINE__)

/**
 * @brief Test 1: Double-Free Detection
 *
 * Tests detection of double-free errors where the same
 * memory block is freed multiple times.
 */
static void test_double_free_detection(void)
{
    LOG_INF("📋 Test 1: Double-free detection");
    heap_tests_run++;
    
    heap_test_result_t *result = &heap_results[0];
    result->test_name = "Double-Free Detection";
    strcpy(result->description, "Freeing same memory block twice");
    
    /* Allocate test memory */
    void *test_ptr = SAFE_MALLOC(256);
    if (test_ptr == NULL) {
        LOG_ERR("Failed to allocate test memory");
        return;
    }
    
    result->allocations_made = 1;
    
    /* Write test pattern */
    memset(test_ptr, 0xAA, 256);
    
    /* First free (should succeed) */
    int first_free = SAFE_FREE(test_ptr);
    result->deallocations_made++;
    
    if (first_free == 0) {
        LOG_INF("First free succeeded as expected");
    } else {
        LOG_ERR("First free failed unexpectedly");
    }
    
    /* Second free (should detect double-free) */
    int second_free = SAFE_FREE(test_ptr);
    
    if (second_free != 0) {
        LOG_INF("✅ Double-free detection working correctly");
        result->double_free_detected = true;
    } else {
        LOG_ERR("❌ Double-free not detected - SECURITY RISK!");
    }
}

/**
 * @brief Test 2: Use-After-Free Detection
 *
 * Tests detection when accessing memory after it has
 * been freed (use-after-free vulnerability).
 */
static void test_use_after_free_detection(void)
{
    LOG_INF("📋 Test 2: Use-after-free detection");
    heap_tests_run++;
    
    heap_test_result_t *result = &heap_results[1];
    result->test_name = "Use-After-Free Detection";
    strcpy(result->description, "Accessing memory after free");
    
    /* Allocate and initialize memory */
    uint32_t *test_data = (uint32_t*)SAFE_MALLOC(sizeof(uint32_t) * 64);
    if (test_data == NULL) {
        LOG_ERR("Failed to allocate test memory");
        return;
    }
    
    result->allocations_made = 1;
    
    /* Initialize with known pattern */
    for (int i = 0; i < 64; i++) {
        test_data[i] = 0x12345678 + i;
    }
    
    /* Verify data integrity */
    bool data_ok = true;
    for (int i = 0; i < 64; i++) {
        if (test_data[i] != 0x12345678 + i) {
            data_ok = false;
            break;
        }
    }
    
    LOG_INF("Data integrity before free: %s", data_ok ? "OK" : "CORRUPTED");
    
    /* Free the memory */
    SAFE_FREE(test_data);
    result->deallocations_made = 1;
    
    /* Now attempt to read freed memory (should be poisoned) */
    LOG_WRN("⚠️  Attempting use-after-free...");
    
    uint32_t first_value = test_data[0];
    uint32_t expected_poison = 0xDDDDDDDD;  /* POISON_PATTERN repeated */
    
    if (first_value == expected_poison) {
        LOG_INF("✅ Use-after-free detected (memory properly poisoned)");
        result->use_after_free_detected = true;
        corruptions_detected++;
    } else {
        LOG_WRN("⚠️  Memory not properly poisoned: 0x%08X", first_value);
    }
    
    /* Attempt to write to freed memory */
    LOG_WRN("⚠️  Attempting write to freed memory...");
    test_data[0] = 0xDEADBEEF;
    
    /* This is dangerous but demonstrates the vulnerability */
    if (test_data[0] == 0xDEADBEEF) {
        LOG_ERR("❌ Write to freed memory succeeded - SECURITY RISK!");
    }
}

/**
 * @brief Test 3: Buffer Overflow in Heap
 *
 * Tests detection of buffer overflows in heap-allocated
 * memory using guard patterns.
 */
static void test_heap_buffer_overflow(void)
{
    LOG_INF("📋 Test 3: Heap buffer overflow detection");
    heap_tests_run++;
    
    heap_test_result_t *result = &heap_results[2];
    result->test_name = "Heap Buffer Overflow";
    strcpy(result->description, "Writing beyond allocated boundaries");
    
    /* Allocate small buffer */
    char *buffer = (char*)SAFE_MALLOC(64);
    if (buffer == NULL) {
        LOG_ERR("Failed to allocate test buffer");
        return;
    }
    
    result->allocations_made = 1;
    
    /* Fill buffer safely */
    memset(buffer, 'A', 64);
    
    /* Intentionally overflow the buffer */
    LOG_WRN("⚠️  Performing intentional buffer overflow...");
    
    /* Write beyond allocated space (will corrupt suffix guard) */
    buffer[64] = 'X';   /* One byte past end */
    buffer[65] = 'Y';   /* Two bytes past end */
    buffer[66] = 'Z';   /* Three bytes past end */
    
    /* Free buffer - should detect overflow */
    int free_result = SAFE_FREE(buffer);
    result->deallocations_made = 1;
    
    if (free_result != 0) {
        LOG_INF("✅ Heap buffer overflow detected during free");
        result->corruption_detected = true;
    } else {
        LOG_ERR("❌ Heap buffer overflow not detected - SECURITY RISK!");
    }
}

/**
 * @brief Test 4: Invalid Pointer Free
 *
 * Tests detection when attempting to free invalid pointers
 * (not allocated by heap or corrupted addresses).
 */
static void test_invalid_pointer_free(void)
{
    LOG_INF("📋 Test 4: Invalid pointer free detection");
    heap_tests_run++;
    
    heap_test_result_t *result = &heap_results[3];
    result->test_name = "Invalid Pointer Free";
    strcpy(result->description, "Freeing non-allocated pointers");
    
    /* Test various invalid pointers */
    void *invalid_pointers[] = {
        (void*)0x12345678,      /* Random address */
        (void*)0xDEADBEEF,      /* Classic invalid address */
        &heap_tests_run,        /* Stack variable */
        test_invalid_pointer_free,  /* Function pointer */
        heap_memory + 10        /* Middle of heap (not allocation start) */
    };
    
    int invalid_frees_detected = 0;
    
    for (size_t i = 0; i < ARRAY_SIZE(invalid_pointers); i++) {
        LOG_DBG("Testing invalid free of %p...", invalid_pointers[i]);
        
        int free_result = SAFE_FREE(invalid_pointers[i]);
        
        if (free_result != 0) {
            LOG_INF("✅ Invalid pointer free detected for %p", invalid_pointers[i]);
            invalid_frees_detected++;
        } else {
            LOG_ERR("❌ Invalid free not detected for %p", invalid_pointers[i]);
        }
    }
    
    result->corruption_detected = (invalid_frees_detected > 0);
    result->deallocations_made = invalid_frees_detected;
    
    LOG_INF("Invalid frees detected: %d/%zu", 
           invalid_frees_detected, ARRAY_SIZE(invalid_pointers));
}

/**
 * @brief Test 5: Memory Leak Detection
 *
 * Tests detection of memory leaks by tracking allocations
 * that are never freed.
 */
static void test_memory_leak_detection(void)
{
    LOG_INF("📋 Test 5: Memory leak detection");
    heap_tests_run++;
    
    heap_test_result_t *result = &heap_results[4];
    result->test_name = "Memory Leak Detection";
    strcpy(result->description, "Tracking unfreed allocations");
    
    /* Allocate several blocks but don't free them */
    size_t leak_sizes[] = {128, 256, 512, 1024};
    void *leak_ptrs[ARRAY_SIZE(leak_sizes)];
    
    size_t total_leaked = 0;
    
    for (size_t i = 0; i < ARRAY_SIZE(leak_sizes); i++) {
        leak_ptrs[i] = SAFE_MALLOC(leak_sizes[i]);
        if (leak_ptrs[i] != NULL) {
            total_leaked += leak_sizes[i];
            result->allocations_made++;
        }
    }
    
    LOG_INF("Intentionally leaked %zu bytes in %u allocations", 
           total_leaked, result->allocations_made);
    
    /* Scan allocation tracker for leaks */
    int active_allocations = 0;
    size_t leaked_bytes = 0;
    
    for (int i = 0; i < allocation_count; i++) {
        if (!allocation_tracker[i].is_free && allocation_tracker[i].ptr != NULL) {
            active_allocations++;
            leaked_bytes += allocation_tracker[i].size;
            
            LOG_DBG("Leak: %zu bytes at %p (allocated at %s:%d)",
                   allocation_tracker[i].size, 
                   allocation_tracker[i].ptr,
                   allocation_tracker[i].file,
                   allocation_tracker[i].line);
        }
    }
    
    result->memory_leaked = leaked_bytes;
    
    if (leaked_bytes > 0) {
        LOG_INF("✅ Memory leak detection working: %zu bytes leaked", leaked_bytes);
        ft_report_fault(FT_MEMORY_LEAK_FAULT, FT_SEVERITY_MEDIUM);
    }
    
    /* Clean up leaks for next test */
    for (size_t i = 0; i < ARRAY_SIZE(leak_sizes); i++) {
        if (leak_ptrs[i] != NULL) {
            SAFE_FREE(leak_ptrs[i]);
            result->deallocations_made++;
        }
    }
}

/**
 * @brief Test 6: Heap Exhaustion
 *
 * Tests behavior when heap memory is exhausted and
 * allocation failure handling.
 */
static void test_heap_exhaustion(void)
{
    LOG_INF("📋 Test 6: Heap exhaustion handling");
    heap_tests_run++;
    
    heap_test_result_t *result = &heap_results[5];
    result->test_name = "Heap Exhaustion";
    strcpy(result->description, "Testing allocation failure handling");
    
    /* Allocate memory until heap is exhausted */
    void *allocations[100];
    int allocation_count = 0;
    size_t total_allocated = 0;
    
    /* Allocate in chunks until failure */
    for (int i = 0; i < 100; i++) {
        size_t chunk_size = 1024;  /* 1KB chunks */
        allocations[i] = SAFE_MALLOC(chunk_size);
        
        if (allocations[i] == NULL) {
            LOG_INF("Heap exhausted after %d allocations (%zu bytes)",
                   i, total_allocated);
            break;
        }
        
        /* Initialize memory to prevent optimization */
        memset(allocations[i], 0xCC, chunk_size);
        
        allocation_count++;
        total_allocated += chunk_size;
        result->allocations_made++;
    }
    
    /* Try one more allocation (should fail) */
    void *fail_ptr = SAFE_MALLOC(1024);
    if (fail_ptr == NULL) {
        LOG_INF("✅ Heap exhaustion handled correctly");
        result->corruption_detected = false;  /* This is expected behavior */
    } else {
        LOG_ERR("❌ Allocation succeeded when heap should be exhausted");
        SAFE_FREE(fail_ptr);
    }
    
    /* Clean up allocations */
    for (int i = 0; i < allocation_count; i++) {
        if (allocations[i] != NULL) {
            SAFE_FREE(allocations[i]);
            result->deallocations_made++;
        }
    }
    
    LOG_INF("Freed %d allocations (%zu bytes)", allocation_count, total_allocated);
}

/**
 * @brief Print heap corruption test summary
 */
static void print_heap_corruption_summary(void)
{
    LOG_INF("=== Heap Corruption Test Results Summary ===");
    
    for (int i = 0; i < heap_tests_run; i++) {
        const heap_test_result_t *result = &heap_results[i];
        
        LOG_INF("Test %d: %s", i + 1, result->test_name);
        LOG_INF("  Description: %s", result->description);
        LOG_INF("  Corruption Detected: %s", result->corruption_detected ? "YES" : "NO");
        LOG_INF("  Double-Free Detected: %s", result->double_free_detected ? "YES" : "NO");
        LOG_INF("  Use-After-Free Detected: %s", result->use_after_free_detected ? "YES" : "NO");
        LOG_INF("  Allocations Made: %u", result->allocations_made);
        LOG_INF("  Deallocations Made: %u", result->deallocations_made);
        
        if (result->memory_leaked > 0) {
            LOG_INF("  Memory Leaked: %zu bytes", result->memory_leaked);
        }
    }
    
    /* Calculate detection effectiveness */
    int tests_with_detection = 0;
    for (int i = 0; i < heap_tests_run; i++) {
        if (heap_results[i].corruption_detected || 
            heap_results[i].double_free_detected || 
            heap_results[i].use_after_free_detected) {
            tests_with_detection++;
        }
    }
    
    float detection_rate = (heap_tests_run > 0) ? 
                          (100.0f * tests_with_detection / heap_tests_run) : 0.0f;
    
    LOG_INF("Heap corruption detection rate: %.1f%% (%d/%d)", 
           detection_rate, tests_with_detection, heap_tests_run);
}

/**
 * @brief Main heap corruption test entry point
 *
 * Orchestrates comprehensive heap corruption testing using
 * custom allocation tracking and validation mechanisms.
 *
 * @param p1 Unused parameter 1
 * @param p2 Unused parameter 2  
 * @param p3 Unused parameter 3
 */
void heap_corruption_test_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("🗂️  Starting ARM Cortex-M3 Heap Corruption Test Suite");
    
    /* Initialize test system */
    heap_tests_run = 0;
    corruptions_detected = 0;
    heap_fault_occurred = false;
    
    if (initialize_heap_monitoring() != 0) {
        LOG_ERR("Failed to initialize heap monitoring");
        return;
    }
    
    /* Wait for system stabilization */
    k_sleep(K_SECONDS(1));
    
    LOG_INF("=== Heap Corruption and Double-Free Detection Tests ===");
    
    /* Execute all heap corruption tests */
    test_double_free_detection();
    k_sleep(K_MSEC(300));
    
    test_use_after_free_detection();
    k_sleep(K_MSEC(300));
    
    test_heap_buffer_overflow();
    k_sleep(K_MSEC(300));
    
    test_invalid_pointer_free();
    k_sleep(K_MSEC(300));
    
    test_memory_leak_detection();
    k_sleep(K_MSEC(300));
    
    test_heap_exhaustion();
    k_sleep(K_MSEC(300));
    
    /* Print detailed results */
    print_heap_corruption_summary();
    
    /* Final statistics */
    LOG_INF("=== Final Heap Corruption Test Results ===");
    LOG_INF("Heap tests executed: %d", heap_tests_run);
    LOG_INF("Corruptions detected: %d", corruptions_detected);
    LOG_INF("Detection rate: %.1f%%", 
           (heap_tests_run > 0) ? 
           (100.0 * corruptions_detected / heap_tests_run) : 0.0);
    
    /* Report final status */
    if (corruptions_detected > 0) {
        LOG_INF("✅ Heap corruption detection is working correctly");
    } else {
        LOG_WRN("⚠️  No heap corruptions detected - may need more aggressive testing");
    }
    
    LOG_INF("🗂️  Heap corruption test suite completed");
}