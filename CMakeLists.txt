cmake_minimum_required(VERSION 3.0.0)
project(toy-chess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
add_compile_options(-Wall -Wextra -Wshadow)

# Sanitizer
if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(DEFAULT_SANITIZERS "-fsanitize=address -fsanitize=undefined")
else()
  set(DEFAULT_SANITIZERS "")
endif()
set(SANITIZERS "${DEFAULT_SANITIZERS}" CACHE STRING "Sanitizer options")
add_compile_options("SHELL: ${SANITIZERS}")
add_link_options("SHELL: ${SANITIZERS}")

# SIMD
option(USE_SSE "Use SSE" OFF)
option(USE_AVX "Use AVX" ON)
option(USE_FMA "Use FMA" ON)
if(USE_SSE)
  add_compile_options("-msse")
endif()
if(USE_AVX)
  add_compile_options("-mavx")
endif()
if(USE_FMA)
  add_compile_options("-mfma")
endif()

# Catch2 testing
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Catch2)

# Precompiled header
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/main_pch_dummy.cpp CONTENT "")
add_library(main_pch ${CMAKE_CURRENT_BINARY_DIR}/main_pch_dummy.cpp)
target_precompile_headers(main_pch PRIVATE <bits/stdc++.h>)

# config.h
include_directories(${CMAKE_CURRENT_BINARY_DIR})
configure_file(src/config.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/config.hpp @ONLY)

#
# embedded_weight.cpp
#
set(NN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/nn)
set(EVAL_WEIGHT_FILE ${NN_DIR}/data/default-eval.bin)
set(MOVE_WEIGHT_FILE ${NN_DIR}/data/default-move.bin)
set(EVAL_WEIGHT_FILE_URL https://github.com/hi-ogawa/toy-chess/releases/download/data/ckpt-2021-01-21-06-25-25.bin)
set(MOVE_WEIGHT_FILE_URL https://github.com/hi-ogawa/toy-chess/releases/download/data/ckpt-move-2021-01-28-12-36-34.bin)

if(NOT EXISTS ${EVAL_WEIGHT_FILE})
  message("Downloading a eval weight file from ${EVAL_WEIGHT_FILE_URL}")
  file(DOWNLOAD ${EVAL_WEIGHT_FILE_URL} ${EVAL_WEIGHT_FILE} SHOW_PROGRESS)
endif()

if(NOT EXISTS ${MOVE_WEIGHT_FILE})
  message("Downloading a move weight file from ${MOVE_WEIGHT_FILE_URL}")
  file(DOWNLOAD ${MOVE_WEIGHT_FILE_URL} ${MOVE_WEIGHT_FILE} SHOW_PROGRESS)
endif()

set(EMBEDDED_WEIGHT_CPP ${CMAKE_CURRENT_BINARY_DIR}/embedded_weight.cpp)
add_custom_command(
  OUTPUT ${EMBEDDED_WEIGHT_CPP}
  DEPENDS ${NN_DIR}/embedded_weight.py ${EVAL_WEIGHT_FILE} ${MOVE_WEIGHT_FILE}
  COMMAND python ${NN_DIR}/embedded_weight.py ${EVAL_WEIGHT_FILE} ${MOVE_WEIGHT_FILE} > ${EMBEDDED_WEIGHT_CPP}
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
  src/nn/move_evaluator.cpp
  src/mcts/engine.cpp
  ${EMBEDDED_WEIGHT_CPP}
)
target_link_libraries(main_lib PRIVATE pthread) # for std::async
target_precompile_headers(main_lib REUSE_FROM main_pch)
add_dependencies(main_lib generate_embedded_weight)

# main_test
add_executable(main_test
  src/base_test.cpp
  src/precomputation_test.cpp
  src/position_test.cpp
  src/engine_test.cpp
  src/uci_test.cpp
  src/timeit_test.cpp
  src/nn/evaluator_test.cpp
  src/nn/move_evaluator_test.cpp
  src/mcts/engine_test.cpp
)
target_link_libraries(main_test PRIVATE main_lib Catch2WithMain)
target_precompile_headers(main_test REUSE_FROM main_pch)

# main
add_executable(main src/main.cpp)
target_link_libraries(main PRIVATE main_lib)
target_precompile_headers(main REUSE_FROM main_pch)

# main_bench
add_executable(main_bench
  src/position_bench.cpp
  src/engine_bench.cpp
  src/nn/evaluator_bench.cpp
)
target_link_libraries(main_bench PRIVATE main_lib Catch2WithMain)
target_precompile_headers(main_bench REUSE_FROM main_pch)

# nn_preprocess
add_executable(nn_preprocess src/nn/training/preprocess.cpp)
target_precompile_headers(nn_preprocess REUSE_FROM main_pch)

# nn_preprocess_pgn
add_executable(nn_preprocess_move src/nn/training/preprocess_move.cpp)
target_link_libraries(nn_preprocess_move PRIVATE main_lib)
target_precompile_headers(nn_preprocess_move REUSE_FROM main_pch)

# slim_pgn
add_executable(slim_pgn src/nn/training/slim_pgn.cpp)
target_precompile_headers(slim_pgn REUSE_FROM main_pch)
