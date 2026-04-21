cmake_minimum_required(VERSION 3.16)
project(teddy CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic -O2)

set(SOURCES
    src/main.cpp
    src/editor.cpp
    src/buffer.cpp
    src/terminal.cpp
    src/syntax.cpp
    src/md_parser.cpp
    src/md_render.cpp
)

add_executable(teddy ${SOURCES})
target_include_directories(teddy PRIVATE src)
