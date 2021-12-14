# Set the minimum version of CMake that can be used
# To find the cmake version run
# $ cmake --version
cmake_minimum_required(VERSION 3.5)

project(gl-silabs-dfu
		VERSION 1.0.0 
        LANGUAGES C )

add_definitions( -D_GNU_SOURCE )
# add_compile_options(-O -Wall -Werror --std=gnu99)
add_compile_options(-O -Wall --std=gnu99)

set(CMAKE_C_FLAGS_RELEASE "-O3") 

add_subdirectory(./main build/main)

