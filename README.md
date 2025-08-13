# leanp2p

A lean, modern C++23 take on core libp2p building blocks with a small, focused example. It uses Boost.Asio coroutines, DI via Boost.DI, QUIC transport via lsquic, logging via soralog, and crypto (secp256k1, OpenSSL), all managed through vcpkg.

## Features

- C++23, header-first public API in `include/libp2p/**`
- Basic host, protocol plumbing, and an Echo protocol example
- QUIC transport (lsquic) + Boost.Asio
- Dependency Injection with Boost.DI
- Logging with soralog
- Packaged with vcpkg manifests and CMake Presets

## Prerequisites

- macOS or Linux with a C++23 compiler (AppleClang 15+/Clang 16+/GCC 12+)
- CMake 3.25+ and Ninja
- Git
- vcpkg (see next section for install and setup)

Tip (macOS): install tools via Homebrew: `brew install cmake ninja git`

## Install vcpkg (one-time)

This project uses vcpkg in manifest mode via the CMake toolchain. You don’t need to "integrate" globally, but you must have vcpkg cloned and bootstrapped, and `VCPKG_ROOT` set.

1) Clone and bootstrap

```sh
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg"/bootstrap-vcpkg.sh
```

2) Set VCPKG_ROOT (zsh)

Add to your shell profile so it’s available in every terminal:

```sh
echo 'export VCPKG_ROOT="$HOME/vcpkg"' >> ~/.zprofile
# if you use ~/.zshrc instead, write there and reload the shell
export VCPKG_ROOT="$HOME/vcpkg"
```

3) Verify

```sh
"$VCPKG_ROOT"/vcpkg version
```

## Build

This project is CMake + vcpkg manifest-based. The provided preset uses Ninja and the vcpkg toolchain.

First configure (will auto-install deps on first run):

```sh
cmake --preset default
```

Then build:

```sh
cmake --build build -j
```

Notes
- The preset builds Debug by default (see `CMakePresets.json`).
- To build Release without creating a new preset:
  ```sh
  cmake -S . -B build-release -G Ninja \
        -D CMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
        -D CMAKE_BUILD_TYPE=Release
  cmake --build build-release -j
  ```

## Echo example

See `example/01-echo/README.md` for usage and run instructions.

## Project layout

- `include/libp2p/**` — public headers
- `src/**` — library implementation
- `example/01-echo/**` — Echo server and client
- `vcpkg.json`, `vcpkg-configuration.json` — dependencies and registries
- `cmake/**` — helper CMake scripts

## Troubleshooting

- vcpkg not found: ensure `VCPKG_ROOT` is set and points to your vcpkg clone (and that your shell exports it).
- First configure is slow: vcpkg will build/install dependencies; subsequent builds are fast.
- Linking errors on macOS: ensure you’re using the same toolchain/SDK across configure and build, and that you didn’t mix Xcode and Ninja generators.

## License

Apache-2.0. See file headers and repository for details.
