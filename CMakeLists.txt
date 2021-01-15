cmake_minimum_required(VERSION 3.0.0)
project(toy-chess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
add_compile_options(-Wall -Wextra -Wshadow)

# Sanitizer
if(CMAKE_BUILD_TYPE MATCHES Debug)
  add_compile_options(-fsanitize=address -fsanitize=undefined)
  add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

# SIMD
option(USE_SIMD "Use SIMD" ON)
if(${USE_SIMD})
  add_compile_options(-mavx -mfma)
endif()

# Pthread for std::async
add_link_options(-lpthread)

# Catch2 testing
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Catch2)

# Precompiled header
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/main_pch_dummy.cpp CONTENT "")
add_library(main_pch ${CMAKE_CURRENT_BINARY_DIR}/main_pch_dummy.cpp)
target_precompile_headers(main_pch PUBLIC <bits/stdc++.h>)

# config.h
include_directories(${CMAKE_CURRENT_BINARY_DIR})
configure_file(src/config.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/config.hpp @ONLY)

#
# embedded_weight.cpp (TODO: make it override-able)
#
set(NN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/nn)
set(WEIGHT_FILE ${NN_DIR}/data/default.bin)
set(WEIGHT_FILE_URL https://github.com/hi-ogawa/toy-chess/releases/download/data/default.bin)
if(NOT EXISTS ${WEIGHT_FILE})
  message("Downloading a weight file from ${WEIGHT_FILE_URL}")
  file(DOWNLOAD ${WEIGHT_FILE_URL} ${WEIGHT_FILE} SHOW_PROGRESS)
endif()

set(EMBEDDED_WEIGHT_CPP ${CMAKE_CURRENT_BINARY_DIR}/embedded_weight.cpp)
add_custom_command(
  OUTPUT ${EMBEDDED_WEIGHT_CPP}
  DEPENDS ${NN_DIR}/embedded_weight.py ${WEIGHT_FILE}
  COMMAND python ${NN_DIR}/embedded_weight.py ${WEIGHT_FILE} > ${EMBEDDED_WEIGHT_CPP}
)
add_custom_target(generate_embedded_weight DEPENDS ${EMBEDDED_WEIGHT_CPP})


#
# Targets
#

# main_lib
add_library(main_lib
  src/precomputation.cpp
  src/position.cpp
  src/engine.cpp
  src/uci.cpp
  src/transposition_table.cpp
  src/nn/evaluator.cpp
  ${EMBEDDED_WEIGHT_CPP}
)
target_precompile_headers(main_lib REUSE_FROM main_pch)
add_dependencies(main_lib generate_embedded_weight)

# main_test
add_executable(main_test
  src/base_test.cpp
  src/precomputation_test.cpp
  src/position_test.cpp
  src/engine_test.cpp
  src/uci_test.cpp
  src/nn/evaluator_test.cpp
)
target_link_libraries(main_test PRIVATE main_lib Catch2WithMain)
target_precompile_headers(main_test REUSE_FROM main_pch)

# main
add_executable(main src/main.cpp)
target_link_libraries(main PRIVATE main_lib)
target_precompile_headers(main REUSE_FROM main_pch)

# main_bench
if(CMAKE_BUILD_TYPE MATCHES Release)
  add_executable(main_bench
    src/position_bench.cpp
  )
  target_link_libraries(main_bench PRIVATE main_lib Catch2WithMain)
  target_precompile_headers(main_bench REUSE_FROM main_pch)
endif()

# nn_preprocess
add_executable(nn_preprocess src/nn/training/preprocess.cpp)
target_precompile_headers(nn_preprocess REUSE_FROM main_pch)
