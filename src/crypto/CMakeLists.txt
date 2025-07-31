add_subdirectory(sha)

add_library(p2p_crypto_error
    error.cpp
)
target_link_libraries(p2p_crypto_error
    Boost::boost
)