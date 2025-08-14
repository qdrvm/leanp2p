#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_executable(libp2p_echo_server
    libp2p_echo_server.cpp
)
target_link_libraries(libp2p_echo_server
    libp2p
    p2p_protocol_echo
)

add_executable(libp2p_echo_client
    libp2p_echo_client.cpp
)
target_link_libraries(libp2p_echo_client
    libp2p
    p2p_protocol_echo
)

# C API Echo Server
add_executable(libp2p_echo_server_c
    libp2p_echo_server_c.c
)
target_link_libraries(libp2p_echo_server_c
    libp2p_c
)

# C API Echo Client
add_executable(libp2p_echo_client_c
    libp2p_echo_client_c.c
)
target_link_libraries(libp2p_echo_client_c
    libp2p_c

)
