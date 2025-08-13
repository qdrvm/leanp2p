/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBP2P_C_API_H
#define LIBP2P_C_API_H

/**
 * @file libp2p_c.h
 * @brief C API for libp2p - A modular peer-to-peer networking library
 *
 * This header provides a C interface to the libp2p networking library,
 * enabling peer-to-peer communication with features including:
 * - Multi-transport support (TCP, QUIC, etc.)
 * - Protocol multiplexing
 * - Cryptographic identity and security
 * - Connection management and stream handling
 *
 * @section usage Basic Usage Example
 * @code
 * // Create context and host
 * libp2p_context_t* ctx = libp2p_context_create();
 * libp2p_host_t* host = libp2p_host_create(ctx, NULL);
 *
 * // Start listening
 * libp2p_host_start(host);
 * libp2p_host_listen(host, "/ip4/0.0.0.0/tcp/0/quic-v1");
 *
 * // Register a protocol handler
 * libp2p_host_register_protocol(host, "/echo/1.0.0", echo_handler, NULL);
 *
 * // Run the event loop
 * libp2p_context_run(ctx);
 *
 * // Cleanup
 * libp2p_host_destroy(host);
 * libp2p_context_destroy(ctx);
 * @endcode
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @defgroup opaque_types Opaque Types
 * @brief Forward declarations for internal libp2p structures
 *
 * These types are opaque to the C API user and should only be accessed
 * through the provided API functions.
 * @{
 */

/** @brief Opaque type representing a libp2p host instance */
typedef struct libp2p_host libp2p_host_t;

/** @brief Opaque type representing a communication stream between peers */
typedef struct libp2p_stream libp2p_stream_t;

/** @brief Opaque type representing a multiaddress (network address) */
typedef struct libp2p_multiaddr libp2p_multiaddr_t;

/** @brief Opaque type representing a peer identifier */
typedef struct libp2p_peer_id libp2p_peer_id_t;

/** @brief Opaque type representing the libp2p execution context */
typedef struct libp2p_context libp2p_context_t;

/** @} */ // end of opaque_types group

/**
 * @defgroup error_codes Error Codes
 * @brief Error codes returned by libp2p C API functions
 * @{
 */

/**
 * @brief Error codes for libp2p operations
 *
 * All libp2p functions that can fail return one of these error codes.
 * LIBP2P_SUCCESS (0) indicates successful operation.
 */
typedef enum {
    LIBP2P_SUCCESS = 0,                    /**< Operation completed successfully */
    LIBP2P_ERROR_INVALID_ARGUMENT = -1,    /**< Invalid argument provided */
    LIBP2P_ERROR_OUT_OF_MEMORY = -2,       /**< Memory allocation failed */
    LIBP2P_ERROR_CONNECTION_FAILED = -3,   /**< Network connection failed */
    LIBP2P_ERROR_PROTOCOL_ERROR = -4,      /**< Protocol-level error occurred */
    LIBP2P_ERROR_IO_ERROR = -5,            /**< Input/output error */
    LIBP2P_ERROR_TIMEOUT = -6,             /**< Operation timed out */
    LIBP2P_ERROR_NOT_FOUND = -7,           /**< Requested resource not found */
    LIBP2P_ERROR_ALREADY_EXISTS = -8,      /**< Resource already exists */
    LIBP2P_ERROR_UNKNOWN = -999            /**< Unknown or unspecified error */
} libp2p_error_t;

/** @} */ // end of error_codes group

/**
 * @defgroup callbacks Callback Types
 * @brief Function pointer types for asynchronous operations
 * @{
 */

/**
 * @brief Callback function for handling incoming streams
 *
 * This callback is invoked when a new stream is established for a registered protocol.
 * The callback should handle the stream communication and is responsible for reading
 * from and writing to the stream as needed.
 *
 * @param stream The newly established stream
 * @param user_data User-provided data passed during protocol registration
 *
 * @note The stream remains valid until explicitly closed or the connection is terminated
 */
typedef void (*libp2p_stream_handler_t)(libp2p_stream_t* stream, void* user_data);

/**
 * @brief Callback function for connection establishment events
 *
 * This callback is invoked when a connection attempt completes, either successfully
 * or with an error. The peer_id parameter indicates which peer the connection was
 * established with.
 *
 * @param host The host that initiated the connection
 * @param peer_id Base58-encoded peer ID of the connected peer, or NULL on failure
 * @param user_data User-provided data passed during dial operation
 */
typedef void (*libp2p_connection_handler_t)(libp2p_host_t* host, const char* peer_id, void* user_data);

/**
 * @brief Callback function for asynchronous read operations
 *
 * This callback is invoked when a read operation completes. The data parameter
 * contains the read data on success, or NULL on error.
 *
 * @param stream The stream that was read from
 * @param data Pointer to the read data, or NULL on error
 * @param size Number of bytes read, or 0 on error
 * @param error Error code indicating the result of the operation
 * @param user_data User-provided data passed during read operation
 */
typedef void (*libp2p_read_callback_t)(libp2p_stream_t* stream, const uint8_t* data, size_t size, libp2p_error_t error, void* user_data);

/**
 * @brief Callback function for asynchronous write operations
 *
 * This callback is invoked when a write operation completes. The bytes_written
 * parameter indicates how many bytes were successfully written.
 *
 * @param stream The stream that was written to
 * @param bytes_written Number of bytes successfully written
 * @param error Error code indicating the result of the operation
 * @param user_data User-provided data passed during write operation
 */
typedef void (*libp2p_write_callback_t)(libp2p_stream_t* stream, size_t bytes_written, libp2p_error_t error, void* user_data);

/** @} */ // end of callbacks group

/**
 * @defgroup context_management Context Management
 * @brief Functions for managing the libp2p execution context
 *
 * The context manages the event loop and asynchronous operations for libp2p.
 * Typically, one context is created per application and shared among hosts.
 * @{
 */

/**
 * @brief Create a new libp2p execution context
 *
 * Creates and initializes a new execution context that manages the event loop
 * and asynchronous operations for libp2p. This context should be used to create
 * hosts and must be kept alive for the duration of their use.
 *
 * @return Pointer to the new context, or NULL on failure
 *
 * @note The returned context must be destroyed with libp2p_context_destroy()
 */
libp2p_context_t* libp2p_context_create(void);

/**
 * @brief Destroy a libp2p execution context
 *
 * Destroys the context and cleans up all associated resources. This will
 * stop the event loop if it's running and invalidate any hosts created
 * with this context.
 *
 * @param ctx Context to destroy, or NULL (safe to pass NULL)
 *
 * @warning All hosts created with this context must be destroyed before
 *          calling this function
 */
void libp2p_context_destroy(libp2p_context_t* ctx);

/**
 * @brief Run the event loop for the given context
 *
 * Starts the event loop and blocks until libp2p_context_stop() is called
 * or all work is completed. This function handles all asynchronous operations
 * including network I/O, timers, and callbacks.
 *
 * @param ctx Context to run
 * @return LIBP2P_SUCCESS on normal completion, error code on failure
 *
 * @note This function blocks until the context is stopped
 * @note This function is thread-safe and can be called from any thread
 */
libp2p_error_t libp2p_context_run(libp2p_context_t* ctx);

/**
 * @brief Stop the event loop for the given context
 *
 * Signals the context's event loop to stop, causing libp2p_context_run()
 * to return. This function is thread-safe and can be called from signal
 * handlers or other threads.
 *
 * @param ctx Context to stop, or NULL (safe to pass NULL)
 *
 * @note This function is non-blocking and thread-safe
 */
void libp2p_context_stop(libp2p_context_t* ctx);

/** @} */ // end of context_management group

/**
 * @defgroup host_management Host Management
 * @brief Functions for creating and managing libp2p hosts
 *
 * A host represents a libp2p node with its own identity and network interfaces.
 * Hosts can listen for connections, dial other peers, and handle protocols.
 * @{
 */

/**
 * @brief Create a new libp2p host
 *
 * Creates a new host with a cryptographic identity. The host can be used to
 * listen for connections, dial other peers, and register protocol handlers.
 *
 * @param ctx Execution context to use for this host
 * @param keypair_seed Optional seed for deterministic key generation (can be NULL)
 * @return Pointer to the new host, or NULL on failure
 *
 * @note If keypair_seed is NULL, a random identity will be generated
 * @note The returned host must be destroyed with libp2p_host_destroy()
 */
libp2p_host_t* libp2p_host_create(libp2p_context_t* ctx, const char* keypair_seed);

/**
 * @brief Destroy a libp2p host
 *
 * Destroys the host and cleans up all associated resources including
 * active connections and streams.
 *
 * @param host Host to destroy, or NULL (safe to pass NULL)
 */
void libp2p_host_destroy(libp2p_host_t* host);

/**
 * @brief Start the host's networking services
 *
 * Initializes and starts the host's networking stack. This must be called
 * before the host can accept connections or dial other peers.
 *
 * @param host Host to start
 * @return LIBP2P_SUCCESS on success, error code on failure
 */
libp2p_error_t libp2p_host_start(libp2p_host_t* host);

/**
 * @brief Start listening on a network address
 *
 * Configures the host to accept incoming connections on the specified
 * multiaddress. Multiple addresses can be used by calling this function
 * multiple times.
 *
 * @param host Host to configure
 * @param multiaddr Multiaddress to listen on (e.g., "/ip4/0.0.0.0/tcp/0/quic-v1")
 * @return LIBP2P_SUCCESS on success, error code on failure
 *
 * @note Port 0 can be used to automatically assign an available port
 * @note Common multiaddress formats:
 *       - "/ip4/0.0.0.0/tcp/8080" (TCP)
 *       - "/ip4/0.0.0.0/tcp/0/quic-v1" (QUIC)
 *       - "/ip6/::/tcp/8080" (IPv6 TCP)
 */
libp2p_error_t libp2p_host_listen(libp2p_host_t* host, const char* multiaddr);

/**
 * @brief Register a protocol handler
 *
 * Registers a handler function for a specific protocol. When a peer opens
 * a stream for this protocol, the handler will be called with the new stream.
 *
 * @param host Host to register the protocol on
 * @param protocol_id Unique protocol identifier (e.g., "/echo/1.0.0")
 * @param handler Callback function to handle incoming streams
 * @param user_data Optional user data passed to the handler
 * @return LIBP2P_SUCCESS on success, error code on failure
 *
 * @note Protocol IDs should follow the convention "/protocol-name/version"
 * @note The handler will be called from the context's event loop thread
 */
libp2p_error_t libp2p_host_register_protocol(libp2p_host_t* host, const char* protocol_id, libp2p_stream_handler_t handler, void* user_data);

/** @} */ // end of host_management group

/**
 * @defgroup host_info Host Information
 * @brief Functions for retrieving host information
 * @{
 */

/**
 * @brief Get the host's peer ID
 *
 * Returns the peer ID of the host as a libp2p_peer_id_t object. This ID uniquely
 * identifies the host in the network.
 *
 * @param host Host to query
 * @return Pointer to peer ID object, or NULL on failure
 *
 * @note The returned peer ID must be destroyed with libp2p_peer_id_destroy()
 */
libp2p_peer_id_t* libp2p_host_peer_id(libp2p_host_t* host);

/**
 * @brief Get the host's default listen address
 *
 * Returns one of the multiaddresses the host is listening on.
 * Useful for advertising the host's location to other peers.
 *
 * @param host Host to query
 * @return Multiaddress string, or NULL if not listening
 *
 * @note The returned string is valid until the host is destroyed
 * @note The string should not be modified or freed
 */
const char* libp2p_host_default_listen(libp2p_host_t* host);

/** @} */ // end of host_info group

/**
 * @defgroup connection_management Connection Management
 * @brief Functions for establishing connections and creating streams
 * @{
 */

/**
 * @brief Dial (connect to) another peer
 *
 * Initiates a connection to the specified peer at the given address.
 * The operation is asynchronous and the callback will be invoked when
 * the connection attempt completes.
 *
 * @param host Host to dial from
 * @param multiaddr Multiaddress of the target peer
 * @param peer_id Base58-encoded peer ID of the target peer
 * @param callback Callback invoked when connection completes
 * @param user_data Optional user data passed to the callback
 * @return LIBP2P_SUCCESS if dial was initiated, error code on immediate failure
 *
 * @note The callback will be called even if the connection fails
 * @note Example: libp2p_host_dial(host, "/ip4/127.0.0.1/tcp/8080/quic-v1", "QmPeerID...", callback, NULL)
 */
libp2p_error_t libp2p_host_dial(libp2p_host_t* host, const char* multiaddr, const char* peer_id, libp2p_connection_handler_t callback, void* user_data);

/**
 * @brief Create a new stream to a peer
 *
 * Opens a new stream to the specified peer for the given protocol.
 * The peer must already be connected (via libp2p_host_dial) and must
 * support the requested protocol.
 *
 * @param host Host to create the stream from
 * @param peer_id Base58-encoded peer ID of the target peer
 * @param protocol_id Protocol identifier for the stream
 * @param callback Callback invoked when stream creation completes
 * @param user_data Optional user data passed to the callback
 * @return LIBP2P_SUCCESS if stream creation was initiated, error code on immediate failure
 *
 * @note The callback will receive the new stream on success, or NULL on failure
 * @note The peer must be connected before calling this function
 */
libp2p_error_t libp2p_host_new_stream(libp2p_host_t* host, const char* peer_id, const char* protocol_id, libp2p_stream_handler_t callback, void* user_data);

/** @} */ // end of connection_management group

/**
 * @defgroup stream_operations Stream Operations
 * @brief Functions for reading from and writing to streams
 * @{
 */

/**
 * @brief Read data from a stream asynchronously
 *
 * Initiates an asynchronous read operation on the stream. The callback
 * will be invoked when data is available or an error occurs.
 *
 * @param stream Stream to read from
 * @param buffer Buffer to store the read data
 * @param buffer_size Maximum number of bytes to read
 * @param callback Callback invoked when read completes
 * @param user_data Optional user data passed to the callback
 * @return LIBP2P_SUCCESS if read was initiated, error code on immediate failure
 *
 * @note The buffer must remain valid until the callback is invoked
 * @note The callback may be called with fewer bytes than requested
 */
libp2p_error_t libp2p_stream_read(libp2p_stream_t* stream, uint8_t* buffer, size_t buffer_size, libp2p_read_callback_t callback, void* user_data);

/**
 * @brief Write data to a stream asynchronously
 *
 * Initiates an asynchronous write operation on the stream. The callback
 * will be invoked when the data has been written or an error occurs.
 *
 * @param stream Stream to write to
 * @param data Data to write
 * @param size Number of bytes to write
 * @param callback Callback invoked when write completes
 * @param user_data Optional user data passed to the callback
 * @return LIBP2P_SUCCESS if write was initiated, error code on immediate failure
 *
 * @note The data buffer must remain valid until the callback is invoked
 * @note The callback indicates how many bytes were actually written
 */
libp2p_error_t libp2p_stream_write(libp2p_stream_t* stream, const uint8_t* data, size_t size, libp2p_write_callback_t callback, void* user_data);

/**
 * @brief Close a stream
 *
 * Closes the stream and releases associated resources. After calling this
 * function, the stream should not be used for further operations.
 *
 * @param stream Stream to close
 * @return LIBP2P_SUCCESS on success, error code on failure
 *
 * @note This operation is synchronous
 * @note The stream pointer becomes invalid after this call
 */
libp2p_error_t libp2p_stream_close(libp2p_stream_t* stream);

/**
 * @brief Check if a stream is closed
 *
 * Determines whether the stream has been closed or is still active.
 *
 * @param stream Stream to check
 * @return true if the stream is closed, false if it's still active
 *
 * @note Returns true if the stream pointer is NULL
 */
bool libp2p_stream_is_closed(libp2p_stream_t* stream);

/** @} */ // end of stream_operations group

/**
 * @defgroup utility_functions Utility Functions
 * @brief Helper functions for working with libp2p types
 * @{
 */

/**
 * @brief Create a multiaddress from a string
 *
 * Parses a multiaddress string and creates a multiaddress object.
 *
 * @param addr_str String representation of the multiaddress
 * @return Pointer to the new multiaddress, or NULL on parse error
 *
 * @note The returned multiaddress must be destroyed with libp2p_multiaddr_destroy()
 * @note Example formats: "/ip4/127.0.0.1/tcp/8080", "/ip6/::1/tcp/9090/quic-v1"
 */
libp2p_multiaddr_t* libp2p_multiaddr_create(const char* addr_str);

/**
 * @brief Destroy a multiaddress
 *
 * Destroys the multiaddress and frees associated memory.
 *
 * @param addr Multiaddress to destroy, or NULL (safe to pass NULL)
 */
void libp2p_multiaddr_destroy(libp2p_multiaddr_t* addr);

/**
 * @brief Convert a multiaddress to string representation
 *
 * Returns the string representation of the multiaddress.
 *
 * @param addr Multiaddress to convert
 * @return String representation, or NULL on error
 *
 * @note The returned string is valid until the multiaddress is destroyed
 * @note The string should not be modified or freed
 */
const char* libp2p_multiaddr_to_string(libp2p_multiaddr_t* addr);

/**
 * @brief Create a peer ID from a Base58 string
 *
 * Parses a Base58-encoded peer ID string and creates a peer ID object.
 *
 * @param base58_str Base58-encoded peer ID string
 * @return Pointer to the new peer ID, or NULL on parse error
 *
 * @note The returned peer ID must be destroyed with libp2p_peer_id_destroy()
 */
libp2p_peer_id_t* libp2p_peer_id_create(const char* base58_str);

/**
 * @brief Destroy a peer ID
 *
 * Destroys the peer ID and frees associated memory.
 *
 * @param peer_id Peer ID to destroy, or NULL (safe to pass NULL)
 */
void libp2p_peer_id_destroy(libp2p_peer_id_t* peer_id);

/**
 * @brief Convert a peer ID to Base58 string representation
 *
 * Returns the Base58 string representation of the peer ID.
 *
 * @param peer_id Peer ID to convert
 * @return Base58 string representation, or NULL on error
 *
 * @note The returned string is valid until the peer ID is destroyed
 * @note The string should not be modified or freed
 */
const char* libp2p_peer_id_to_string(libp2p_peer_id_t* peer_id);

/** @} */ // end of utility_functions group

/**
 * @defgroup error_handling Error Handling
 * @brief Functions for working with error codes
 * @{
 */

/**
 * @brief Convert an error code to a human-readable string
 *
 * Returns a descriptive string for the given error code.
 *
 * @param error Error code to convert
 * @return Human-readable error description
 *
 * @note The returned string is statically allocated and should not be freed
 */
const char* libp2p_error_string(libp2p_error_t error);

/** @} */ // end of error_handling group

/**
 * @defgroup logging Logging
 * @brief Functions for controlling library logging
 * @{
 */

/**
 * @brief Log level enumeration
 *
 * Controls the verbosity of libp2p's internal logging.
 */
typedef enum {
    LIBP2P_LOG_TRACE = 0,      /**< Most verbose - trace execution flow */
    LIBP2P_LOG_DEBUG = 1,      /**< Debug information */
    LIBP2P_LOG_INFO = 2,       /**< General information */
    LIBP2P_LOG_WARN = 3,       /**< Warning messages */
    LIBP2P_LOG_ERROR = 4,      /**< Error messages */
    LIBP2P_LOG_CRITICAL = 5    /**< Critical errors only */
} libp2p_log_level_t;

/**
 * @brief Set the global log level
 *
 * Controls the verbosity of libp2p's internal logging. Only messages
 * at or above the specified level will be output.
 *
 * @param level Minimum log level to output
 *
 * @note This affects all libp2p instances in the process
 * @note Default level is typically LIBP2P_LOG_INFO
 */
void libp2p_set_log_level(libp2p_log_level_t level);

/** @} */ // end of logging group

#ifdef __cplusplus
}
#endif

#endif // LIBP2P_C_API_H
