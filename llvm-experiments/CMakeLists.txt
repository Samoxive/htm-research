cmake_minimum_required(VERSION 3.10)
project(llvm_experiments)

set(CMAKE_CXX_STANDARD 17)

add_library(llvm_experiments SHARED library.cpp)
add_library(tm tm.cpp tm.h)
add_executable(main main.cpp tm.cpp tm.h)