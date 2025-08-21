cmake_minimum_required(VERSION 3.25)
project(leanp2p-example)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libp2p CONFIG REQUIRED)

add_executable(example main.cpp)
target_link_libraries(example PRIVATE p2p::p2p)
