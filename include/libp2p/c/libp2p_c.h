/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBP2P_C_API_H
#define LIBP2P_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declarations for opaque types
typedef struct libp2p_host libp2p_host_t;
typedef struct libp2p_stream libp2p_stream_t;
typedef struct libp2p_multiaddr libp2p_multiaddr_t;
typedef struct libp2p_peer_id libp2p_peer_id_t;
typedef struct libp2p_context libp2p_context_t;

// Error codes
typedef enum {
    LIBP2P_SUCCESS = 0,
    LIBP2P_ERROR_INVALID_ARGUMENT = -1,
    LIBP2P_ERROR_OUT_OF_MEMORY = -2,
    LIBP2P_ERROR_CONNECTION_FAILED = -3,
    LIBP2P_ERROR_PROTOCOL_ERROR = -4,
    LIBP2P_ERROR_IO_ERROR = -5,
    LIBP2P_ERROR_TIMEOUT = -6,
    LIBP2P_ERROR_NOT_FOUND = -7,
    LIBP2P_ERROR_ALREADY_EXISTS = -8,
    LIBP2P_ERROR_UNKNOWN = -999
} libp2p_error_t;

// Callback types
typedef void (*libp2p_stream_handler_t)(libp2p_stream_t* stream, void* user_data);
typedef void (*libp2p_connection_handler_t)(libp2p_host_t* host, const char* peer_id, void* user_data);
typedef void (*libp2p_read_callback_t)(libp2p_stream_t* stream, const uint8_t* data, size_t size, libp2p_error_t error, void* user_data);
typedef void (*libp2p_write_callback_t)(libp2p_stream_t* stream, size_t bytes_written, libp2p_error_t error, void* user_data);

// Context management
libp2p_context_t* libp2p_context_create(void);
void libp2p_context_destroy(libp2p_context_t* ctx);
libp2p_error_t libp2p_context_run(libp2p_context_t* ctx);
void libp2p_context_stop(libp2p_context_t* ctx);

// Host management
libp2p_host_t* libp2p_host_create(libp2p_context_t* ctx, const char* keypair_seed);
void libp2p_host_destroy(libp2p_host_t* host);
libp2p_error_t libp2p_host_start(libp2p_host_t* host);
libp2p_error_t libp2p_host_listen(libp2p_host_t* host, const char* multiaddr);
libp2p_error_t libp2p_host_register_protocol(libp2p_host_t* host, const char* protocol_id, libp2p_stream_handler_t handler, void* user_data);

// Host info accessors
const char* libp2p_host_peer_id(libp2p_host_t* host);
const char* libp2p_host_default_listen(libp2p_host_t* host);

// Connection management
libp2p_error_t libp2p_host_dial(libp2p_host_t* host, const char* multiaddr, const char* peer_id, libp2p_connection_handler_t callback, void* user_data);
libp2p_error_t libp2p_host_new_stream(libp2p_host_t* host, const char* peer_id, const char* protocol_id, libp2p_stream_handler_t callback, void* user_data);

// Stream operations
libp2p_error_t libp2p_stream_read(libp2p_stream_t* stream, uint8_t* buffer, size_t buffer_size, libp2p_read_callback_t callback, void* user_data);
libp2p_error_t libp2p_stream_write(libp2p_stream_t* stream, const uint8_t* data, size_t size, libp2p_write_callback_t callback, void* user_data);
libp2p_error_t libp2p_stream_close(libp2p_stream_t* stream);
bool libp2p_stream_is_closed(libp2p_stream_t* stream);

// Utility functions
libp2p_multiaddr_t* libp2p_multiaddr_create(const char* addr_str);
void libp2p_multiaddr_destroy(libp2p_multiaddr_t* addr);
const char* libp2p_multiaddr_to_string(libp2p_multiaddr_t* addr);

libp2p_peer_id_t* libp2p_peer_id_create(const char* base58_str);
void libp2p_peer_id_destroy(libp2p_peer_id_t* peer_id);
const char* libp2p_peer_id_to_string(libp2p_peer_id_t* peer_id);

// Error handling
const char* libp2p_error_string(libp2p_error_t error);

// Logging
typedef enum {
    LIBP2P_LOG_TRACE = 0,
    LIBP2P_LOG_DEBUG = 1,
    LIBP2P_LOG_INFO = 2,
    LIBP2P_LOG_WARN = 3,
    LIBP2P_LOG_ERROR = 4,
    LIBP2P_LOG_CRITICAL = 5
} libp2p_log_level_t;

void libp2p_set_log_level(libp2p_log_level_t level);

#ifdef __cplusplus
}
#endif

#endif // LIBP2P_C_API_H
