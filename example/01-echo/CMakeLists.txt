#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_executable(libp2p_echo_server
    libp2p_echo_server.cpp
)
target_link_libraries(libp2p_echo_server
    Boost::Boost.DI
    p2p_basic_host
    p2p_peer_repository
    p2p_inmem_address_repository
    p2p_inmem_key_repository
    p2p_quic
    p2p_inmem_protocol_repository
    p2p_read_buffer
    p2p_protocol_echo
    p2p_literals
    p2p_varint_prefix_reader
    p2p_converters
    p2p_dialer
    p2p_crypto_provider
    p2p_connection_manager
    p2p_transport_manager
    p2p_identity_manager
    p2p_key_marshaller
    p2p_key_validator
    p2p_crypto_error
    p2p_peer_errors
)

add_executable(libp2p_echo_client
    libp2p_echo_client.cpp
)
target_link_libraries(libp2p_echo_client
    Boost::Boost.DI
    p2p_basic_host
    p2p_peer_repository
    p2p_inmem_address_repository
    p2p_inmem_key_repository
    p2p_quic
    p2p_inmem_protocol_repository
    p2p_read_buffer
    p2p_protocol_echo
    p2p_literals
    p2p_varint_prefix_reader
    p2p_converters
    p2p_dialer
    p2p_crypto_provider
    p2p_connection_manager
    p2p_transport_manager
    p2p_identity_manager
    p2p_key_marshaller
    p2p_key_validator
    p2p_crypto_error
    p2p_peer_errors
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
