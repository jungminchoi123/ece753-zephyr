# Simple ARM Cortex-M3 Enhanced Fault Tolerance Application
# Build only the files that compile successfully
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ft_cortexm3_enhanced)

# Main application files
target_sources(app PRIVATE
    src/main.c
)

# Only include test files that compile
target_sources(app PRIVATE
    src/fault_tests/stack_overflow_test.c
    src/fault_tests/division_by_zero_test.c
    src/fault_tests/null_pointer_test.c
    src/fault_tests/peripheral_misconfig_test.c
)

# Include directories for modules
target_include_directories(app PRIVATE
    src/
    src/fault_tests/
)