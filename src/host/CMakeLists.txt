#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(p2p_basic_host
    basic_host.cpp
    )
target_link_libraries(p2p_basic_host
    Boost::boost
    p2p_multiaddress
    p2p_peer_repository
    p2p_connection_manager
    p2p_transport_manager
    p2p_listener_manager
    p2p_dialer
    p2p_identity_manager
    )
