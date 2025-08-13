/**
 * LibP2P Echo Client Example in C
 * Connects to an echo server and sends a message, printing the echoed reply.
 * Usage:
 *   libp2p_echo_client_c \
 *     "/ip4/127.0.0.1/udp/12345/quic-v1/p2p/<peer-id>" [message]
 */

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libp2p/c/libp2p_c.h>

// Global context for signal handling
static libp2p_context_t *g_context = NULL;

// Simple state for the request/response
typedef struct {
  char message[1024];
  uint8_t read_buffer[1024];
} client_state_t;

static void on_read_complete(libp2p_stream_t *stream,
                             const uint8_t *data,
                             size_t size,
                             libp2p_error_t error,
                             void *user_data);

static void on_write_complete(libp2p_stream_t *stream,
                              size_t bytes_written,
                              libp2p_error_t error,
                              void *user_data) {
  client_state_t *state = (client_state_t *)user_data;
  if (error != LIBP2P_SUCCESS) {
    printf("Write failed: %s\n", libp2p_error_string(error));
    libp2p_stream_close(stream);
    libp2p_context_stop(g_context);
    free(state);
    return;
  }

  printf("Sent %zu bytes, waiting for echo...\n", bytes_written);
  libp2p_error_t r = libp2p_stream_read(stream,
                                        state->read_buffer,
                                        sizeof(state->read_buffer),
                                        on_read_complete,
                                        state);
  if (r != LIBP2P_SUCCESS) {
    printf("Failed to initiate read: %s\n", libp2p_error_string(r));
    libp2p_stream_close(stream);
    libp2p_context_stop(g_context);
    free(state);
    return;
  }
}

static void on_read_complete(libp2p_stream_t *stream,
                             const uint8_t *data,
                             size_t size,
                             libp2p_error_t error,
                             void *user_data) {
  client_state_t *state = (client_state_t *)user_data;
  if (error != LIBP2P_SUCCESS) {
    if (error != LIBP2P_ERROR_IO_ERROR) {
      printf("Read failed: %s\n", libp2p_error_string(error));
    }
    libp2p_stream_close(stream);
    libp2p_context_stop(g_context);
    free(state);
    return;
  }

  printf("Received %zu bytes: ", size);
  if (size > 0) {
    size_t print_size =
        size < sizeof(state->read_buffer) ? size : sizeof(state->read_buffer);
    fwrite(data, 1, print_size, stdout);
  }
  printf("\n");

  libp2p_stream_close(stream);
  libp2p_context_stop(g_context);
  free(state);
}

static void on_stream_open(libp2p_stream_t *stream, void *user_data) {
  if (!stream) {
    printf("Failed to open stream to peer\n");
    libp2p_context_stop(g_context);
    free(user_data);
    return;
  }

  client_state_t *state = (client_state_t *)user_data;
  const uint8_t *bytes = (const uint8_t *)state->message;
  size_t len = strlen(state->message);

  printf("Opened echo stream, sending message: %s\n", state->message);
  libp2p_error_t w =
      libp2p_stream_write(stream, bytes, len, on_write_complete, state);
  if (w != LIBP2P_SUCCESS) {
    printf("Failed to initiate write: %s\n", libp2p_error_string(w));
    libp2p_stream_close(stream);
    libp2p_context_stop(g_context);
    free(state);
  }
}

static void on_connected(libp2p_host_t *host,
                         const char *peer_id,
                         void *user_data) {
  (void)peer_id;  // info only
  printf("Connected callback fired, opening echo stream...\n");
  libp2p_error_t r = libp2p_host_new_stream(
      host, peer_id, "/echo/1.0.0", on_stream_open, user_data);
  if (r != LIBP2P_SUCCESS) {
    printf("Failed to request new stream: %s\n", libp2p_error_string(r));
    libp2p_context_stop(g_context);
    free(user_data);
  }
}

static void signal_handler(int sig) {
  (void)sig;
  printf("\nStopping...\n");
  if (g_context) {
    libp2p_context_stop(g_context);
  }
}

// Extract base multiaddress and peer id from a string like
//   "/ip4/127.0.0.1/udp/12345/quic-v1/p2p/<peerid>"
// Returns 0 on success, -1 on failure.
static int split_multiaddr_peer(const char *full,
                                char *addr_out,
                                size_t addr_out_sz,
                                char *peer_out,
                                size_t peer_out_sz) {
  const char *tag = "/p2p/";
  const char *p = strstr(full, tag);
  if (!p) {
    return -1;
  }
  size_t addr_len = (size_t)(p - full);
  size_t peer_len = strlen(p + strlen(tag));
  if (addr_len + 1 > addr_out_sz) {
    return -1;
  }
  if (peer_len + 1 > peer_out_sz) {
    return -1;
  }
  memcpy(addr_out, full, addr_len);
  addr_out[addr_len] = '\0';
  memcpy(peer_out, p + strlen(tag), peer_len + 1);
  return 0;
}

int main(int argc, char *argv[]) {
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  libp2p_set_log_level(LIBP2P_LOG_INFO);

  printf("LibP2P Echo Client (C API)\n");
  printf("==========================\n");

  if (argc < 2) {
    printf(
        "Usage: %s \"/ip4/127.0.0.1/udp/<port>/quic-v1/p2p/<peer-id>\" "
        "[message]\n",
        argv[0]);
    return 2;
  }

  const char *connect_full = argv[1];
  const char *message_arg = (argc >= 3) ? argv[2] : "Hello from C";

  // Prepare message state
  client_state_t *state = (client_state_t *)malloc(sizeof(client_state_t));
  if (!state) {
    printf("Out of memory\n");
    return 1;
  }
  snprintf(state->message, sizeof(state->message), "%s", message_arg);

  // Create context and host
  g_context = libp2p_context_create();
  if (!g_context) {
    printf("Failed to create context\n");
    free(state);
    return 1;
  }

  const char *keypair_seed = "echo_client_seed_0";
  libp2p_host_t *host = libp2p_host_create(g_context, keypair_seed);
  if (!host) {
    printf("Failed to create host\n");
    libp2p_context_destroy(g_context);
    free(state);
    return 1;
  }

  if (libp2p_host_start(host) != LIBP2P_SUCCESS) {
    printf("Failed to start host\n");
    libp2p_host_destroy(host);
    libp2p_context_destroy(g_context);
    free(state);
    return 1;
  }

  char base_addr[512];
  char peer_id[256];
  if (split_multiaddr_peer(
          connect_full, base_addr, sizeof(base_addr), peer_id, sizeof(peer_id))
      != 0) {
    printf("Invalid connect address, expected .../p2p/<peer-id>\n");
    libp2p_host_destroy(host);
    libp2p_context_destroy(g_context);
    free(state);
    return 2;
  }

  printf("Dialing %s (peer %s) ...\n", base_addr, peer_id);
  libp2p_error_t d =
      libp2p_host_dial(host, base_addr, peer_id, on_connected, state);
  if (d != LIBP2P_SUCCESS) {
    printf("Dial initiation failed: %s\n", libp2p_error_string(d));
    libp2p_host_destroy(host);
    libp2p_context_destroy(g_context);
    free(state);
    return 1;
  }

  // Run event loop until finished
  libp2p_error_t run = libp2p_context_run(g_context);
  if (run != LIBP2P_SUCCESS) {
    printf("Context error: %s\n", libp2p_error_string(run));
  }

  libp2p_host_destroy(host);
  libp2p_context_destroy(g_context);
  g_context = NULL;

  printf("Client finished\n");
  return 0;
}
