cmake_minimum_required(VERSION 3.1)

project(aoc2022)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wno-unused-parameter -pedantic -g3)

add_library(token token.cpp token.hpp)

add_library(debug_output debug_output.cpp debug_output.hpp)
target_link_libraries(debug_output token)

add_library(lexer lexer.cpp lexer.hpp)
target_link_libraries(lexer token)

add_executable(compiler compiler.cpp)
target_link_libraries(compiler lexer debug_output)
