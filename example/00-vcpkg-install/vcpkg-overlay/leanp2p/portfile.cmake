vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF d057937f4757f55a64ee5e9003ae6ac8eef70bb4
  SHA512 d7b4be105d6e517e3cd0934ad152f1122cf9b7b0a2d9688e7e2f6846c1e525dc3473a223c5f67332a5b709e89f578cc67606b4f4f6854bb911451107a08494a5
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
