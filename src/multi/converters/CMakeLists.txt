#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(p2p_converters
    converter_utils.cpp
    conversion_error.cpp
    ip_v4_converter.cpp
    ip_v6_converter.cpp
    tcp_converter.cpp
    udp_converter.cpp
    ipfs_converter.cpp
    dns_converter.cpp
    )
target_link_libraries(p2p_converters
    Boost::boost
    p2p_uvarint
    p2p_multibase_codec
    p2p_byteutil
)