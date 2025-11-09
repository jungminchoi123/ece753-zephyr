#
# Copyright (c) 2025 Jack Ostapeic
#
# Modular Fault Tolerance Integration Test Application
#

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ft_modular_integration)

target_sources(app PRIVATE src/main.c)

# Additional sources for comprehensive testing
target_sources_ifdef(CONFIG_FT_ENABLE_STACK_OVERFLOW_PROTECTION app PRIVATE src/stack_overflow_test.c)
target_sources_ifdef(CONFIG_FT_ENABLE_BUFFER_OVERFLOW_PROTECTION app PRIVATE src/buffer_overflow_test.c)
target_sources_ifdef(CONFIG_FT_ENABLE_DEADLOCK_DETECTION app PRIVATE src/deadlock_test.c)
target_sources_ifdef(CONFIG_FT_ENABLE_RACE_CONDITION_DETECTION app PRIVATE src/race_condition_test.c)