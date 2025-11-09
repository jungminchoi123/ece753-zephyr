# CMakeLists.txt for Race Condition Fault Tolerance Demo
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ft_race_condition)

target_sources(app PRIVATE src/race_condition_demo.c)