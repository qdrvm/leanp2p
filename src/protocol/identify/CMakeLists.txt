#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#


add_proto_library(p2p_identify_proto
    identify.proto
    )

libp2p_add_library(p2p_protocol_identify
    identify.cpp
    )
target_link_libraries(p2p_protocol_identify
    Boost::boost
    p2p_identify_proto
    p2p_logger
    )
