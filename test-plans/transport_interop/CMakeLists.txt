add_executable(test_plans_ping
    test_plans_ping.cpp
)
target_link_libraries(test_plans_ping
    libp2p
    p2p_protocol_ping
    test_plans_common
)
