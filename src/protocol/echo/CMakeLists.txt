#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(p2p_protocol_echo
    echo.cpp
    )
target_link_libraries(p2p_protocol_echo
    Boost::boost
    p2p_logger
    )
