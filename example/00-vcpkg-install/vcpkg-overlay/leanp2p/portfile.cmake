vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO qdrvm/leanp2p
  REF ad6b90667d1e0c9f46fb521bd50412ebe1208ad3
  SHA512 2d6781cabbc3aa688a07e6ad333e1d3fd11c32e02072a91d1f2188d6826238ad8bfed18b9107ae843d9f78bebc2d8e907f2b5d5f8f7db8d4a1726dc8e4693ebf
)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
