cmake_minimum_required(VERSION 3.25)
project(leanp2p-example)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost CONFIG REQUIRED filesystem random beast program_options)
find_package(Boost.DI CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(libsecp256k1 CONFIG REQUIRED)
find_package(lsquic CONFIG REQUIRED)
find_package(OpenSSL CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(soralog CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

find_package(libp2p CONFIG REQUIRED)

add_executable(example main.cpp)
target_link_libraries(example PRIVATE p2p::libp2p)
