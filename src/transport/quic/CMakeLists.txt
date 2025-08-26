#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

libp2p_add_library(p2p_quic
    connection.cpp
    engine.cpp
    listener.cpp
    stream.cpp
    transport.cpp
    )
target_link_libraries(p2p_quic
    lsquic::lsquic
    p2p_tls
    p2p_multiaddress
    p2p_peer_id
    p2p_identity_manager
    p2p_multihash
    p2p_varint_prefix_reader
    p2p_logger
    p2p_key_marshaller
    p2p_crypto_key
    p2p_keys_proto
    p2p_ecdsa_provider
    p2p_ed25519_provider
    p2p_sha
    p2p_crypto_common
    p2p_crypto_error
    ZLIB::ZLIB
    OpenSSL::SSL
    OpenSSL::Crypto
    fmt::fmt
    )
