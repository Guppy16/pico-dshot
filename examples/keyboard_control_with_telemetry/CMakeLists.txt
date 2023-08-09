cmake_minimum_required(VERSION 3.12)

include(../../lib/extern/pico-sdk/pico_sdk_init.cmake)

# include(pico_sdk_import.cmake)
# include(pico_extras_import.cmake)

project(dshot_example LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_compile_options(
  -Wall
  -Wno-format # int != int32_t as far as the compiler is concerned because gcc
              # has int32_t as long int
  -Wno-unused-function # we have some for the docs that aren't called
  -Wno-maybe-uninitialized)

pico_sdk_init()

add_executable(${PROJECT_NAME}
  main.cpp
)

# dshot-pico api
add_subdirectory(../../ dshot-pico)

target_link_libraries(
  ${PROJECT_NAME}
  pico_stdlib
  pico_platform
  dshot-pico
)

pico_add_extra_outputs(${PROJECT_NAME})
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

