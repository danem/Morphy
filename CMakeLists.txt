cmake_minimum_required(VERSION 3.2)
project(Morphy CXX)
set(CMAKE_CXX_STANDARD 11)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/source)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SOURCE_FILES
    ${SOURCE_DIR}/board.cc
    ${SOURCE_DIR}/engine.cc
    ${SOURCE_DIR}/main.cc
    ${SOURCE_DIR}/uci.cc
)

set(INCLUDE_FILES 
    ${INCLUDE_DIR}/board.h
    ${INCLUDE_DIR}/engine.h
    ${INCLUDE_DIR}/uci.h
)

add_executable(morphy ${SOURCE_FILES} ${INCLUDE_FILES})



