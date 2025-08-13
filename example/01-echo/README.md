# Echo example (C++)

This example demonstrates a minimal libp2p Echo protocol using a BasicHost and QUIC transport (lsquic). The server listens and echoes any bytes back; the client connects and verifies a round-trip message.

## Binaries

Built targets (under your CMake build directory):

- `build/example/01-echo/libp2p_echo_server`
- `build/example/01-echo/libp2p_echo_client`

Build the project from the repository root as described in the top-level README.

## Run

1) Start the server (prints listen address, peer id, and a connection string):

```sh
./build/example/01-echo/libp2p_echo_server
```

Look for a line like:

```
Connection string: /ip4/127.0.0.1/udp/<port>/quic-v1/p2p/<peer-id>
```

2) In another terminal, run the client. You can pass the connection string printed by the server:

```sh
./build/example/01-echo/libp2p_echo_client "/ip4/127.0.0.1/udp/<port>/quic-v1/p2p/<peer-id>"
```

If you omit the argument, the client uses a built-in sample connection info suitable for the sample server with defaults.

Expected logs include lines like:

- client: `SENDING Hello from C++` then `RESPONSE Hello from C++`
- server: `Listening on: ...`, `Peer id: ...`, `Connection string: ...`
