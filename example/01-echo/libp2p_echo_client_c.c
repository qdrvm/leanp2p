/**
 * LibP2P Echo Client Example in C
 * This example demonstrates how to create a basic echo client using the libp2p C API
 * that connects to an echo server and sends/receives messages
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <libp2p/c/libp2p_c.h>

// Global context for signal handling
static libp2p_context_t* g_context = NULL;

// Client state
typedef struct {
    const char* message;
    size_t message_len;
    uint8_t read_buffer[1024];
    bool message_sent;
    bool response_received;
} client_state_t;

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    if (g_context) {
        libp2p_context_stop(g_context);
    }
}

// Read callback - called when response is read from the server
void on_response_read(libp2p_stream_t* stream, const uint8_t* data, size_t size,
                      libp2p_error_t error, void* user_data) {
    client_state_t* state = (client_state_t*)user_data;

    if (error != LIBP2P_SUCCESS) {
        printf("Error reading response: %s\n", libp2p_error_string(error));
        libp2p_context_stop(g_context);
        return;
    }

    if (size == 0) {
        printf("No response received\n");
        libp2p_context_stop(g_context);
        return;
    }

    // Log the response
    printf("RESPONSE (%zu bytes): %.*s\n", size, (int)size, data);

    state->response_received = true;

    // Close the stream and stop the context
    libp2p_stream_close(stream);
    libp2p_context_stop(g_context);
}

// Write callback - called when message has been sent to the server
void on_message_sent(libp2p_stream_t* stream, size_t bytes_written,
                     libp2p_error_t error, void* user_data) {
    client_state_t* state = (client_state_t*)user_data;

    if (error != LIBP2P_SUCCESS) {
        printf("Error sending message: %s\n", libp2p_error_string(error));
        libp2p_context_stop(g_context);
        return;
    }

    printf("SENT (%zu bytes): %s\n", bytes_written, state->message);
    state->message_sent = true;

    // Now read the response
    libp2p_error_t result = libp2p_stream_read(stream, state->read_buffer,
                                               sizeof(state->read_buffer),
                                               on_response_read, user_data);
    if (result != LIBP2P_SUCCESS) {
        printf("Failed to start reading response: %s\n", libp2p_error_string(result));
        libp2p_context_stop(g_context);
    }
}

// Stream handler - called when stream to server is established
void on_stream_ready(libp2p_stream_t* stream, void* user_data) {
    client_state_t* state = (client_state_t*)user_data;

    printf("Stream established, sending message...\n");

    // Send the message
    libp2p_error_t result = libp2p_stream_write(stream, (const uint8_t*)state->message,
                                                state->message_len, on_message_sent, user_data);
    if (result != LIBP2P_SUCCESS) {
        printf("Failed to send message: %s\n", libp2p_error_string(result));
        libp2p_context_stop(g_context);
    }
}

// Connection handler - called when connection to server is established
void on_connection_ready(libp2p_host_t* host, const char* peer_id, void* user_data) {
    printf("Connected to peer: %s\n", peer_id);

    // Create a new stream with the echo protocol
    libp2p_error_t result = libp2p_host_new_stream(host, peer_id, "/echo/1.0.0",
                                                   on_stream_ready, user_data);
    if (result != LIBP2P_SUCCESS) {
        printf("Failed to create new stream: %s\n", libp2p_error_string(result));
        libp2p_context_stop(g_context);
    }
}

int main(int argc, char* argv[]) {
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Set logging level
    libp2p_set_log_level(LIBP2P_LOG_INFO);

    printf("LibP2P Echo Client (C API)\n");
    printf("==========================\n");

    // Parse command line arguments
    const char* server_addr = "/ip4/127.0.0.1/udp/4001/quic-v1/p2p/12D3KooWPjceQrSwdWXPyLLeABRXmuqt69Rg3sBYbU1Nft9HyQ6X";
    const char* message = "Hello from C client!";

    if (argc >= 2) {
        server_addr = argv[1];
    }
    if (argc >= 3) {
        message = argv[2];
    }

    printf("Server address: %s\n", server_addr);
    printf("Message: %s\n", message);
    printf("\n");

    // Create client state
    client_state_t state = {
        .message = message,
        .message_len = strlen(message),
        .message_sent = false,
        .response_received = false
    };

    // Create libp2p context
    g_context = libp2p_context_create();
    if (!g_context) {
        printf("Failed to create libp2p context\n");
        return 1;
    }

    // Create host with a different seed for the client
    const char* keypair_seed = "echo_client_seed_1";
    libp2p_host_t* host = libp2p_host_create(g_context, keypair_seed);
    if (!host) {
        printf("Failed to create libp2p host\n");
        libp2p_context_destroy(g_context);
        return 1;
    }

    // Start the host
    libp2p_error_t result = libp2p_host_start(host);
    if (result != LIBP2P_SUCCESS) {
        printf("Failed to start host: %s\n", libp2p_error_string(result));
        libp2p_host_destroy(host);
        libp2p_context_destroy(g_context);
        return 1;
    }

    // Extract peer ID from the server address
    // For now, we'll use a placeholder - in a real implementation,
    // you'd parse the multiaddr to extract the peer ID
    const char* peer_id = "12D3KooWPjceQrSwdWXPyLLeABRXmuqt69Rg3sBYbU1Nft9HyQ6X";

    printf("Connecting to server...\n");

    // Dial the server
    result = libp2p_host_dial(host, server_addr, peer_id, on_connection_ready, &state);
    if (result != LIBP2P_SUCCESS) {
        printf("Failed to dial server: %s\n", libp2p_error_string(result));
        libp2p_host_destroy(host);
        libp2p_context_destroy(g_context);
        return 1;
    }

    // Run the event loop
    result = libp2p_context_run(g_context);
    if (result != LIBP2P_SUCCESS) {
        printf("Context run failed: %s\n", libp2p_error_string(result));
    }

    // Check if we successfully completed the echo exchange
    if (state.message_sent && state.response_received) {
        printf("\nEcho exchange completed successfully!\n");
    } else {
        printf("\nEcho exchange failed or incomplete\n");
    }

    // Cleanup
    printf("Shutting down...\n");
    libp2p_host_destroy(host);
    libp2p_context_destroy(g_context);

    printf("Echo client stopped\n");
    return state.message_sent && state.response_received ? 0 : 1;
}
