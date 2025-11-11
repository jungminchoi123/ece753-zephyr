cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ft_comprehensive_test)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/deadlock_test.c)
target_sources(app PRIVATE src/buffer_overflow_test.c)
target_sources(app PRIVATE src/memory_leak_test.c)
target_sources(app PRIVATE src/timing_violation_test.c)