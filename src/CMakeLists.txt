# src/CMakeLists.txt

# Add subdirectories to collect source files
add_subdirectory(basic)
add_subdirectory(c)
add_subdirectory(common)
add_subdirectory(crypto)
add_subdirectory(host)
add_subdirectory(log)
add_subdirectory(multi)
add_subdirectory(network)
add_subdirectory(peer)
add_subdirectory(protocol)
add_subdirectory(protocol_muxer)
add_subdirectory(security)
add_subdirectory(transport)

# Create main libp2p interface library that combines all components
libp2p_add_library(libp2p INTERFACE)

# Start with basic component libraries and essential dependencies
target_link_libraries(libp2p
    INTERFACE
        Boost::boost
        Boost::Boost.DI
        fmt::fmt
        libsecp256k1::secp256k1
        lsquic::lsquic
        OpenSSL::Crypto
        OpenSSL::SSL
        p2p_basic_host
        p2p_byteutil
        p2p_connection_manager
        p2p_crypto_provider
        p2p_dialer
        p2p_ecdsa_provider
        p2p_ed25519_provider
        p2p_gossip
        p2p_hmac_provider
        p2p_identity_manager
        p2p_inmem_address_repository
        p2p_inmem_key_repository
        p2p_inmem_protocol_repository
        p2p_inmem_rtt_repository
        p2p_key_validator
        p2p_listener_manager
        p2p_literals
        p2p_logger
        p2p_multiaddress
        p2p_multibase_codec
        p2p_multiselect
        p2p_peer_address
        p2p_peer_id
        p2p_peer_repository
        p2p_protocol_identify
        p2p_protocol_ping
        p2p_quic
        p2p_random_generator
        p2p_read_buffer
        p2p_rsa_provider
        p2p_secp256k1_provider
        p2p_tls
        p2p_transport_manager
        p2p_varint_prefix_reader
        protobuf::libprotobuf
        soralog::soralog
        yaml-cpp::yaml-cpp
        ZLIB::ZLIB
)

target_include_directories(libp2p
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
