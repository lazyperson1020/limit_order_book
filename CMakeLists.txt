cmake_minimum_required(VERSION 3.16)
project(limitorderbook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(lob src/OrderBook.cpp)
target_include_directories(lob PUBLIC include)
target_compile_options(lob PRIVATE -Wall -Wextra -Wpedantic)

add_executable(lob_demo examples/main.cpp)
target_link_libraries(lob_demo PRIVATE lob)

add_executable(lob_live examples/live.cpp)
target_link_libraries(lob_live PRIVATE lob)

enable_testing()
add_subdirectory(tests)
