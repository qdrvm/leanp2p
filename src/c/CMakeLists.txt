add_library(libp2p_c SHARED
    libp2p_c.cpp
)

target_include_directories(libp2p_c
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(libp2p_c
    PUBLIC
        libp2p
)

# Set library properties for shared library
set_target_properties(libp2p_c PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    OUTPUT_NAME "p2p_c"
)

# Export symbols for shared library on Windows
if(WIN32)
    set_target_properties(libp2p_c PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endif()
