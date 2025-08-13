/**
 * LibP2P Echo Server Example in C
 * This example demonstrates how to create a basic echo server using the libp2p
 * C API that listens for incoming connections and echoes back any data it
 * receives
 */

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libp2p/c/libp2p_c.h>

// Global context for signal handling
static libp2p_context_t *g_context = NULL;
static volatile sig_atomic_t should_stop = 0;
static pthread_t signal_thread;

// Signal monitoring thread function
void *signal_monitor_thread(void *arg) {
  sigset_t sigset;
  int sig;

  // Wait for SIGINT or SIGTERM
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  sigaddset(&sigset, SIGTERM);

  // Block until we receive one of these signals
  if (sigwait(&sigset, &sig) == 0) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    should_stop = 1;
    if (g_context) {
      libp2p_context_stop(g_context);
    }
  }

  return NULL;
}

// Echo protocol handler state
typedef struct {
  uint8_t read_buffer[1024];
  size_t bytes_to_echo;
} echo_state_t;

// Forward declarations
void on_write_complete(libp2p_stream_t *stream,
                       size_t bytes_written,
                       libp2p_error_t error,
                       void *user_data);

// Read callback - called when data is read from the stream
void on_read_complete(libp2p_stream_t *stream,
                      const uint8_t *data,
                      size_t size,
                      libp2p_error_t error,
                      void *user_data) {
  echo_state_t *state = (echo_state_t *)user_data;

  if (error != LIBP2P_SUCCESS) {
    if (error != LIBP2P_ERROR_IO_ERROR) {  // Ignore normal stream closure
      printf("Error reading from stream: %s\n", libp2p_error_string(error));
    }
    free(state);
    return;
  }

  if (size == 0) {
    // End of stream
    printf("Stream closed by peer\n");
    free(state);
    return;
  }

  // Log the received message
  if (size < 120) {
    printf("Received message (%zu bytes): %.*s\n", size, (int)size, data);
  } else {
    printf("Received %zu bytes\n", size);
  }

  // Echo the data back
  state->bytes_to_echo = size;
  libp2p_error_t write_result =
      libp2p_stream_write(stream, data, size, on_write_complete, user_data);
  if (write_result != LIBP2P_SUCCESS) {
    printf("Failed to initiate write: %s\n", libp2p_error_string(write_result));
    free(state);
  }
}

// Write callback - called when data has been written to the stream
void on_write_complete(libp2p_stream_t *stream,
                       size_t bytes_written,
                       libp2p_error_t error,
                       void *user_data) {
  echo_state_t *state = (echo_state_t *)user_data;

  if (error != LIBP2P_SUCCESS) {
    printf("Error writing to stream: %s\n", libp2p_error_string(error));
    free(state);
    return;
  }

  // Log the echoed message
  if (state->bytes_to_echo < 120) {
    printf("Echoed message (%zu bytes)\n", bytes_written);
  } else {
    printf("Echoed %zu bytes\n", bytes_written);
  }

  // Continue reading for more data
  libp2p_error_t read_result = libp2p_stream_read(stream,
                                                  state->read_buffer,
                                                  sizeof(state->read_buffer),
                                                  on_read_complete,
                                                  user_data);
  if (read_result != LIBP2P_SUCCESS) {
    printf("Failed to initiate read: %s\n", libp2p_error_string(read_result));
    free(state);
  }
}

// Echo protocol stream handler - called when a new stream is opened with the
// echo protocol
void echo_protocol_handler(libp2p_stream_t *stream, void *user_data) {
  printf("New echo protocol stream opened\n");

  // Allocate state for this stream
  echo_state_t *state = malloc(sizeof(echo_state_t));
  if (!state) {
    printf("Failed to allocate memory for echo state\n");
    libp2p_stream_close(stream);
    return;
  }

  state->bytes_to_echo = 0;

  // Start reading from the stream
  libp2p_error_t result = libp2p_stream_read(stream,
                                             state->read_buffer,
                                             sizeof(state->read_buffer),
                                             on_read_complete,
                                             state);
  if (result != LIBP2P_SUCCESS) {
    printf("Failed to start reading from stream: %s\n",
           libp2p_error_string(result));
    free(state);
    libp2p_stream_close(stream);
  }
}

int main(int argc, char *argv[]) {
  // Set up signal mask for this thread
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  sigaddset(&sigset, SIGTERM);
  pthread_sigmask(SIG_BLOCK, &sigset, NULL);

  // Create the signal monitoring thread
  if (pthread_create(&signal_thread, NULL, signal_monitor_thread, NULL) != 0) {
    perror("pthread_create");
    return 1;
  }

  // Set logging level
  libp2p_set_log_level(LIBP2P_LOG_INFO);

  printf("LibP2P Echo Server (C API)\n");
  printf("==========================\n");

  // Create libp2p context
  g_context = libp2p_context_create();
  if (!g_context) {
    printf("Failed to create libp2p context\n");
    return 1;
  }

  // Create host with a deterministic seed for consistent peer ID
  const char *keypair_seed = "echo_server_seed_0";
  libp2p_host_t *host = libp2p_host_create(g_context, keypair_seed);
  if (!host) {
    printf("Failed to create libp2p host\n");
    libp2p_context_destroy(g_context);
    return 1;
  }

  // Register the echo protocol handler
  libp2p_error_t result = libp2p_host_register_protocol(
      host, "/echo/1.0.0", echo_protocol_handler, NULL);
  if (result != LIBP2P_SUCCESS) {
    printf("Failed to register echo protocol: %s\n",
           libp2p_error_string(result));
    libp2p_host_destroy(host);
    libp2p_context_destroy(g_context);
    return 1;
  }

  // Listen on the specified address
  const char *listen_addr = "/ip4/127.0.0.1/udp/0/quic-v1";
  if (argc >= 2) {
    listen_addr = argv[1];
  }

  result = libp2p_host_listen(host, listen_addr);
  if (result != LIBP2P_SUCCESS) {
    printf("Failed to listen on %s: %s\n",
           listen_addr,
           libp2p_error_string(result));
    libp2p_host_destroy(host);
    libp2p_context_destroy(g_context);
    return 1;
  }

  // Start the host
  result = libp2p_host_start(host);
  if (result != LIBP2P_SUCCESS) {
    printf("Failed to start host: %s\n", libp2p_error_string(result));
    libp2p_host_destroy(host);
    libp2p_context_destroy(g_context);
    return 1;
  }

  printf("Echo server started successfully\n");
  const char *resolved_listen = listen_addr;
  libp2p_peer_id_t *peer_id_obj = libp2p_host_peer_id(host);
  const char *peer_id_str = NULL;
  if (peer_id_obj) {
    peer_id_str = libp2p_peer_id_to_string(peer_id_obj);
    printf("Peer ID: %s\n", peer_id_str);
  }
  const char *default_listen = libp2p_host_default_listen(host);
  if (default_listen) {
    resolved_listen = default_listen;
  }
  printf("Listening on: %s\n", resolved_listen);
  if (peer_id_str) {
    printf("Full multiaddr: %s/p2p/%s\n", resolved_listen, peer_id_str);
  }
  printf("Protocol: /echo/1.0.0\n");
  printf("Press Ctrl+C to stop the server\n");
  printf("\n");

  // Run the event loop - the signal handler should interrupt it
  result = libp2p_context_run(g_context);
  if (result != LIBP2P_SUCCESS) {
    printf("Context run failed: %s\n", libp2p_error_string(result));
  }

  // Cleanup
  printf("Shutting down...\n");
  if (peer_id_obj) {
    libp2p_peer_id_destroy(peer_id_obj);
  }
  libp2p_host_destroy(host);
  libp2p_context_destroy(g_context);

  // Wait for the signal thread to finish
  pthread_join(signal_thread, NULL);

  printf("Echo server stopped\n");
  return 0;
}
