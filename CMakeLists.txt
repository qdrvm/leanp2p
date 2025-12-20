cmake_minimum_required(VERSION 3.25)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
project(leanp2p)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_REDIS "Enable Redis support" OFF)

find_package(Boost CONFIG REQUIRED filesystem random beast program_options)
find_package(OpenSSL CONFIG REQUIRED)
find_package(soralog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(lsquic CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(libsecp256k1 CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(tsl_hat_trie CONFIG REQUIRED)
find_package(Boost.DI CONFIG REQUIRED)
if(ENABLE_REDIS)
    find_package(hiredis CONFIG REQUIRED)
endif()

include(cmake/functions.cmake)
include(cmake/install.cmake)

# Set global include directory
include_directories(include)

# Add subdirectories to collect source files
add_subdirectory(src)

# Examples
add_subdirectory(example)

# Test-plans
if(ENABLE_REDIS)
    add_subdirectory(test-plans)
endif()