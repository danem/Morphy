cmake_minimum_required(VERSION 3.2)
project(Morphy CXX)
set(CMAKE_CXX_STANDARD 11)

set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)


add_library(morphy
    ./src/board.cc
    ./src/engine.cc
    ./src/uci.cc
)
target_include_directories(morphy PUBLIC ./include)

add_executable(morphy_tests ./tests/board.cc)
target_link_libraries(morphy_tests morphy)


