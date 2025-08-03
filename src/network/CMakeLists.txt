add_library(p2p_connection_manager
    connection_manager.cpp
)
target_link_libraries(p2p_connection_manager
    Boost::boost
)

add_library(p2p_transport_manager
    transport_manager.cpp
)
target_link_libraries(p2p_transport_manager
    p2p_multiaddress
)

add_library(p2p_listener_manager
    listener_manager.cpp
)
target_link_libraries(p2p_listener_manager
    Boost::boost
    p2p_multiaddress
    p2p_peer_id
    p2p_logger
)