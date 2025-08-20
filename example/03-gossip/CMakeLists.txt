#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_executable(gossip_chat_example
    gossip_chat_example.cpp
)
target_link_libraries(gossip_chat_example
    Boost::Boost.DI
    libp2p
)
