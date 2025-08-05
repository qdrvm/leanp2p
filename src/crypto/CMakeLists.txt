add_subdirectory(aes_provider)
add_subdirectory(crypto_provider)
add_subdirectory(ecdsa_provider)
add_subdirectory(ed25519_provider)
add_subdirectory(hmac_provider)
add_subdirectory(key_validator)
add_subdirectory(protobuf)
add_subdirectory(random_generator)
add_subdirectory(rsa_provider)
add_subdirectory(secp256k1_provider)
add_subdirectory(sha)

add_library(p2p_crypto_error
    error.cpp
)
target_link_libraries(p2p_crypto_error
    Boost::boost
)

add_library(p2p_crypto_key
    key.cpp
)
target_link_libraries(p2p_crypto_key
    Boost::boost
)

add_library(p2p_crypto_common
    common_functions.cpp
)
target_link_libraries(p2p_crypto_common
    p2p_crypto_error
)

add_library(p2p_key_marshaller
    key_marshaller.cpp
)
target_link_libraries(p2p_key_marshaller
    p2p_crypto_error
    p2p_crypto_key
    p2p_keys_proto
)