cmake_minimum_required(VERSION 3.25)
project(mini_libp2p)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

include(cmake/functions.cmake)
include(cmake/install.cmake)

# Set global include directory
include_directories(include)

# Add subdirectories to collect source files
add_subdirectory(src)
