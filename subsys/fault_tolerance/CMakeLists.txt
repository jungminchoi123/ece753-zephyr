# Copyright (c) 2025 ECE753 Project Team
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_FAULT_TOLERANCE)

zephyr_library()
zephyr_library_sources(fault_tolerance.c)

# Add additional sources based on configuration
if(CONFIG_FAULT_TOLERANCE_STACK_MONITORING)
  zephyr_library_sources_ifdef(CONFIG_THREAD_STACK_INFO stack_monitor.c)
endif()

if(CONFIG_FAULT_TOLERANCE_MEMORY_MONITORING)
  zephyr_library_sources(memory_monitor.c)
endif()

if(CONFIG_FAULT_TOLERANCE_TIMING_MONITORING)
  zephyr_library_sources(timing_monitor.c)
endif()

if(CONFIG_FAULT_TOLERANCE_DEADLOCK_DETECTION)
  zephyr_library_sources(deadlock_detector.c)
endif()

if(CONFIG_FAULT_TOLERANCE_PERSISTENT_LOGGING)
  zephyr_library_sources(persistent_log.c)
endif()

endif() # CONFIG_FAULT_TOLERANCE