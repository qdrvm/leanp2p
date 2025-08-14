#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_proto_library(p2p_gossip_proto
    gossip.proto
    )

add_library(p2p_gossip
    gossip.cpp
    )
target_link_libraries(p2p_gossip
    Boost::boost
    p2p_gossip_proto
    p2p_logger
    )
