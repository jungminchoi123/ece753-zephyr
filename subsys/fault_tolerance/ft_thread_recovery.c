/**
 * @file ft_thread_recovery.c
 * @brief Thread Recovery Module Implementation
 * @author Jack Ostapeic
 */

#include <zephyr/fault_tolerance/ft_thread_recovery.h>
#include <zephyr/fault_tolerance/ft_core.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/mutex.h>
#include <string.h>

LOG_MODULE_REGISTER(ft_thread_recovery, CONFIG_FT_LOG_LEVEL);

/* Thread registry */
static sys_slist_t thread_registry = SYS_SLIST_STATIC_INIT(&thread_registry);
static struct k_mutex registry_mutex;
static bool thread_recovery_initialized = false;

/* Memory pool for thread stacks */
K_MEM_SLAB_DEFINE(thread_stack_pool, CONFIG_FT_THREAD_RECOVERY_STACK_SIZE, 
                  CONFIG_FT_MAX_RECOVERY_ATTEMPTS, 4);

/* Recovery work queue items */
struct ft_thread_recovery_work {
    struct k_work work;
    struct ft_thread_entry *entry;
    k_tid_t faulted_thread;
};

/* Forward declarations */
static struct ft_thread_entry *find_thread_entry(k_tid_t thread_id);
static void thread_recovery_worker(struct k_work *work);
static enum ft_handler_result thread_fault_handler(
    const struct ft_fault_context *fault_ctx,
    struct ft_recovery_context *recovery_ctx,
    void *user_data);

/* Fault handler registration */
static struct ft_handler thread_handler = {
    .fault_type = FT_FAULT_STACK_OVERFLOW,
    .handler = thread_fault_handler,
    .priority = 10,
    .name = "thread_recovery"
};

int ft_thread_recovery_init(void)
{
    if (thread_recovery_initialized) {
        return -EALREADY;
    }

    k_mutex_init(&registry_mutex);

    /* Register fault handlers for thread-related faults */
    int ret = ft_register_handler(&thread_handler);
    if (ret != 0) {
        LOG_ERR("Failed to register thread fault handler: %d", ret);
        return ret;
    }

    thread_recovery_initialized = true;
    LOG_INF("Thread recovery module initialized");

    return 0;
}

static struct ft_thread_entry *find_thread_entry(k_tid_t thread_id)
{
    struct ft_thread_entry *entry;

    SYS_SLIST_FOR_EACH_CONTAINER(&thread_registry, entry, node) {
        if (entry->thread_id == thread_id) {
            return entry;
        }
    }

    return NULL;
}

int ft_thread_register(k_tid_t thread_id, k_thread_entry_t entry_point,
                      void *p1, void *p2, void *p3,
                      size_t stack_size, int priority, uint32_t options,
                      const char *name)
{
    if (!thread_recovery_initialized) {
        return -ENODEV;
    }

    if (thread_id == NULL || entry_point == NULL) {
        return -EINVAL;
    }

    struct ft_thread_entry *entry = k_malloc(sizeof(*entry));
    if (entry == NULL) {
        return -ENOMEM;
    }

    k_mutex_lock(&registry_mutex, K_FOREVER);

    /* Check if thread is already registered */
    if (find_thread_entry(thread_id) != NULL) {
        k_mutex_unlock(&registry_mutex);
        k_free(entry);
        return -EALREADY;
    }

    /* Initialize entry */
    entry->thread_id = thread_id;
    entry->info.entry_point = entry_point;
    entry->info.p1 = p1;
    entry->info.p2 = p2;
    entry->info.p3 = p3;
    entry->info.stack_size = stack_size > 0 ? stack_size : CONFIG_FT_THREAD_RECOVERY_STACK_SIZE;
    entry->info.priority = priority;
    entry->info.options = options;
    entry->info.delay = K_NO_WAIT;
    entry->info.name = name;
    entry->info.max_restarts = CONFIG_FT_MAX_RECOVERY_ATTEMPTS;
    entry->info.restart_count = 0;
    entry->auto_recovery = true;

    sys_slist_append(&thread_registry, &entry->node);

    k_mutex_unlock(&registry_mutex);

    LOG_INF("Registered thread %p (%s) for fault tolerance", 
            thread_id, name ? name : "unnamed");

    return 0;
}

int ft_thread_unregister(k_tid_t thread_id)
{
    if (!thread_recovery_initialized) {
        return -ENODEV;
    }

    if (thread_id == NULL) {
        return -EINVAL;
    }

    k_mutex_lock(&registry_mutex, K_FOREVER);

    struct ft_thread_entry *entry = find_thread_entry(thread_id);
    if (entry == NULL) {
        k_mutex_unlock(&registry_mutex);
        return -ENOENT;
    }

    sys_slist_find_and_remove(&thread_registry, &entry->node);

    k_mutex_unlock(&registry_mutex);

    LOG_INF("Unregistered thread %p (%s) from fault tolerance", 
            thread_id, entry->info.name ? entry->info.name : "unnamed");

    k_free(entry);

    return 0;
}

static void thread_recovery_worker(struct k_work *work)
{
    struct ft_thread_recovery_work *recovery_work = 
        CONTAINER_OF(work, struct ft_thread_recovery_work, work);
    
    struct ft_thread_entry *entry = recovery_work->entry;
    k_tid_t faulted_thread = recovery_work->faulted_thread;

    LOG_INF("Starting thread recovery for %s (attempt %d/%d)",
            entry->info.name ? entry->info.name : "unnamed",
            entry->info.restart_count + 1, entry->info.max_restarts);

    /* Abort the faulted thread */
    if (faulted_thread != NULL) {
        k_thread_abort(faulted_thread);
    }

    /* Allocate new stack */
    void *stack_ptr = NULL;
    int ret = k_mem_slab_alloc(&thread_stack_pool, &stack_ptr, K_NO_WAIT);
    if (ret != 0) {
        LOG_ERR("Failed to allocate stack for thread recovery: %d", ret);
        goto cleanup;
    }

    /* Create new thread structure */
    struct k_thread *new_thread = k_malloc(sizeof(struct k_thread));
    if (new_thread == NULL) {
        LOG_ERR("Failed to allocate thread structure");
        k_mem_slab_free(&thread_stack_pool, &stack_ptr);
        goto cleanup;
    }

    /* Create the new thread */
    k_tid_t new_tid = k_thread_create(
        new_thread,
        stack_ptr,
        entry->info.stack_size,
        entry->info.entry_point,
        entry->info.p1,
        entry->info.p2,
        entry->info.p3,
        entry->info.priority,
        entry->info.options,
        entry->info.delay
    );

    if (new_tid == NULL) {
        LOG_ERR("Failed to create recovery thread");
        k_free(new_thread);
        k_mem_slab_free(&thread_stack_pool, &stack_ptr);
        goto cleanup;
    }

    /* Set thread name */
    if (entry->info.name != NULL) {
        k_thread_name_set(new_tid, entry->info.name);
    }

    /* Update registry */
    k_mutex_lock(&registry_mutex, K_FOREVER);
    entry->thread_id = new_tid;
    entry->info.restart_count++;
    k_mutex_unlock(&registry_mutex);

    LOG_INF("Successfully recovered thread %p (%s)", 
            new_tid, entry->info.name ? entry->info.name : "unnamed");

cleanup:
    k_free(recovery_work);
}

int ft_thread_recover(k_tid_t thread_id, k_tid_t *new_thread_id)
{
    if (!thread_recovery_initialized) {
        return -ENODEV;
    }

    if (thread_id == NULL) {
        return -EINVAL;
    }

    k_mutex_lock(&registry_mutex, K_FOREVER);

    struct ft_thread_entry *entry = find_thread_entry(thread_id);
    if (entry == NULL) {
        k_mutex_unlock(&registry_mutex);
        return -ENOENT;
    }

    if (entry->info.restart_count >= entry->info.max_restarts) {
        k_mutex_unlock(&registry_mutex);
        LOG_ERR("Thread %p exceeded maximum restart attempts (%d)", 
                thread_id, entry->info.max_restarts);
        return -EAGAIN;
    }

    k_mutex_unlock(&registry_mutex);

    /* Schedule recovery work */
    struct ft_thread_recovery_work *work = k_malloc(sizeof(*work));
    if (work == NULL) {
        return -ENOMEM;
    }

    work->entry = entry;
    work->faulted_thread = thread_id;
    k_work_init(&work->work, thread_recovery_worker);
    
    /* Submit to system work queue */
    k_work_submit(&work->work);

    if (new_thread_id != NULL) {
        *new_thread_id = entry->thread_id; /* Will be updated by worker */
    }

    return 0;
}

static enum ft_handler_result thread_fault_handler(
    const struct ft_fault_context *fault_ctx,
    struct ft_recovery_context *recovery_ctx,
    void *user_data)
{
    LOG_INF("Thread fault handler called for fault type: %s",
            ft_get_fault_type_name(fault_ctx->fault_type));

    /* Only handle thread-related faults */
    if (fault_ctx->fault_type != FT_FAULT_STACK_OVERFLOW &&
        fault_ctx->fault_type != FT_FAULT_THREAD_EXCEPTION) {
        return FT_HANDLER_CONTINUE;
    }

    k_mutex_lock(&registry_mutex, K_FOREVER);

    struct ft_thread_entry *entry = find_thread_entry(fault_ctx->thread_id);
    if (entry == NULL) {
        k_mutex_unlock(&registry_mutex);
        LOG_WRN("Faulted thread %p not registered for recovery", fault_ctx->thread_id);
        return FT_HANDLER_CONTINUE;
    }

    if (!entry->auto_recovery) {
        k_mutex_unlock(&registry_mutex);
        LOG_INF("Auto-recovery disabled for thread %p", fault_ctx->thread_id);
        return FT_HANDLER_CONTINUE;
    }

    if (entry->info.restart_count >= entry->info.max_restarts) {
        k_mutex_unlock(&registry_mutex);
        LOG_ERR("Thread %p exceeded maximum restart attempts", fault_ctx->thread_id);
        recovery_ctx->action = FT_RECOVERY_SYSTEM_REBOOT;
        return FT_HANDLER_HANDLED;
    }

    k_mutex_unlock(&registry_mutex);

    /* Set up recovery context */
    recovery_ctx->action = FT_RECOVERY_CUSTOM;
    recovery_ctx->target_thread = fault_ctx->thread_id;

    /* Trigger thread recovery */
    int ret = ft_thread_recover(fault_ctx->thread_id, NULL);
    if (ret != 0) {
        LOG_ERR("Failed to initiate thread recovery: %d", ret);
        return FT_HANDLER_ERROR;
    }

    return FT_HANDLER_HANDLED;
}

int ft_thread_set_recovery_config(k_tid_t thread_id, bool auto_recovery, 
                                  uint32_t max_restarts)
{
    if (!thread_recovery_initialized) {
        return -ENODEV;
    }

    if (thread_id == NULL) {
        return -EINVAL;
    }

    k_mutex_lock(&registry_mutex, K_FOREVER);

    struct ft_thread_entry *entry = find_thread_entry(thread_id);
    if (entry == NULL) {
        k_mutex_unlock(&registry_mutex);
        return -ENOENT;
    }

    entry->auto_recovery = auto_recovery;
    entry->info.max_restarts = max_restarts;

    k_mutex_unlock(&registry_mutex);

    LOG_INF("Updated recovery config for thread %p: auto=%d, max_restarts=%d",
            thread_id, auto_recovery, max_restarts);

    return 0;
}

int ft_thread_get_stats(k_tid_t thread_id, uint32_t *restart_count)
{
    if (!thread_recovery_initialized) {
        return -ENODEV;
    }

    if (thread_id == NULL || restart_count == NULL) {
        return -EINVAL;
    }

    k_mutex_lock(&registry_mutex, K_FOREVER);

    struct ft_thread_entry *entry = find_thread_entry(thread_id);
    if (entry == NULL) {
        k_mutex_unlock(&registry_mutex);
        return -ENOENT;
    }

    *restart_count = entry->info.restart_count;

    k_mutex_unlock(&registry_mutex);

    return 0;
}