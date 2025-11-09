# SPDX-License-Identifier: Apache-2.0
#
# Copyright (c) 2025 Jack Ostapeic
#
# CMakeLists.txt for Fault Tolerance Framework

if(CONFIG_FAULT_TOLERANCE)

zephyr_library()

# Core fault tolerance framework
zephyr_library_sources(ft_core.c)
zephyr_library_sources(ft_framework.c)

# Thread recovery module
zephyr_library_sources_ifdef(CONFIG_FT_ENABLE_THREAD_RECOVERY ft_thread_recovery.c)

# System monitoring module
zephyr_library_sources_ifdef(CONFIG_FT_ENABLE_SYSTEM_MONITOR ft_system_monitor.c)

# Fault logging module
zephyr_library_sources_ifdef(CONFIG_FT_ENABLE_FAULT_LOGGING ft_fault_logging.c)

# Stack overflow protection module
zephyr_library_sources_ifdef(CONFIG_FT_ENABLE_STACK_OVERFLOW_PROTECTION ft_stack_overflow.c)

# Buffer overflow protection module
zephyr_library_sources_ifdef(CONFIG_FT_ENABLE_BUFFER_OVERFLOW_PROTECTION ft_buffer_overflow.c)

# Deadlock detection module
zephyr_library_sources_ifdef(CONFIG_FT_ENABLE_DEADLOCK_DETECTION ft_deadlock.c)

# Race condition detection module
zephyr_library_sources_ifdef(CONFIG_FT_ENABLE_RACE_CONDITION_DETECTION ft_race_condition.c)

# Include directories for fault tolerance headers
zephyr_library_include_directories(${ZEPHYR_BASE}/include)

endif() # CONFIG_FAULT_TOLERANCE