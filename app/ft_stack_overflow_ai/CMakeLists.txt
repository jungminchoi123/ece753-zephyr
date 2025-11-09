# Author: Jack Ostapeic, MS ECE student at UW-Madison

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ft_stack_overflow_test)

# Use the safe predictive demo
target_sources(app PRIVATE src/safe_predictive_demo.c)

# Alternative test files:
# target_sources(app PRIVATE src/recovery_demo.c)   # Complex recovery demo
# target_sources(app PRIVATE src/test_simple.c)    # Simple test
# target_sources(app PRIVATE src/main.c)           # Original complex test