cmake_minimum_required(VERSION 3.10)
project(llvm_experiments)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "-fno-rtti")

add_library(llvm_experiments SHARED library.cpp)