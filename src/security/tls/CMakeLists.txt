#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

libp2p_add_library(p2p_tls
    ssl_context.cpp
    tls_details.cpp
    )
target_link_libraries(p2p_tls
    Boost::boost
    p2p_crypto_error
    p2p_logger
    p2p_peer_id
    p2p_identity_manager
    p2p_key_marshaller
    p2p_ecdsa_provider
    p2p_ed25519_provider
    OpenSSL::SSL
    OpenSSL::Crypto
    )
