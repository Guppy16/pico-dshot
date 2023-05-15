cmake_minimum_required(VERSION 3.12)

# Use this cmake file to import pico sdk. This makes it easier to use as a submodule
include(lib/extern/pico_sdk_import.cmake)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_library(dshot-pico ${CMAKE_CURRENT_LIST_DIR}/src/dshot.c)
target_include_directories(dshot-pico PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(dshot-pico
  pico_stdlib
  pico_time
  hardware_pwm
  hardware_dma
  hardware_irq
  hardware_clocks
  hardware_uart
)
