/**
 * @file ft_core.c
 * @brief Core Fault Tolerance Framework Implementation
 * @author Jack Ostapeic
 */

#include <zephyr/fault_tolerance/ft_core.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/mutex.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/kernel.h>
#include <string.h>

LOG_MODULE_REGISTER(ft_core, CONFIG_FT_LOG_LEVEL);

/* Framework state */
static bool ft_initialized = false;
static struct k_mutex ft_mutex;
static struct ft_config ft_current_config;
static struct ft_stats ft_statistics;

/* Handler management */
static sys_slist_t ft_handlers = SYS_SLIST_STATIC_INIT(&ft_handlers);

/* Recovery thread and work queue */
static K_THREAD_STACK_DEFINE(ft_recovery_stack, CONFIG_FT_RECOVERY_STACK_SIZE);
static struct k_work_q ft_recovery_workq;
static ft_recovery_handler_t custom_recovery_handler = NULL;

/* Fault type names for debugging */
static const char *fault_type_names[] = {
    "STACK_OVERFLOW",
    "MEMORY_CORRUPTION", 
    "DEADLOCK",
    "HARDWARE_ERROR",
    "TIMEOUT",
    "ASSERTION_FAILED",
    "WATCHDOG_TIMEOUT",
    "THREAD_EXCEPTION",
    "RESOURCE_EXHAUSTION",
    "CUSTOM"
};

static const char *severity_names[] = {
    "INFO", "WARNING", "ERROR", "CRITICAL", "FATAL"
};

/* Recovery work item */
struct ft_recovery_work {
    struct k_work work;
    struct ft_fault_context fault_ctx;
    struct ft_recovery_context recovery_ctx;
    ft_recovery_handler_t handler;
};

/* Forward declarations */
static void ft_recovery_worker(struct k_work *work);
static int ft_execute_recovery(struct ft_recovery_context *recovery_ctx);
static enum ft_handler_result ft_call_handlers(const struct ft_fault_context *fault_ctx,
                                               struct ft_recovery_context *recovery_ctx);

const char *ft_get_fault_type_name(enum ft_fault_type fault_type)
{
    if (fault_type >= FT_FAULT_TYPE_COUNT) {
        return "UNKNOWN";
    }
    return fault_type_names[fault_type];
}

const char *ft_get_severity_name(enum ft_severity severity)
{
    if (severity > FT_SEVERITY_FATAL) {
        return "UNKNOWN";
    }
    return severity_names[severity];
}

int ft_init(const struct ft_config *config)
{
    if (ft_initialized) {
        return -EALREADY;
    }

    k_mutex_init(&ft_mutex);
    
    /* Initialize default configuration */
    if (config != NULL) {
        ft_current_config = *config;
    } else {
        ft_current_config = (struct ft_config) {
            .enable_logging = true,
            .enable_statistics = true,
            .enable_async_recovery = true,
            .max_recovery_attempts = 3,
            .recovery_timeout_ms = 5000
        };
    }

    /* Initialize statistics */
    memset(&ft_statistics, 0, sizeof(ft_statistics));

    /* Start recovery work queue */
    k_work_queue_start(&ft_recovery_workq, ft_recovery_stack,
                      K_THREAD_STACK_SIZEOF(ft_recovery_stack),
                      CONFIG_FT_RECOVERY_THREAD_PRIORITY, NULL);
    
    k_thread_name_set(&ft_recovery_workq.thread, "ft_recovery");

    ft_initialized = true;
    
    LOG_INF("Fault Tolerance Framework initialized");
    if (ft_current_config.enable_logging) {
        LOG_INF("  - Logging: enabled");
    }
    if (ft_current_config.enable_statistics) {
        LOG_INF("  - Statistics: enabled");
    }
    if (ft_current_config.enable_async_recovery) {
        LOG_INF("  - Async recovery: enabled");
    }

    return 0;
}

bool ft_is_initialized(void)
{
    return ft_initialized;
}

int ft_register_handler(struct ft_handler *handler)
{
    if (!ft_initialized) {
        return -ENODEV;
    }

    if (handler == NULL || handler->handler == NULL) {
        return -EINVAL;
    }

    if (handler->fault_type >= FT_FAULT_TYPE_COUNT) {
        return -EINVAL;
    }

    k_mutex_lock(&ft_mutex, K_FOREVER);

    /* Insert handler in priority order (lower number = higher priority) */
    struct ft_handler *current, *prev = NULL;
    bool inserted = false;

    SYS_SLIST_FOR_EACH_CONTAINER(&ft_handlers, current, node) {
        if (handler->priority < current->priority) {
            if (prev == NULL) {
                sys_slist_prepend(&ft_handlers, &handler->node);
            } else {
                sys_slist_insert(&ft_handlers, &prev->node, &handler->node);
            }
            inserted = true;
            break;
        }
        prev = current;
    }

    if (!inserted) {
        sys_slist_append(&ft_handlers, &handler->node);
    }

    k_mutex_unlock(&ft_mutex);

    LOG_INF("Registered fault handler: %s (type=%s, priority=%d)",
            handler->name ? handler->name : "unnamed",
            ft_get_fault_type_name(handler->fault_type),
            handler->priority);

    return 0;
}

int ft_unregister_handler(struct ft_handler *handler)
{
    if (!ft_initialized) {
        return -ENODEV;
    }

    if (handler == NULL) {
        return -EINVAL;
    }

    k_mutex_lock(&ft_mutex, K_FOREVER);
    
    bool found = sys_slist_find_and_remove(&ft_handlers, &handler->node);
    
    k_mutex_unlock(&ft_mutex);

    if (!found) {
        return -ENOENT;
    }

    LOG_INF("Unregistered fault handler: %s",
            handler->name ? handler->name : "unnamed");

    return 0;
}

static enum ft_handler_result ft_call_handlers(const struct ft_fault_context *fault_ctx,
                                               struct ft_recovery_context *recovery_ctx)
{
    struct ft_handler *handler;
    enum ft_handler_result result = FT_HANDLER_CONTINUE;

    SYS_SLIST_FOR_EACH_CONTAINER(&ft_handlers, handler, node) {
        /* Skip handlers that don't match this fault type */
        if (handler->fault_type != fault_ctx->fault_type &&
            handler->fault_type != FT_FAULT_CUSTOM) {
            continue;
        }

        LOG_DBG("Calling handler: %s", 
                handler->name ? handler->name : "unnamed");

        result = handler->handler(fault_ctx, recovery_ctx, handler->user_data);

        if (result == FT_HANDLER_ERROR) {
            atomic_inc(&ft_statistics.handler_errors);
            LOG_ERR("Handler error in: %s", 
                    handler->name ? handler->name : "unnamed");
        } else if (result == FT_HANDLER_HANDLED) {
            LOG_DBG("Fault handled by: %s",
                    handler->name ? handler->name : "unnamed");
            break;
        }
    }

    return result;
}

static void ft_recovery_worker(struct k_work *work)
{
    struct ft_recovery_work *recovery_work = 
        CONTAINER_OF(work, struct ft_recovery_work, work);
    
    LOG_INF("Executing recovery action: %d for fault type: %s",
            recovery_work->recovery_ctx.action,
            ft_get_fault_type_name(recovery_work->fault_ctx.fault_type));

    int ret = ft_execute_recovery(&recovery_work->recovery_ctx);
    if (ret == 0) {
        atomic_inc(&ft_statistics.recovered_faults);
        LOG_INF("Recovery successful");
    } else {
        atomic_inc(&ft_statistics.unrecoverable_faults);
        LOG_ERR("Recovery failed: %d", ret);
    }

    k_free(recovery_work);
}

static int ft_execute_recovery(struct ft_recovery_context *recovery_ctx)
{
    switch (recovery_ctx->action) {
    case FT_RECOVERY_RESTART_THREAD:
        if (recovery_ctx->target_thread != NULL) {
            LOG_WRN("Aborting thread %p", recovery_ctx->target_thread);
            k_thread_abort(recovery_ctx->target_thread);
            return 0;
        }
        return -EINVAL;

    case FT_RECOVERY_SYSTEM_REBOOT:
        LOG_ERR("System reboot requested due to critical fault");
        sys_reboot(SYS_REBOOT_COLD);
        return 0;

    case FT_RECOVERY_GRACEFUL_SHUTDOWN:
        LOG_ERR("Graceful shutdown requested");
        /* Implementation depends on system requirements */
        return 0;

    case FT_RECOVERY_CUSTOM:
        if (custom_recovery_handler != NULL) {
            return custom_recovery_handler(recovery_ctx);
        } else if (recovery_ctx->target_thread != NULL) {
            /* Fallback to thread restart */
            k_thread_abort(recovery_ctx->target_thread);
            return 0;
        }
        return -ENOSYS;

    case FT_RECOVERY_NONE:
    default:
        LOG_WRN("No recovery action specified");
        return 0;
    }
}

int ft_report_fault(const struct ft_fault_context *fault_ctx)
{
    if (!ft_initialized) {
        return -ENODEV;
    }

    if (fault_ctx == NULL) {
        return -EINVAL;
    }

    /* Update statistics */
    if (ft_current_config.enable_statistics) {
        atomic_inc(&ft_statistics.total_faults);
        if (fault_ctx->fault_type < FT_FAULT_TYPE_COUNT) {
            atomic_inc(&ft_statistics.fault_counts[fault_ctx->fault_type]);
        }
        ft_statistics.last_fault_timestamp = fault_ctx->timestamp;
        ft_statistics.last_fault_type = fault_ctx->fault_type;
    }

    /* Log the fault */
    if (ft_current_config.enable_logging) {
        LOG_ERR("FAULT REPORTED: Type=%s, Severity=%s, Thread=%p, Code=0x%x, Desc=%s",
                ft_get_fault_type_name(fault_ctx->fault_type),
                ft_get_severity_name(fault_ctx->severity),
                fault_ctx->thread_id,
                fault_ctx->error_code,
                fault_ctx->description ? fault_ctx->description : "none");
    }

    /* Create recovery context */
    struct ft_recovery_context recovery_ctx = {
        .action = FT_RECOVERY_NONE,
        .target_thread = fault_ctx->thread_id,
        .recovery_data = NULL,
        .recovery_data_size = 0,
        .timeout_ms = ft_current_config.recovery_timeout_ms,
        .async_recovery = ft_current_config.enable_async_recovery
    };

    k_mutex_lock(&ft_mutex, K_FOREVER);
    
    /* Call registered handlers */
    enum ft_handler_result result = ft_call_handlers(fault_ctx, &recovery_ctx);
    (void)result; /* Result could be used for further processing */
    
    k_mutex_unlock(&ft_mutex);

    /* Execute recovery if needed */
    if (recovery_ctx.action != FT_RECOVERY_NONE) {
        if (recovery_ctx.async_recovery && ft_current_config.enable_async_recovery) {
            /* Asynchronous recovery */
            struct ft_recovery_work *work = k_malloc(sizeof(*work));
            if (work == NULL) {
                LOG_ERR("Failed to allocate recovery work item");
                return -ENOMEM;
            }

            work->fault_ctx = *fault_ctx;
            work->recovery_ctx = recovery_ctx;
            work->handler = custom_recovery_handler;
            k_work_init(&work->work, ft_recovery_worker);
            k_work_submit_to_queue(&ft_recovery_workq, &work->work);
        } else {
            /* Synchronous recovery */
            int ret = ft_execute_recovery(&recovery_ctx);
            if (ret == 0) {
                atomic_inc(&ft_statistics.recovered_faults);
            } else {
                atomic_inc(&ft_statistics.unrecoverable_faults);
                return ret;
            }
        }
    }

    return 0;
}

int ft_get_stats(struct ft_stats *stats)
{
    if (!ft_initialized) {
        return -ENODEV;
    }

    if (stats == NULL) {
        return -EINVAL;
    }

    if (!ft_current_config.enable_statistics) {
        return -ENOTSUP;
    }

    *stats = ft_statistics;
    return 0;
}

int ft_reset_stats(void)
{
    if (!ft_initialized) {
        return -ENODEV;
    }

    if (!ft_current_config.enable_statistics) {
        return -ENOTSUP;
    }

    memset(&ft_statistics, 0, sizeof(ft_statistics));
    LOG_INF("Fault tolerance statistics reset");
    
    return 0;
}

int ft_set_recovery_handler(ft_recovery_handler_t handler)
{
    if (!ft_initialized) {
        return -ENODEV;
    }

    custom_recovery_handler = handler;
    LOG_INF("Custom recovery handler %s", handler ? "set" : "cleared");
    
    return 0;
}

int ft_configure_fault_type(enum ft_fault_type fault_type, bool enable)
{
    if (!ft_initialized) {
        return -ENODEV;
    }

    if (fault_type >= FT_FAULT_TYPE_COUNT) {
        return -EINVAL;
    }

    /* This could be extended to maintain per-type enable/disable state */
    LOG_INF("Fault type %s %s", 
            ft_get_fault_type_name(fault_type),
            enable ? "enabled" : "disabled");

    return 0;
}