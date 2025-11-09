/**
 * @file ft_buffer_overflow.h
 * @brief Buffer Overflow Protection Module
 * @author Jack Ostapeic
 * 
 * This header provides buffer overflow detection and mitigation capabilities
 * for the Zephyr Fault Tolerance Framework.
 */

#ifndef ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_BUFFER_OVERFLOW_H_
#define ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_BUFFER_OVERFLOW_H_

#include <zephyr/kernel.h>
#include <zephyr/fault_tolerance/ft_core.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_FT_ENABLE_BUFFER_OVERFLOW_PROTECTION

/** @brief Protected buffer structure */
struct ft_protected_buffer {
    void *data;
    size_t size;
    uint32_t canary_before;
    uint32_t canary_after;
    uint32_t checksum;
    uint64_t last_check;
    uint32_t corruption_count;
    bool is_protected;
    const char *name;
};

/** @brief Buffer overflow protection statistics */
struct ft_buffer_overflow_stats {
    uint32_t buffers_protected;
    uint32_t integrity_checks;
    uint32_t violations_detected;
    uint32_t repairs_performed;
    uint32_t false_positives;
};

/**
 * @brief Initialize buffer overflow protection module
 * 
 * @return 0 on success, negative error code on failure
 */
int ft_buffer_overflow_init(void);

/**
 * @brief Shutdown buffer overflow protection module
 * 
 * @return 0 on success, negative error code on failure
 */
int ft_buffer_overflow_shutdown(void);

/**
 * @brief Create a protected buffer
 * 
 * @param size Size of the buffer to protect
 * @param name Human-readable name for the buffer
 * @return Pointer to protected buffer structure, NULL on failure
 */
struct ft_protected_buffer *ft_buffer_overflow_create_protected(size_t size, const char *name);

/**
 * @brief Destroy a protected buffer
 * 
 * @param buffer Buffer to destroy
 * @return 0 on success, negative error code on failure
 */
int ft_buffer_overflow_destroy_protected(struct ft_protected_buffer *buffer);

/**
 * @brief Safely copy data to protected buffer
 * 
 * @param buffer Protected buffer to copy to
 * @param src Source data
 * @param len Length of data to copy
 * @return 0 on success, negative error code on failure
 */
int ft_buffer_overflow_safe_copy(struct ft_protected_buffer *buffer, const void *src, size_t len);

/**
 * @brief Check integrity of a protected buffer
 * 
 * @param buffer Buffer to check
 * @return 0 if intact, negative error code if corrupted
 */
int ft_buffer_overflow_check_integrity(struct ft_protected_buffer *buffer);

/**
 * @brief Force check of all protected buffers
 * 
 * @return Number of violations detected
 */
uint32_t ft_buffer_overflow_force_check(void);

/**
 * @brief Protect an existing buffer
 * 
 * @param buffer Buffer to protect
 * @param size Size of the buffer
 * @param name Human-readable name for the buffer
 * @return 0 on success, negative error code on failure
 */
int ft_buffer_overflow_protect(struct ft_protected_buffer *buffer, size_t size, const char *name);

/**
 * @brief Unprotect a buffer and remove monitoring
 * 
 * @param buffer Buffer to unprotect
 * @return 0 on success, negative error code on failure
 */
int ft_buffer_overflow_unprotect(struct ft_protected_buffer *buffer);

/**
 * @brief Check buffer integrity
 * 
 * @param buffer Buffer to check
 * @return 0 if intact, negative error code if corrupted
 */
int ft_buffer_overflow_check(struct ft_protected_buffer *buffer);

/**
 * @brief Repair a corrupted buffer
 * 
 * @param buffer Buffer to repair
 * @return 0 on success, negative error code on failure
 */
int ft_buffer_overflow_repair(struct ft_protected_buffer *buffer);

/**
 * @brief Check all protected buffers
 * 
 * @return Number of corrupted buffers found
 */
uint32_t ft_buffer_overflow_check_all(void);

/**
 * @brief Get buffer overflow statistics
 * 
 * @param stats Output buffer for statistics
 * @return 0 on success, negative error code on failure
 */
int ft_buffer_overflow_get_stats(struct ft_buffer_overflow_stats *stats);

/**
 * @brief Update buffer checksum after legitimate modification
 * 
 * @param buffer Buffer that was modified
 * @return 0 on success, negative error code on failure
 */
int ft_buffer_overflow_update_checksum(struct ft_protected_buffer *buffer);

/**
 * @brief Safe memory copy with overflow protection
 * 
 * @param buffer Protected destination buffer
 * @param src Source data
 * @param src_size Size of source data
 * @return 0 on success, negative error code on failure
 */
int ft_buffer_overflow_safe_copy(struct ft_protected_buffer *buffer, 
                                const void *src, size_t src_size);

#else /* CONFIG_FT_ENABLE_BUFFER_OVERFLOW_PROTECTION */

/* Stub implementations when module is disabled */
static inline int ft_buffer_overflow_init(void) { return 0; }
static inline int ft_buffer_overflow_shutdown(void) { return 0; }
static inline uint32_t ft_buffer_overflow_check_all(void) { return 0; }

#endif /* CONFIG_FT_ENABLE_BUFFER_OVERFLOW_PROTECTION */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_FAULT_TOLERANCE_FT_BUFFER_OVERFLOW_H_ */