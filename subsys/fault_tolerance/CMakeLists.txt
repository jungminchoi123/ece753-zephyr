# Author: Jack Ostapeic, MS ECE student at UW-Madison

if(CONFIG_FAULT_TOLERANCE)

zephyr_library()
zephyr_library_sources(
    fault_tolerance.c
)

endif() # CONFIG_FAULT_TOLERANCE