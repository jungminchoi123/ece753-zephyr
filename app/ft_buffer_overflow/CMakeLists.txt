# Buffer Overflow Fault Tolerance Application
# Author: Jack Ostapeic, MS ECE student at UW-Madison

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ft_buffer_overflow_test)

# Use the buffer overflow demonstration
target_sources(app PRIVATE src/buffer_overflow_demo.c)

# Alternative test files can be added here:
# target_sources(app PRIVATE src/memory_corruption_demo.c)    # Memory corruption test
# target_sources(app PRIVATE src/heap_overflow_demo.c)       # Heap overflow test