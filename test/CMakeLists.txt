cmake_minimum_required(VERSION 3.12)

project(test_tts)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(CTest)

# Setup the Unity Testing library
add_library(Unity STATIC ../lib/extern/Unity/src/unity.c)
target_include_directories(Unity PUBLIC ../lib/extern/Unity/src)

# Setup the dshot lib
add_library(Dshot STATIC ../lib/dshot/dshot.cpp)
target_include_directories(Dshot PUBLIC ../lib/dshot)

add_executable(test_dshot test_dshot.cpp)
target_link_libraries(test_dshot Unity Dshot)

add_test(NAME test_dshot COMMAND test_dshot)