cmake_minimum_required(VERSION 3.17)
project(DTFTools)

set(CMAKE_CXX_STANDARD 11)

add_executable(DTFTools main.cpp HDTools.cpp HDTools.h FSTools.cpp FSTools.h type.h list.cpp list.h)