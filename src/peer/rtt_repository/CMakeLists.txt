#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

libp2p_add_library(p2p_inmem_rtt_repository
    inmem_rtt_repository.cpp
    )
target_link_libraries(p2p_inmem_rtt_repository
    p2p_peer_id
    )

