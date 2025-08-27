# Gossip chat example

A minimal chat over libp2p Gossipsub. It spins up a libp2p host with QUIC transport, subscribes to topic "example", prints received messages, and publishes any line you type in the terminal.

## Build

From the repository root:

```bash
# Configure and build (Release)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The binary will be at:
- build/example/03-gossip/gossip_chat_example

## Run

Open two terminals and run:

Terminal A:
```bash
./build/example/03-gossip/gossip_chat_example 1 2
```

Terminal B:
```bash
./build/example/03-gossip/gossip_chat_example 2 1
```

Type a line and press Enter to broadcast it to subscribers of topic "example". Each process logs messages received from the network; your own messages are not echoed back.

You can connect more peers by passing additional indices (3, 4, …); each index maps to a deterministic key and listen address.

## Notes

- Transport: QUIC (UDP). Ensure your platform supports it.
- Logging: minimal logging is enabled; set env vars used by your logger if you need more detail.
- Toolchain: the example uses modern C++ features (e.g., coroutines). Build with a recent compiler and standard library.

## Troubleshooting

- Build errors about coroutines or <print>: ensure your compiler and C++ standard library are recent enough.
- Port already in use: if a previous run didn’t exit cleanly, wait a few seconds or use a different index.
- Firewalls: QUIC uses UDP; allow inbound/outbound UDP on the used ports for peers to connect.

