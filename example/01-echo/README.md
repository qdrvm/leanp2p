# Echo example (C++ / C / Zig)

This example demonstrates a minimal libp2p Echo protocol using a BasicHost and QUIC transport (lsquic). The server listens and echoes any bytes back; the client connects and verifies a round-trip message. In addition to C++, the same flow is provided via the C wrapper API and Zig using that C API.

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

## C (via C wrapper)

Sources:
- `example/01-echo/libp2p_echo_server_c.c`
- `example/01-echo/libp2p_echo_client_c.c`

First, build the project with CMake so the C wrapper library is produced:

```sh
cmake --preset default
cmake --build build -j
```

Then build the C examples with your system compiler, linking to the wrapper lib produced at `build/src/c`:

```sh
# Server
clang -std=c17 -O2 -Iinclude \
	example/01-echo/libp2p_echo_server_c.c \
	-L./build/src/c -lp2p_c -pthread \
	-o build/example/01-echo/libp2p_echo_server_c

# Client
clang -std=c17 -O2 -Iinclude \
	example/01-echo/libp2p_echo_client_c.c \
	-L./build/src/c -lp2p_c -pthread \
	-o build/example/01-echo/libp2p_echo_client_c
```

Run:

```sh
# Terminal 1
DYLD_LIBRARY_PATH=./build/src/c:$DYLD_LIBRARY_PATH \
	./build/example/01-echo/libp2p_echo_server_c

# Terminal 2 (use the server’s printed full multiaddr ending with /p2p/<peer-id>)
DYLD_LIBRARY_PATH=./build/src/c:$DYLD_LIBRARY_PATH \
	./build/example/01-echo/libp2p_echo_client_c \
	"/ip4/127.0.0.1/udp/<port>/quic-v1/p2p/<peer-id>" "Hello from C"
```

Linux users: replace `DYLD_LIBRARY_PATH` with `LD_LIBRARY_PATH`.

## Zig (via C wrapper)

Sources:
- `example/01-echo/libp2p_echo_server.zig`
- `example/01-echo/libp2p_echo_client.zig`

Build (after the CMake build so `libp2p_c` exists):

```sh
# Server
zig build-exe example/01-echo/libp2p_echo_server.zig \
	-Iinclude -L./build/src/c -lp2p_c \
	-O ReleaseSafe -femit-bin=build/example/01-echo/libp2p_echo_server_zig

# Client
zig build-exe example/01-echo/libp2p_echo_client.zig \
	-Iinclude -L./build/src/c -lp2p_c \
	-O ReleaseSafe -femit-bin=build/example/01-echo/libp2p_echo_client_zig
```

Run:

```sh
# Terminal 1
DYLD_LIBRARY_PATH=./build/src/c:$DYLD_LIBRARY_PATH \
	./build/example/01-echo/libp2p_echo_server_zig

# Terminal 2
DYLD_LIBRARY_PATH=./build/src/c:$DYLD_LIBRARY_PATH \
	./build/example/01-echo/libp2p_echo_client_zig \
	"/ip4/127.0.0.1/udp/<port>/quic-v1/p2p/<peer-id>" "Hello from Zig"
```

You can mix and match (e.g., run the C++ server and connect with the Zig or C client).

### Runtime note

If you see dynamic linker errors, ensure the directory containing the wrapper library is on your runtime path:
- macOS: `export DYLD_LIBRARY_PATH=./build/src/c:$DYLD_LIBRARY_PATH`
- Linux: `export LD_LIBRARY_PATH=./build/src/c:$LD_LIBRARY_PATH`
