cmake_minimum_required(VERSION 3.20)
project(cHashMap C)

set(CMAKE_C_STANDARD 99)

add_executable(cHashMap hashmap_example.c hashmap.h hashmap.c)
