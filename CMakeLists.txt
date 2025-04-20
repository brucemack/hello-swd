cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(hello-pico)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
#set(CMAKE_VERBOSE_MAKEFILE True)
set(HOME $ENV{HOME})
pico_sdk_init()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")

# ----- main ------------------------------------------------------------------

add_executable(main
  main.cpp
  SWDDriver.cpp
)

pico_enable_stdio_usb(main 1)
target_link_libraries(main pico_stdlib hardware_i2c)

add_executable(prog-1
  prog-1.cpp
  SWDDriver.cpp
)

pico_enable_stdio_usb(prog-1 1)
target_link_libraries(prog-1 pico_stdlib hardware_i2c)

# ----- blinky ----------------------------------------------------------------
# Useful for keeping a processor visibly busy when testing debuggers.

add_executable(blinky
  blinky.cpp
)

pico_enable_stdio_usb(blinky 1)
target_link_libraries(blinky pico_stdlib)

# ----- flash-test-1 ----------------------------------------------------------

add_executable(flash-test-1
  flash-test-1.cpp
  flash-test-1a.c
)

pico_enable_stdio_usb(flash-test-1 1)
target_link_libraries(flash-test-1 pico_stdlib)

# ----- flash-test-2 ----------------------------------------------------------

add_executable(flash-test-2
  flash-test-2.cpp
  flash-test-2a.c
)

pico_enable_stdio_usb(flash-test-2 1)
target_link_libraries(flash-test-2 pico_stdlib)

# ----- flash-test-3 ----------------------------------------------------------

add_executable(flash-test-3
  flash-test-2.cpp
  flash-test-3a.c
)

pico_enable_stdio_usb(flash-test-3 1)
target_link_libraries(flash-test-3 pico_stdlib)
