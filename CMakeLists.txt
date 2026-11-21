cmake_minimum_required(VERSION 3.16)

# <bits/stdc++.h> is GCC only, so on a Mac use Homebrew's GCC.
if(APPLE AND NOT CMAKE_CXX_COMPILER)
  file(GLOB HOMEBREW_GXX /opt/homebrew/bin/g++-[0-9]* /usr/local/bin/g++-[0-9]*)
  if(NOT HOMEBREW_GXX)
    message(FATAL_ERROR "GCC not found. Install it with: brew install gcc")
  endif()
  list(GET HOMEBREW_GXX -1 CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_FLAGS_INIT "-mmacosx-version-min=15.0") # fixes GCC on new macOS
endif()

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
