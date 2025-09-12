add_subdirectory(echo)
add_subdirectory(gossip)
add_subdirectory(identify)

libp2p_add_library(p2p_protocol_ping
    ping.cpp
    )
target_link_libraries(p2p_protocol_ping
    Boost::boost
    p2p_logger
    )
