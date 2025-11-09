# Deadlock Fault Tolerance Application
# Author: Jack Ostapeic, MS ECE student at UW-Madison

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ft_deadlock_test)

# Use the deadlock demonstration
target_sources(app PRIVATE src/deadlock_demo.c)

# Alternative test files can be added here:
# target_sources(app PRIVATE src/priority_inversion_demo.c)    # Priority inversion test
# target_sources(app PRIVATE src/resource_starvation_demo.c)  # Resource starvation test