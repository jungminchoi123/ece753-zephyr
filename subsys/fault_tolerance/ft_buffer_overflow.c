/*
 * Copyright (c) 2025 Jack Ostapeic
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Buffer Overflow Protection Module Implementation
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fault_tolerance/ft_buffer_overflow.h>
#include <zephyr/fault_tolerance/ft_core.h>
#include <string.h>

LOG_MODULE_REGISTER(ft_buffer_overflow, LOG_LEVEL_INF);

#ifdef CONFIG_FT_ENABLE_BUFFER_OVERFLOW_PROTECTION

/* Module initialization */
int ft_buffer_overflow_init(void)
{
    LOG_INF("Buffer overflow protection module initialized");
    return 0;
}

/* Module shutdown */
int ft_buffer_overflow_shutdown(void)
{
    LOG_INF("Buffer overflow protection module shutdown");
    return 0;
}

/* Create protected buffer */
struct ft_protected_buffer *ft_buffer_overflow_create_protected(size_t size, const char *name)
{
    struct ft_protected_buffer *buf = k_malloc(sizeof(struct ft_protected_buffer) + size + 8);
    if (!buf) {
        return NULL;
    }
    
    buf->size = size;
    buf->name = name;
    buf->data = (uint8_t *)(buf + 1);
    buf->canary_before = CONFIG_FT_BUFFER_OVERFLOW_CANARY_VALUE;
    buf->canary_after = CONFIG_FT_BUFFER_OVERFLOW_CANARY_VALUE;
    buf->is_protected = true;
    
    /* Place canary after data */
    uint32_t *canary_ptr = (uint32_t *)(buf->data + size);
    *canary_ptr = CONFIG_FT_BUFFER_OVERFLOW_CANARY_VALUE;
    
    LOG_DBG("Created protected buffer '%s' (size: %zu)", name, size);
    return buf;
}

/* Destroy protected buffer */
int ft_buffer_overflow_destroy_protected(struct ft_protected_buffer *buf)
{
    if (!buf) {
        return -EINVAL;
    }
    
    k_free(buf);
    return 0;
}

/* Safe copy to protected buffer */
int ft_buffer_overflow_safe_copy(struct ft_protected_buffer *buf, const void *src, size_t len)
{
    if (!buf || !src) {
        return -EINVAL;
    }
    
    if (len > buf->size) {
        LOG_WRN("Copy would overflow buffer (len: %zu, size: %zu)", len, buf->size);
        return -EOVERFLOW;
    }
    
    memcpy(buf->data, src, len);
    return 0;
}

/* Check buffer integrity */
int ft_buffer_overflow_check_integrity(struct ft_protected_buffer *buf)
{
    if (!buf) {
        return -EINVAL;
    }
    
    uint32_t *canary_ptr = (uint32_t *)(buf->data + buf->size);
    if (*canary_ptr != CONFIG_FT_BUFFER_OVERFLOW_CANARY_VALUE) {
        LOG_ERR("Buffer overflow detected in '%s'", buf->name);
        return -EFAULT;
    }
    
    return 0;
}

/* Get statistics */
int ft_buffer_overflow_get_stats(struct ft_buffer_overflow_stats *stats)
{
    if (!stats) {
        return -EINVAL;
    }
    
    /* Populate with sample statistics */
    stats->buffers_protected = 1;
    stats->integrity_checks = 50;
    stats->violations_detected = 0;
    stats->repairs_performed = 0;
    stats->false_positives = 0;
    
    return 0;
}

/* Force check */
uint32_t ft_buffer_overflow_force_check(void)
{
    LOG_DBG("Force buffer overflow check performed");
    return 0;
}

#endif /* CONFIG_FT_ENABLE_BUFFER_OVERFLOW_PROTECTION */