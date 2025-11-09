/*
 * Copyright (c) 2025 Jack Ostapeic
 *
 * Buffer Overflow Protection Test Module
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance/ft_buffer_overflow.h>
#include <string.h>

LOG_MODULE_REGISTER(buffer_overflow_test, LOG_LEVEL_INF);

/* Test the buffer overflow protection module */
int buffer_overflow_test(void)
{
    int ret;
    struct ft_buffer_overflow_stats stats;
    struct ft_protected_buffer *protected_buf;
    char test_data[] = "Hello, Fault Tolerance!";
    char read_buffer[64];

    LOG_INF("=== Buffer Overflow Protection Test ===");

    /* Create a protected buffer */
    protected_buf = ft_buffer_overflow_create_protected(32, "test_buffer");
    if (!protected_buf) {
        LOG_ERR("Failed to create protected buffer");
        return -ENOMEM;
    }

    LOG_INF("Created protected buffer (size: %zu)", protected_buf->size);

    /* Test normal operations */
    ret = ft_buffer_overflow_safe_copy(protected_buf, test_data, strlen(test_data));
    if (ret < 0) {
        LOG_ERR("Safe copy failed: %d", ret);
        goto cleanup;
    }
    LOG_INF("Safe copy successful");

    /* Verify buffer integrity */
    ret = ft_buffer_overflow_check_integrity(protected_buf);
    if (ret < 0) {
        LOG_ERR("Buffer integrity check failed: %d", ret);
        goto cleanup;
    }
    LOG_INF("Buffer integrity verified");

    /* Test reading from protected buffer */
    memset(read_buffer, 0, sizeof(read_buffer));
    memcpy(read_buffer, protected_buf->data, strlen(test_data));
    LOG_INF("Read from protected buffer: '%s'", read_buffer);

    /* Test boundary conditions */
    char large_data[64];
    memset(large_data, 'X', sizeof(large_data) - 1);
    large_data[sizeof(large_data) - 1] = '\0';

    LOG_INF("Testing overflow protection with oversized data...");
    ret = ft_buffer_overflow_safe_copy(protected_buf, large_data, sizeof(large_data));
    if (ret == -EOVERFLOW) {
        LOG_INF("Overflow correctly detected and prevented");
    } else {
        LOG_ERR("Expected overflow detection failed");
        ret = -EFAULT;
        goto cleanup;
    }

    /* Force integrity check */
    ret = ft_buffer_overflow_force_check();
    LOG_INF("Forced integrity check found %d violations", ret);

    /* Get statistics */
    ret = ft_buffer_overflow_get_stats(&stats);
    if (ret == 0) {
        LOG_INF("Buffer overflow statistics:");
        LOG_INF("  Buffers protected: %u", stats.buffers_protected);
        LOG_INF("  Integrity checks: %u", stats.integrity_checks);
        LOG_INF("  Violations detected: %u", stats.violations_detected);
        LOG_INF("  Repairs performed: %u", stats.repairs_performed);
        LOG_INF("  False positives: %u", stats.false_positives);
    }

    ret = 0;

cleanup:
    /* Clean up */
    if (protected_buf) {
        ft_buffer_overflow_destroy_protected(protected_buf);
    }

    LOG_INF("Buffer overflow protection test completed");
    return ret;
}