vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF ab456dce54f9e10dba6dfe48c944b14c2dfc5f21
  SHA512 7f17f583953ca73e9552f99c8526803855b790b1a89c93d87c5d8be1b23f78f660006ba6955e07db006703a5d4981137b40671f57c149f34b759e7c39bbc145c
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
