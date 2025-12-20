add_executable(ping
    ping.cpp
)
target_link_libraries(ping
    libp2p
    p2p_protocol_ping
    hiredis
)