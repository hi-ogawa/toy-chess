cmake_minimum_required(VERSION 3.0.0)
project(project-abc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
add_compile_options(-Wall -Wextra -Wshadow)

# Sanitizer
if(CMAKE_BUILD_TYPE MATCHES Debug)
  add_compile_options(-fsanitize=address -fsanitize=undefined)
  add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

# Catch2 testing
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Catch2)

# Precompiled header
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/main_pch_dummy.cpp CONTENT "")
add_library(main_pch ${CMAKE_CURRENT_BINARY_DIR}/main_pch_dummy.cpp)
target_precompile_headers(main_pch PUBLIC <bits/stdc++.h>)

# config.h
include_directories(${CMAKE_CURRENT_BINARY_DIR})
configure_file(src/config.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/config.hpp @ONLY)

# Targets
add_library(main_lib
  src/precomputation.cpp
  src/position.cpp
)
target_precompile_headers(main_lib REUSE_FROM main_pch)

add_executable(main_test
  src/base_test.cpp
  src/precomputation_test.cpp
  src/position_test.cpp
)
target_link_libraries(main_test PRIVATE main_lib Catch2WithMain)
target_precompile_headers(main_test REUSE_FROM main_pch)

if(CMAKE_BUILD_TYPE MATCHES Release)
  add_executable(main_bench
    src/position_bench.cpp
  )
  target_link_libraries(main_bench PRIVATE main_lib Catch2WithMain)
  target_precompile_headers(main_bench REUSE_FROM main_pch)
endif()
