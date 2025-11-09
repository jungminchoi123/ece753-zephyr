/**
 * @file fault_tolerance.h
 * @brief Main Fault Tolerance Framework Header
 * @author Jack Ostapeic
 * 
 * This header provides a convenient include for all fault tolerance
 * framework functionality. Include this header to access the complete
 * fault tolerance API.
 */

#ifndef ZEPHYR_INCLUDE_FAULT_TOLERANCE_H_
#define ZEPHYR_INCLUDE_FAULT_TOLERANCE_H_

/* Core fault tolerance framework */
#include <zephyr/fault_tolerance/ft_core.h>

/* Thread recovery module */
#ifdef CONFIG_FT_ENABLE_THREAD_RECOVERY
#include <zephyr/fault_tolerance/ft_thread_recovery.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the complete fault tolerance framework
 * 
 * This function initializes all enabled fault tolerance modules
 * in the correct order.
 * 
 * @param config Framework configuration (NULL for defaults)
 * @return 0 on success, negative error code on failure
 */
int fault_tolerance_init(const struct ft_config *config);

/**
 * @brief Shutdown the fault tolerance framework
 * 
 * @return 0 on success, negative error code on failure
 */
int fault_tolerance_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_FAULT_TOLERANCE_H_ */