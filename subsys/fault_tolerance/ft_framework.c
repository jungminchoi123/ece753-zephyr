/**
 * @file ft_framework.c
 * @brief Main Fault Tolerance Framework Implementation
 * @author Jack Ostapeic
 */

#include <zephyr/fault_tolerance.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ft_framework, CONFIG_FT_LOG_LEVEL);

int fault_tolerance_init(const struct ft_config *config)
{
    int ret;

    LOG_INF("Initializing Fault Tolerance Framework");

    /* Initialize core framework */
    ret = ft_init(config);
    if (ret != 0) {
        LOG_ERR("Failed to initialize core framework: %d", ret);
        return ret;
    }

#ifdef CONFIG_FT_ENABLE_THREAD_RECOVERY
    /* Initialize thread recovery module */
    ret = ft_thread_recovery_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize thread recovery: %d", ret);
        return ret;
    }
    LOG_INF("Thread recovery module initialized");
#endif

    LOG_INF("Fault Tolerance Framework fully initialized");
    return 0;
}

int fault_tolerance_shutdown(void)
{
    LOG_INF("Shutting down Fault Tolerance Framework");
    /* Add cleanup code here if needed */
    return 0;
}