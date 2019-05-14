cmake_minimum_required(VERSION 3.12)
project(ex4 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
add_executable(ex4 threadPool.c threadPool.h osqueue.h osqueue.c)