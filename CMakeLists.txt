cmake_minimum_required(VERSION 3.12)

include(lib/extern/pico-sdk/pico_sdk_init.cmake)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(DSHOT_FILES 
  ./lib/shoot/shoot.cpp 
  ./lib/tts/tts.cpp 
  ./lib/utils/utils.cpp
)

set(DSHOT_INCL
  ./lib/shoot
  ./lib/tts
  ./lib/utils
  ./include
)

add_library(dshot-pico ${DSHOT_FILES})
target_include_directories(dshot-pico PUBLIC ${DSHOT_INCL})
target_link_libraries(dshot-pico
  pico_stdlib
  pico_time
  hardware_pwm
  hardware_dma
  hardware_irq
)
