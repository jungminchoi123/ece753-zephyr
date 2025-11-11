#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance.h>
#include <string.h>

LOG_MODULE_REGISTER(buffer_overflow_test, LOG_LEVEL_INF);

/* Buffer overflow detection using canaries */
#define BUFFER_SIZE 64
#define CANARY_PATTERN 0xDEADBEEF

struct protected_buffer {
    uint32_t pre_canary;
    char data[BUFFER_SIZE];
    uint32_t post_canary;
};

static bool check_buffer_integrity(struct protected_buffer *buf) {
    if (buf->pre_canary != CANARY_PATTERN || buf->post_canary != CANARY_PATTERN) {
        return false;
    }
    return true;
}

static void init_protected_buffer(struct protected_buffer *buf) {
    buf->pre_canary = CANARY_PATTERN;
    buf->post_canary = CANARY_PATTERN;
    memset(buf->data, 0, BUFFER_SIZE);
}

/* Simulate common buffer overflow scenarios */
static void test_strcpy_overflow(void) {
    struct protected_buffer buf;
    init_protected_buffer(&buf);
    
    /* Dangerous: Copy string longer than buffer */
    const char *long_string = "This is a very long string that will definitely overflow the 64-byte buffer and corrupt memory beyond it causing serious security vulnerabilities";
    
    LOG_INF("📝 Testing strcpy overflow (unsafe string copy)");
    LOG_INF("   Buffer size: %d bytes", BUFFER_SIZE);
    LOG_INF("   String length: %d bytes", strlen(long_string));
    
    /* This will overflow! */
    strcpy(buf.data, long_string);
    
    /* Check for corruption */
    if (!check_buffer_integrity(&buf)) {
        LOG_ERR("🚨 BUFFER OVERFLOW DETECTED! Canaries corrupted");
        LOG_ERR("   Pre-canary: 0x%08X (expected: 0x%08X)", 
               buf.pre_canary, CANARY_PATTERN);
        LOG_ERR("   Post-canary: 0x%08X (expected: 0x%08X)", 
               buf.post_canary, CANARY_PATTERN);
        
        ft_report_fault(FT_BUFFER_OVERFLOW_FAULT, FT_SEVERITY_HIGH);
    } else {
        LOG_INF("✅ Buffer integrity maintained");
    }
}

static void test_array_bounds_overflow(void) {
    struct protected_buffer buf;
    init_protected_buffer(&buf);
    
    LOG_INF("📝 Testing array bounds overflow");
    
    /* Write beyond array bounds */
    for (int i = 0; i <= BUFFER_SIZE + 10; i++) {
        buf.data[i] = (char)(i % 256);
        
        /* Check integrity periodically */
        if (i > BUFFER_SIZE && !check_buffer_integrity(&buf)) {
            LOG_ERR("🚨 BUFFER OVERFLOW DETECTED at index %d!", i);
            ft_report_fault(FT_BUFFER_OVERFLOW_FAULT, FT_SEVERITY_HIGH);
            break;
        }
    }
}

static void test_stack_buffer_overflow(void) {
    /* Stack buffer without protection (more dangerous) */
    char stack_buffer[32];
    
    LOG_INF("📝 Testing stack buffer overflow (no canaries)");
    
    /* This will corrupt the stack */
    const char *overflow_data = "This string is much too long for the 32 byte stack buffer and will corrupt the stack frame";
    
    LOG_WRN("⚠️  Performing dangerous stack overflow...");
    strcpy(stack_buffer, overflow_data);
    
    /* If we reach here, stack wasn't corrupted enough to crash immediately */
    LOG_INF("Stack buffer contains: %.30s...", stack_buffer);
    
    /* Report the fault */
    ft_report_fault(FT_BUFFER_OVERFLOW_FAULT, FT_SEVERITY_CRITICAL);
}

void buffer_overflow_test_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("🛡️  Starting buffer overflow detection test");
    
    k_sleep(K_SECONDS(1));
    
    /* Test 1: String copy overflow with canary protection */
    test_strcpy_overflow();
    k_sleep(K_MSEC(500));
    
    /* Test 2: Array bounds overflow */
    test_array_bounds_overflow();
    k_sleep(K_MSEC(500));
    
    /* Test 3: Stack buffer overflow (most dangerous) */
    test_stack_buffer_overflow();
    
    LOG_INF("✅ Buffer overflow test completed");
    LOG_INF("🛡️  Buffer overflow detection test finished");
}