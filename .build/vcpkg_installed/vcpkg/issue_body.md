Package: boringssl:x64-linux@2024-09-13#1

**Host Environment**

- Host: x64-linux
- Compiler: GNU 13.3.0
- CMake Version: 3.31.10
-    vcpkg-tool version: 2025-12-16-44bb3ce006467fc13ba37ca099f64077b8bbf84d
    vcpkg-scripts version: 25b458671a 2026-01-10 (11 days ago)

**To Reproduce**

`vcpkg install `

**Failure logs**

```
CMake Error at scripts/cmake/vcpkg_find_acquire_program.cmake:201 (message):
  Could not find nasm.  Please install it via your package manager:

      sudo apt-get install nasm
Call Stack (most recent call first):
  /home/runner/.cache/vcpkg/registries/git-trees/78e32f29395487348c0dbbc78828b71b020a92b1/portfile.cmake:9 (vcpkg_find_acquire_program)
  scripts/ports.cmake:206 (include)



```

**Additional context**

<details><summary>vcpkg.json</summary>

```
{
  "name": "leanp2p",
  "version": "0.1.0",
  "dependencies": [
    "boost-asio",
    "boost-beast",
    "boost-di",
    "boost-filesystem",
    "boost-program-options",
    "boost-random",
    "boost-signals2",
    "fmt",
    "liblsquic",
    "libsecp256k1",
    "protobuf",
    "qtils",
    "soralog",
    "tsl-hat-trie",
    "zlib"
  ]
}

```
</details>
