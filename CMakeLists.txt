CMAKE_MINIMUM_REQUIRED(VERSION 2.4)

project(libmultigain CXX)

set(CMAKE_VERBOSE_MAKEFILE false)

if(NOT MSVC)
set(CMAKE_C_FLAGS "-std=c99 -O2 -pipe")
set(CMAKE_CXX_FLAGS "-std=c++98 -O2 -pipe")
endif(NOT MSVC)

add_subdirectory(src)
