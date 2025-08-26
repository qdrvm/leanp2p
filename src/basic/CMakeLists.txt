libp2p_add_library(p2p_varint_prefix_reader
    varint_prefix_reader.cpp
)
target_link_libraries(p2p_varint_prefix_reader
    p2p_logger
)

libp2p_add_library(p2p_read_buffer
    read_buffer.cpp
)
target_link_libraries(p2p_read_buffer
    p2p_logger
)