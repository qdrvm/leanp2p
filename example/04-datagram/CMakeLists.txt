#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_executable(libp2p_datagram_example
    datagram_example.cpp
)
target_link_libraries(libp2p_datagram_example
    libp2p
)
