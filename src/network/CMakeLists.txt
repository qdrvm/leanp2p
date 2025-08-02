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